/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_convenience.h"
#include "jpl_frames.h"
#include "jpl_private.h"
#include "erfa.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HORIZONS_ROWS = 220, HORIZONS_DATES_PER_BODY = 22 };

typedef struct horizons_sample {
    int body, tested;
    double tt[2], reference_lon_deg, computed_lon_rate;
} horizons_sample;

typedef struct horizons_residuals {
    double modern_angle, modern_legacy_angle, modern_radial;
    double distant_angle, distant_legacy_angle, distant_range, distant_radial;
} horizons_residuals;

static int parse_tt_jd(const char *text, double tt[2]) {
    char *fraction, *end;
    long long whole;
    double part = 0.0;
    size_t i;
    if (!text || !*text || !tt) return 1;
    for (i = text[0] == '-' ? 1u : 0u; text[i] != '\0'; ++i) {
        if (text[i] != '.' && (text[i] < '0' || text[i] > '9')) return 1;
    }
    errno = 0;
    whole = strtoll(text, &fraction, 10);
    if (errno != 0 || fraction == text) return 1;
    if (*fraction == '.') {
        if (fraction[1] == '\0') return 1;
        part = strtod(fraction, &end);
        if (*end != '\0' || !isfinite(part)) return 1;
    } else if (*fraction != '\0') {
        return 1;
    }
    tt[0] = (double)whole;
    tt[1] = text[0] == '-' ? -part : part;
    return 0;
}

static int horizons_target_for(int body) {
    if (body == JPL_MOON) return 301;
    if (body == JPL_SUN) return 10;
    if (body >= JPL_MERCURY && body <= JPL_PLUTO && body != JPL_EARTH)
        return body;
    return -1;
}

static double direction_separation(const double a[3], const double b[3]) {
    double x = a[1] * b[2] - a[2] * b[1];
    double y = a[2] * b[0] - a[0] * b[2];
    double z = a[0] * b[1] - a[1] * b[0];
    double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    if (dot > 1.0) dot = 1.0;
    if (dot < -1.0) dot = -1.0;
    return atan2(hypot(hypot(x, y), z), dot);
}

