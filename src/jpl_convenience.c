/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_convenience.h"
#include "jpl_private.h"
#include "jpl_frames.h"
#include "erfa.h"
#include <math.h>
#include <string.h>

/* Only ICRF-aligned files pass the frame check below. Their native
 * equatorial states need no ecliptic-to-ICRS rotation here; core
 * jpl_get_state still reads legacy files in their native frames. */
/* Modern-AU unit conversions leave outputs unchanged on failure.
 * Velocity scaling can overflow and supports exact input/output aliasing;
 * distance division cannot overflow after rejecting nonfinite km. */
jpl_status jpl_au_per_day_to_km_per_s(const double v[3], double o[3]) {
    double tmp[3];
    size_t i;
    const double K = JPL_AU_KM / JPL_DAYSEC;
    if (!v || !o) return JPL_ERROR_ARGUMENT;
    for (i = 0; i < 3; ++i) {
        if (!isfinite(v[i])) return JPL_ERROR_ARGUMENT;
        tmp[i] = v[i] * K;
        if (!isfinite(tmp[i])) return JPL_ERROR_NUMERIC;
    }
    o[0] = tmp[0]; o[1] = tmp[1]; o[2] = tmp[2];
    return JPL_OK;
}
jpl_status jpl_km_to_au(double km, double *au) {
    if (!au) return JPL_ERROR_ARGUMENT;
    if (!isfinite(km)) return JPL_ERROR_ARGUMENT;
    *au = km / JPL_AU_KM;
    return JPL_OK;
}

/* The ordinary body-center path needs these three evaluations in this
 * order. Return the solved delay for callers that also need target geometry
 * at emission; the observer and Sun legs stay at reception. */
static jpl_status observer_geometry(const jpl_ephemeris *e, const jpl_date *tdb,
                                    int target, int center, jpl_state *retarded,
                                    jpl_state *observer, jpl_state *sun_from_observer,
                                    double *light_time_days) {
    jpl_status s = jpl_get_state_lighttime(e, tdb, target, center,
                                            retarded, light_time_days);
    if (s != JPL_OK) return s;
    s = jpl_get_state(e, tdb, center, JPL_SSB, observer);
    if (s != JPL_OK) return s;
    return jpl_get_state(e, tdb, center, JPL_SUN, sun_from_observer);
}

static jpl_status convenience_epoch(const jpl_ephemeris *e, const jpl_date *jd_tt,
                                    jpl_date *tt, jpl_date *tdb) {
    const jpl_header *header;
    jpl_date cov_start, cov_end;
    jpl_status s;
    if (!e || !jd_tt || !tt || !tdb) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd_tt->hi) || !isfinite(jd_tt->lo)) return JPL_ERROR_ARGUMENT;
    s = jpl_date_normalize(jd_tt, tt);
    if (s != JPL_OK) return s;
    s = jpl_tt_to_tdb(tt, tdb);
    if (s != JPL_OK) return s;
    header = jpl_get_header(e);
    if (!header) return JPL_ERROR_ARGUMENT;
    s = jpl_date_normalize(header->start_jd, &cov_start);
    if (s != JPL_OK) return JPL_ERROR_FORMAT;
    s = jpl_date_normalize(header->end_jd, &cov_end);
    if (s != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(tdb, &cov_start) < 0 ||
        jpl_date_compare(tdb, &cov_end) > 0) return JPL_ERROR_RANGE;
    if (header->reference_frame != JPL_FRAME_ICRF) return JPL_ERROR_UNSUPPORTED;
    return JPL_OK;
}

