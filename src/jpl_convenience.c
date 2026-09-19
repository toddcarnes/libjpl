/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_convenience.h"
#include "jpl_private.h"
#include "jpl_frames.h"
#include <math.h>
#include <string.h>
/* DE coefficient records are Cartesian in the ICRS equatorial frame, so
 * jpl_get_state output is already ICRS: no rotation is applied here. The
 * frames module stays for user-side ecliptic conversion. */
/* Modern-AU unit conversions: status-returning, transactional locals.
 * Reject NULL/nonfinite inputs and unrepresentable outputs; leave outputs
 * unchanged on failure. Exact input/output aliasing is supported. */
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
    double tmp;
    if (!au) return JPL_ERROR_ARGUMENT;
    if (!isfinite(km)) return JPL_ERROR_ARGUMENT;
    tmp = km / JPL_AU_KM;
    if (!isfinite(tmp)) return JPL_ERROR_NUMERIC;
    *au = tmp;
    return JPL_OK;
}
jpl_status jpl_planet_position(const jpl_ephemeris *e, const double jd_tt[2], int planet, int center, double st[6]) {
    const jpl_header *header;
    double tt[2], tdb[2], lt, tmp[6], vobs[3], cst[6];
    double cov_start[2], cov_end[2];
    int eff_center;
    jpl_status s;
    if(!e||!st||!jd_tt) return JPL_ERROR_ARGUMENT;
    if(!isfinite(jd_tt[0])||!isfinite(jd_tt[1])) return JPL_ERROR_ARGUMENT;
    /* Targets 1-11 (Mercury..Sun, incl. Earth 3) or EMB 13. Target zero
     * is consistently invalid (L05); SSB 12 and the nutation (14) /
     * libration (15) sensitivities plus out-of-range codes are rejected. */
    if(!((planet>=1&&planet<=11)||planet==JPL_EARTH_MOON_BARYCENTER)) return JPL_ERROR_ARGUMENT;
    /* Centers: 0 (SSB) or bodies 1-13. Reject negatives and anything above 13. */
    if(center<0||center>JPL_EARTH_MOON_BARYCENTER) return JPL_ERROR_ARGUMENT;
    s = jpl_date_normalize(jd_tt, tt);
    if(s!=JPL_OK) return JPL_ERROR_ARGUMENT;
    /* Convert convenience TT input to a local TDB pair; propagate errors. */
    s = jpl_tt_to_tdb(tt, tdb);
    if(s!=JPL_OK) return s;
    /* Validate TDB coverage before frame gating, so out-of-coverage
     * queries report RANGE regardless of frame identity. */
    header = jpl_get_header(e);
    if(!header) return JPL_ERROR_ARGUMENT;
    s = jpl_date_normalize(header->start_jd, cov_start);
    if(s!=JPL_OK) return JPL_ERROR_FORMAT;
    s = jpl_date_normalize(header->end_jd, cov_end);
    if(s!=JPL_OK) return JPL_ERROR_FORMAT;
    if(jpl_date_compare(tdb, cov_start)<0||jpl_date_compare(tdb, cov_end)>0) return JPL_ERROR_RANGE;
    /* ICRS convenience requires ICRF-aligned metadata (R04), checked even
     * for identical-body calls so a zero vector cannot imply support for
     * a legacy frame. Core jpl_get_state stays available for native reads. */
    if(header->reference_frame!=JPL_FRAME_ICRF) return JPL_ERROR_UNSUPPORTED;
    /* Center 0 is the SSB alias (L05, R02); normalize to 12 wherever a
     * target code is required because target 0 is invalid in core getters. */
    eff_center = center==JPL_SSB ? JPL_SOLAR_SYSTEM_BARYCENTER : center;
    /* Identical-body shortcut (R15): validate the required body at
     * reception, then return zero without touching unrelated Sun data. */
    if(planet==center) {
        s = jpl_get_state(e, tdb, planet, center, tmp);
        if(s!=JPL_OK) return s;
        memset(st, 0, sizeof tmp);
        return JPL_OK;
    }
    /* SSB-observer shortcut (R02): the barycentric observer velocity is
     * zero by definition, so no aberration/Sun lookup is needed. */
    if(eff_center==JPL_SOLAR_SYSTEM_BARYCENTER) {
        s = jpl_get_state_lighttime(e, tdb, planet, center, tmp, &lt);
        if(s!=JPL_OK) return s;
        memcpy(st, tmp, sizeof tmp);
        return JPL_OK;
    }
    s = jpl_get_state_lighttime(e, tdb, planet, center, tmp, &lt);
    if(s!=JPL_OK) return s;
    /* Aberration needs the observer (center-body) barycentric velocity at
     * reception (tdb); stellar aberration is a reception phenomenon.
     * Sun distance for the <=0.4uas Klioner term comes from one extra
     * evaluation (skipped when the observer is the Sun itself). */
    s = jpl_get_state(e, tdb, eff_center, 0, cst);
    if (s != JPL_OK) return s;
    vobs[0] = cst[3]; vobs[1] = cst[4]; vobs[2] = cst[5];
    if (eff_center != JPL_SUN) {
        double sun_obs[6];
        double sdist;
        jpl_status as;
        s = jpl_get_state(e, tdb, eff_center, JPL_SUN, sun_obs);
        if (s != JPL_OK) return s;
        sdist = jpl_vec_norm3(sun_obs);
        if (!isfinite(sdist)) return JPL_ERROR_NUMERIC;
        /* Finite nonpositive Sun distance omits the potential term
         * inside the aberration call; nonfinite is ARGUMENT there. */
        as = jpl_apply_aberration_full(tmp, vobs, sdist, tmp);
        if (as != JPL_OK) return as;
    } else {
        s = jpl_apply_aberration(tmp, vobs, tmp);
        if (s != JPL_OK) return s;
    }
    memcpy(st, tmp, sizeof tmp);
    return JPL_OK;
}
jpl_status jpl_moon_position(const jpl_ephemeris *e, const double jd_tt[2], double st[6]) {
    return jpl_planet_position(e, jd_tt, JPL_MOON, JPL_EARTH, st);
}