static int compare_row(const jpl_ephemeris *e, int body, const double tt[2],
                       double ra, double dec, double old_lon, double old_lat,
                       double range, double deldot_km_s, double *reference_lon,
                       double *computed_lon_rate, horizons_residuals *residuals) {
    double ra_rad = ra * (acos(-1.0) / 180.0);
    double dec_rad = dec * (acos(-1.0) / 180.0);
    double icrs[3], modern[3], legacy[3], got_vector[3];
    double matrix[3][3], dpsi, deps, sep, sep_legacy, lon_ref, lat_ref;
    double old_vector[3], old_lon_rad = old_lon * (acos(-1.0) / 180.0);
    double old_lat_rad = old_lat * (acos(-1.0) / 180.0);
    jpl_apparent_ecliptic_result got;
    int modern_epoch;
    eraS2c(ra_rad, dec_rad, icrs);
    eraPnm06a(tt[0], tt[1], matrix);
    eraNut06a(tt[0], tt[1], &dpsi, &deps);
    eraRx(eraObl06(tt[0], tt[1]) + deps, matrix);
    eraRxp(matrix, icrs, modern);
    eraPnm80(tt[0], tt[1], matrix);
    eraNut80(tt[0], tt[1], &dpsi, &deps);
    eraRx(eraObl80(tt[0], tt[1]) + deps, matrix);
    eraRxp(matrix, icrs, legacy);
    eraS2c(old_lon_rad, old_lat_rad, old_vector);
    sep_legacy = direction_separation(legacy, old_vector);
    if (!isfinite(sep_legacy)) return 1;
    eraC2s(modern, &lon_ref, &lat_ref);
    *reference_lon = eraAnp(lon_ref) * (180.0 / acos(-1.0));
    if (!isfinite(*reference_lon) || !isfinite(lat_ref)) return 1;

    {
        jpl_date tt_date = {tt[0], tt[1]};
        if (jpl_apparent_ecliptic_position(e, &tt_date, body, &got) != JPL_OK ||
            !isfinite(got.longitude_deg) || !isfinite(got.latitude_deg) ||
            !isfinite(got.range_au) || !(got.range_au > 0.0) ||
            !isfinite(got.longitude_rate_deg_per_day) ||
            !isfinite(got.range_rate_km_per_s)) return 1;
    }
    eraS2c(got.longitude_deg * (acos(-1.0) / 180.0),
           got.latitude_deg * (acos(-1.0) / 180.0), got_vector);
    sep = direction_separation(modern, got_vector);
    if (!isfinite(sep)) return 1;
    *computed_lon_rate = got.longitude_rate_deg_per_day;
    modern_epoch = tt[0] >= 2433282.5 && tt[0] <= 2469807.5;
    if (modern_epoch) {
        double radial_diff = fabs(got.range_rate_km_per_s - deldot_km_s);
        if (radial_diff > residuals->modern_radial) residuals->modern_radial = radial_diff;
        if (sep > residuals->modern_angle) residuals->modern_angle = sep;
        if (sep_legacy > residuals->modern_legacy_angle)
            residuals->modern_legacy_angle = sep_legacy;
        if (sep > 1e-6 || fabs(got.range_au - range) > 1e-7 || radial_diff > 1e-4) {
            fprintf(stderr, "Horizons body %d TT %.9f: %.5f arcsec, %.10g AU range, %.10g km/s radial\n",
                    body, tt[0] + tt[1], sep * (180.0 / acos(-1.0)) * 3600.0,
                    got.range_au - range, got.range_rate_km_per_s - deldot_km_s);
            return 1;
        }
        if (sep_legacy > 1e-6) {
            fprintf(stderr, "Horizons legacy ecliptic body %d TT %.9f: %.5f arcsec\n",
                    body, tt[0] + tt[1], sep_legacy * (180.0 / acos(-1.0)) * 3600.0);
            return 1;
        }
    } else {
        double range_diff = fabs(got.range_au - range);
        double radial_diff = fabs(got.range_rate_km_per_s - deldot_km_s);
        if (sep > residuals->distant_angle) residuals->distant_angle = sep;
        if (sep_legacy > residuals->distant_legacy_angle)
            residuals->distant_legacy_angle = sep_legacy;
        if (range_diff > residuals->distant_range) residuals->distant_range = range_diff;
        if (radial_diff > residuals->distant_radial) residuals->distant_radial = radial_diff;
    }
    return 0;
}