jpl_status jpl_planet_position(const jpl_ephemeris *e, const jpl_date *jd_tt, int planet, int center, jpl_state *st) {
    jpl_date tt, tdb;
    double lt;
    jpl_state tmp, vobs, cst;
    int eff_center;
    jpl_status s;
    if(!e||!st||!jd_tt) return JPL_ERROR_ARGUMENT;
    if(!isfinite(jd_tt->hi)||!isfinite(jd_tt->lo)) return JPL_ERROR_ARGUMENT;
    /* Targets 1-11 (Mercury..Sun, incl. Earth 3) or EMB 13. Target zero
     * is invalid; SSB 12 and the nutation (14) /
     * libration (15) sensitivities plus out-of-range codes are rejected. */
    if(!((planet>=1&&planet<=11)||planet==JPL_EARTH_MOON_BARYCENTER)) return JPL_ERROR_ARGUMENT;
    /* Centers: 0 (SSB) or bodies 1-13. Reject negatives and anything above 13. */
    if(center<0||center>JPL_EARTH_MOON_BARYCENTER) return JPL_ERROR_ARGUMENT;
    /* Coverage precedes frame gating even for identical-body calls. */
    s = convenience_epoch(e, jd_tt, &tt, &tdb);
    if (s != JPL_OK) return s;
    /* Center 0 is the SSB alias; normalize to 12 wherever a
     * target code is required because target 0 is invalid in core getters. */
    eff_center = center==JPL_SSB ? JPL_SOLAR_SYSTEM_BARYCENTER : center;
    /* Identical-body shortcut: validate the required body at
     * reception, then return zero without touching unrelated Sun data. */
    if(planet==center) {
        s = jpl_get_state(e, &tdb, planet, center, &tmp);
        if(s!=JPL_OK) return s;
        st->x = st->y = st->z = st->vx = st->vy = st->vz = 0.0;
        return JPL_OK;
    }
    /* SSB-observer shortcut: the barycentric observer velocity is
     * zero by definition, so no aberration/Sun lookup is needed. */
    if(eff_center==JPL_SOLAR_SYSTEM_BARYCENTER) {
        s = jpl_get_state_lighttime(e, &tdb, planet, center, &tmp, &lt);
        if(s!=JPL_OK) return s;
        *st = tmp;
        return JPL_OK;
    }
    /* Stellar aberration uses the observer's reception velocity; Sun
     * distance supplies its solar-potential term unless the observer
     * itself is the Sun. */
    if (eff_center != JPL_SUN) {
        jpl_state sun_obs;
        double sdist;
        s = observer_geometry(e, &tdb, planet, center, &tmp, &cst, &sun_obs, &lt);
        if (s != JPL_OK) return s;
        vobs.vx = cst.vx; vobs.vy = cst.vy; vobs.vz = cst.vz;
        sdist = jpl_vec_norm3(&sun_obs);
        if (!isfinite(sdist)) return JPL_ERROR_NUMERIC;
        /* Finite nonpositive Sun distance omits the potential term
         * inside the aberration call; nonfinite is ARGUMENT there. */
        s = jpl_apply_aberration(&tmp, &vobs, sdist, &tmp);
        if (s != JPL_OK) return s;
    } else {
        s = jpl_get_state_lighttime(e, &tdb, planet, center, &tmp, &lt);
        if (s != JPL_OK) return s;
        s = jpl_get_state(e, &tdb, eff_center, 0, &cst);
        if (s != JPL_OK) return s;
        vobs.vx = cst.vx; vobs.vy = cst.vy; vobs.vz = cst.vz;
        s = jpl_apply_aberration(&tmp, &vobs, 0.0, &tmp);
        if (s != JPL_OK) return s;
    }
    *st = tmp;
    return JPL_OK;
}

jpl_status jpl_moon_position(const jpl_ephemeris *e, const jpl_date *jd_tt, jpl_state *st) {
    return jpl_planet_position(e, jd_tt, JPL_MOON, JPL_EARTH, st);
}

static jpl_status apparent_position_at_tt(const jpl_ephemeris *e,
                                          const jpl_date *jd_tt, int target,
                                          double position[3], double *radial_km_per_s) {
    jpl_date tt, tdb, emission;
    double light_time;
    jpl_state state, observer, sun_from_observer, sun_to_target;
    jpl_state aberrated;
    double ecliptic[3], vobs[3], unit[3], angular_distance;
    double distance, sun_distance, q_distance, longitude, latitude, norm;
    double projected_radial = 0.0;
    double result[3];
    jpl_status s;
    size_t k;
    if (!e || !jd_tt || !position) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd_tt->hi) || !isfinite(jd_tt->lo)) return JPL_ERROR_ARGUMENT;
    if (target < JPL_MERCURY || target > JPL_SUN || target == JPL_EARTH)
        return JPL_ERROR_ARGUMENT;
    s = convenience_epoch(e, jd_tt, &tt, &tdb);
    if (s != JPL_OK) return s;
    s = observer_geometry(e, &tdb, target, JPL_EARTH, &state, &observer,
                          &sun_from_observer, &light_time);
    if (s != JPL_OK) return s;
    distance = jpl_vec_norm3(&state);
    sun_distance = jpl_vec_norm3(&sun_from_observer);
    if (!isfinite(distance) || !(distance > 0.0) ||
        !isfinite(sun_distance) || !(sun_distance > 0.0)) return JPL_ERROR_NUMERIC;
    if (radial_km_per_s) {
        double au_per_day = (state.x*state.vx + state.y*state.vy +
                              state.z*state.vz) / distance;
        projected_radial = au_per_day * (JPL_AU_KM / JPL_DAYSEC);
        if (!isfinite(projected_radial)) return JPL_ERROR_NUMERIC;
    }
    if (target != JPL_SUN) {
        double p[3], q[3], se[3], bent[3], bent_norm;
        double limb, dlim;
        s = jpl_date_add_days(&tdb, -light_time, &emission);
        if (s != JPL_OK) return s;
        s = jpl_get_state(e, &emission, target, JPL_SUN, &sun_to_target);
        if (s != JPL_OK) return s;
        q_distance = jpl_vec_norm3(&sun_to_target);
        if (!isfinite(q_distance) || !(q_distance > 0.0)) return JPL_ERROR_NUMERIC;
        for (k = 0; k < 3; ++k) {
            p[k] = state.x / distance;
            q[k] = (&sun_to_target.x)[k] / q_distance;
            se[k] = (&sun_from_observer.x)[k] / sun_distance;
        }
        limb = atan2(JPL_SOLAR_RADIUS_KM / JPL_AU_KM, sun_distance);
        dlim = 0.5 * limb * limb;
        if (!isfinite(dlim) || !(dlim > 0.0)) return JPL_ERROR_NUMERIC;
        eraLd(1.0, p, q, se, sun_distance, dlim, bent);
        bent_norm = jpl_vec_norm3(bent);
        if (!isfinite(bent_norm) || !(bent_norm > 0.0)) return JPL_ERROR_NUMERIC;
        for (k = 0; k < 3; ++k) (&state.x)[k] = (bent[k] / bent_norm) * distance;
    }
    vobs[0] = observer.vx; vobs[1] = observer.vy; vobs[2] = observer.vz;
    s = jpl_apply_aberration(&state, vobs, sun_distance, &aberrated);
    if (s != JPL_OK) return s;
    s = jpl_icrs_to_true_ecliptic_position(&aberrated, &tt, ecliptic);
    if (s != JPL_OK) return s;
    norm = jpl_vec_norm3(ecliptic);
    if (!isfinite(norm) || !(norm > 0.0)) return JPL_ERROR_NUMERIC;
    for (k = 0; k < 3; ++k) unit[k] = ecliptic[k] / norm;
    eraP2s(unit, &longitude, &latitude, &angular_distance);
    result[0] = eraAnp(longitude) * (180.0 / acos(-1.0));
    if (result[0] >= 360.0) result[0] = 0.0;
    result[1] = latitude * (180.0 / acos(-1.0));
    result[2] = distance;
    if (!isfinite(result[0]) || !isfinite(result[1]) ||
        !isfinite(result[2])) return JPL_ERROR_NUMERIC;
    memcpy(position, result, sizeof result);
    if (radial_km_per_s) *radial_km_per_s = projected_radial;
    return JPL_OK;
}

static jpl_status apparent_position_offset(const jpl_ephemeris *e,
                                           const jpl_date *jd_tt, int target,
                                           double days, double position[3]) {
    jpl_date sample_tt;
    jpl_status s = jpl_date_add_days(jd_tt, days, &sample_tt);
    if (s != JPL_OK) return s;
    return apparent_position_at_tt(e, &sample_tt, target, position, NULL);
}

static double longitude_delta(double a_deg, double b_deg) {
    return remainder(a_deg - b_deg, 360.0);
}

jpl_status jpl_apparent_ecliptic_position(const jpl_ephemeris *e,
                                           const jpl_date *jd_tt, int target,
                                           jpl_apparent_ecliptic_result *out) {
    double center[3], earlier[3], later[3], farther[3];
    double h = 0.01, longitude_rate, projected_radial;
    jpl_apparent_ecliptic_result local;
    jpl_status s, before, after;
    int attempt;
    if (!out) return JPL_ERROR_ARGUMENT;
    s = apparent_position_at_tt(e, jd_tt, target, center, &projected_radial);
    if (s != JPL_OK) return s;
    for (attempt = 0; attempt <= 12; ++attempt) {
        before = apparent_position_offset(e, jd_tt, target, -h, earlier);
        after = apparent_position_offset(e, jd_tt, target, h, later);
        if (before != JPL_OK && before != JPL_ERROR_RANGE) return before;
        if (after != JPL_OK && after != JPL_ERROR_RANGE) return after;
        if (before != JPL_ERROR_RANGE || after != JPL_ERROR_RANGE) break;
        h *= 0.5;
    }
    if (before == JPL_ERROR_RANGE && after == JPL_ERROR_RANGE) return JPL_ERROR_RANGE;
    if (before == JPL_OK && after == JPL_OK) {
        longitude_rate = longitude_delta(later[0], earlier[0]) / (2.0 * h);
    } else if (after == JPL_OK) {
        s = apparent_position_offset(e, jd_tt, target, 2.0 * h, farther);
        if (s == JPL_OK) {
            longitude_rate = (4.0 * longitude_delta(later[0], center[0]) -
                              longitude_delta(farther[0], center[0])) / (2.0 * h);
        } else if (s == JPL_ERROR_RANGE) {
            longitude_rate = longitude_delta(later[0], center[0]) / h;
        } else {
            return s;
        }
    } else {
        s = apparent_position_offset(e, jd_tt, target, -2.0 * h, farther);
        if (s == JPL_OK) {
            longitude_rate = (-4.0 * longitude_delta(earlier[0], center[0]) +
                              longitude_delta(farther[0], center[0])) / (2.0 * h);
        } else if (s == JPL_ERROR_RANGE) {
            longitude_rate = longitude_delta(center[0], earlier[0]) / h;
        } else {
            return s;
        }
    }
    local.longitude_deg = center[0];
    local.latitude_deg = center[1];
    local.range_au = center[2];
    local.longitude_rate_deg_per_day = longitude_rate;
    local.range_rate_km_per_s = projected_radial;
    if (!isfinite(local.longitude_rate_deg_per_day) ||
        !isfinite(local.range_rate_km_per_s)) return JPL_ERROR_NUMERIC;
    *out = local;
    return JPL_OK;
}