int main(int argc, char **argv) {
    FILE *file;
    jpl_ephemeris *e = NULL;
    const jpl_header *header;
    char line[512], seen[HORIZONS_ROWS][96];
    horizons_sample samples[HORIZONS_ROWS];
    int counts[JPL_SUN + 1] = {0};
    int total = 0, selected = 0, failed = 0, full;
    int require_full = 0;
    horizons_residuals residuals = {0};
    double longitude_rate_max = 0.0;
    if ((argc != 3 && argc != 4) || jpl_open(argv[1], &e) != JPL_OK) return 2;
    if (argc == 4) {
        if (strcmp(argv[3], "full") != 0) { jpl_close(e); return 2; }
        require_full = 1;
    }
    header = jpl_private_get_header(e);
    if (!header || header->ephemeris_number != 441) {
        jpl_close(e);
        return 1;
    }
    full = header->start_jd[0] < -1894917.5 &&
           header->end_jd[0] > 5370000.5;
    if (require_full && !full) {
        fprintf(stderr, "DE441 local file does not cover the advertised full span\n");
        jpl_close(e);
        return 1;
    }
    file = fopen(argv[2], "r");
    if (!file) { jpl_close(e); return 2; }
    while (fgets(line, sizeof line, file)) {
        char jd[64], extra;
        int denum, body, horizons_target, count, i;
        double ra, dec, old_lon, old_lat, range, deldot, tt[2];
        double tdb[2];
        jpl_date tt_date, tdb_date;
        char identity[96];
        if (line[0] == '#' || line[0] == '\n') continue;
        count = sscanf(line, " %d %d %d %63s %lf %lf %lf %lf %lf %lf %c",
                       &denum, &body, &horizons_target, jd,
                       &ra, &dec, &old_lon, &old_lat, &range, &deldot, &extra);
        if (count != 10 || denum != 441 ||
            horizons_target != horizons_target_for(body) ||
            parse_tt_jd(jd, tt) != 0 ||
            !isfinite(ra) || !isfinite(dec) || !isfinite(old_lon) ||
            !isfinite(old_lat) || !isfinite(range) || !(range > 0.0) ||
            !isfinite(deldot) || total >= HORIZONS_ROWS) {
            fprintf(stderr, "Invalid Horizons reference row: %.120s\n", line);
            failed = 1; break;
        }
        if (snprintf(identity, sizeof identity, "%d/%s", body, jd) >= (int)sizeof identity) {
            failed = 1; break;
        }
        for (i = 0; i < total; ++i) {
            if (strcmp(identity, seen[i]) == 0) { failed = 1; break; }
        }
        if (failed) break;
        strcpy(seen[total], identity);
        samples[total].body = body;
        samples[total].tt[0] = tt[0];
        samples[total].tt[1] = tt[1];
        samples[total].tested = 0;
        ++total;
        tt_date.hi = tt[0]; tt_date.lo = tt[1];
        if (jpl_tt_to_tdb(&tt_date, &tdb_date) != JPL_OK) { failed = 1; break; }
        jpl_date_to_pair(&tdb_date, tdb);
        if (jpl_date_compare(tdb, header->start_jd) < 0 ||
            jpl_date_compare(tdb, header->end_jd) > 0) continue;
        if (compare_row(e, body, tt, ra, dec, old_lon, old_lat, range, deldot,
                        &samples[total - 1].reference_lon_deg,
                        &samples[total - 1].computed_lon_rate,
                        &residuals) != 0) {
            failed = 1; break;
        }
        samples[total - 1].tested = 1;
        ++counts[body];
        ++selected;
    }
    if (ferror(file)) failed = 1;
    if (fclose(file) != 0) failed = 1;
    if (total != HORIZONS_ROWS) failed = 1;
    for (int body = JPL_MERCURY; body <= JPL_SUN; ++body) {
        if (horizons_target_for(body) < 0) continue;
        if (counts[body] != (full ? HORIZONS_DATES_PER_BODY : 12)) failed = 1;
    }
    if (!failed) {
        static const double centers[4] = {2451545.0, 2453005.0, 2453166.5, 2455197.5};
        for (int body = JPL_MERCURY; body <= JPL_SUN && !failed; ++body) {
            if (horizons_target_for(body) < 0) continue;
            for (size_t c = 0; c < 4; ++c) {
                const horizons_sample *previous = NULL, *at = NULL, *next = NULL;
                for (int i = 0; i < total; ++i) {
                    double delta;
                    if (!samples[i].tested || samples[i].body != body) continue;
                    delta = (samples[i].tt[0] - centers[c]) + samples[i].tt[1];
                    if (fabs(delta) < 1e-9) at = &samples[i];
                    if (fabs(delta + 0.01) < 1e-9) previous = &samples[i];
                    if (fabs(delta - 0.01) < 1e-9) next = &samples[i];
                }
                if (!at || !previous || !next) { failed = 1; break; }
                double expected = remainder(next->reference_lon_deg - previous->reference_lon_deg,
                                            360.0) / 0.02;
                double speed_error = fabs(at->computed_lon_rate - expected);
                if (speed_error > longitude_rate_max) longitude_rate_max = speed_error;
                if (speed_error >= 1e-4) {
                    fprintf(stderr, "Horizons body %d TT %.1f: longitude speed off by %.10g deg/day\n",
                            body, centers[c], speed_error);
                    failed = 1; break;
                }
            }
        }
    }
    if (!failed)
        printf("Horizons: %d/%d rows; modern maxima %.5f arcsec direction, %.5f arcsec legacy, %.9g deg/day longitude, %.9g km/s radial\n",
               selected, total,
               residuals.modern_angle * (180.0 / acos(-1.0)) * 3600.0,
               residuals.modern_legacy_angle * (180.0 / acos(-1.0)) * 3600.0,
               longitude_rate_max, residuals.modern_radial);
    if (!failed)
        printf("Distant diagnostics: %.5f arcsec direction, %.5f arcsec legacy, %.9g AU range, %.9g km/s radial\n",
               residuals.distant_angle * (180.0 / acos(-1.0)) * 3600.0,
               residuals.distant_legacy_angle * (180.0 / acos(-1.0)) * 3600.0,
               residuals.distant_range, residuals.distant_radial);
    jpl_close(e);
    return failed ? 1 : 0;
}
