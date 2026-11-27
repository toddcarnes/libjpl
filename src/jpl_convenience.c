/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_convenience.h"
#include "jpl_private.h"
#include "jpl_frames.h"
#include "erfa.h"
#include "erfam.h"
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

/* TT→TDB policy (see jpl_private.h): file-interpolated refinement over a
 * guaranteed model baseline. The model converts first (seed and fallback);
 * when the file carries TT-TDB and the seed is in coverage, fixed-point
 * refinements TDB = TT - f(TDB)/86400 converge the millisecond-scale
 * correction to arithmetic noise. Any file-path failure (absent or
 * span-invalid data, corrupt coefficients, out-of-coverage seed) keeps
 * the model result: the file is an enhancement, never a new failure mode.
 * Deterministic: fixed iteration count, no convergence error. The model
 * leg is accurate to ±3 ns over 1950–2050 (see jpl_tt_to_tdb); outside
 * that envelope the model value still stands, unchanged from before. */
jpl_status jpl_tt_to_tdb_policy(const jpl_ephemeris *e, const jpl_date *tt, jpl_date *tdb) {
    double tt_arr[2], cur[2], sec;
    int i;
    jpl_status s;
    if (!e || !tt || !tdb) return JPL_ERROR_ARGUMENT;
    s = jpl_tt_to_tdb(tt, tdb);
    if (s != JPL_OK) return s;
    if (!jpl_has_tt_tdb(e)) return JPL_OK;
    jpl_date_to_pair(tt, tt_arr);
    jpl_date_to_pair(tdb, cur);
    for (i = 0; i < 3; ++i) {
        jpl_date cur_date;
        double next[2];
        jpl_pair_to_date(cur, &cur_date);
        s = jpl_get_tt_minus_tdb(e, &cur_date, &sec, NULL);
        if (s != JPL_OK) return JPL_OK;
        if (!isfinite(sec)) return JPL_OK;
        s = jpl_date_add_days(tt_arr, -(sec / JPL_DAYSEC), next);
        if (s != JPL_OK) return JPL_OK;
        cur[0] = next[0]; cur[1] = next[1];
    }
    jpl_pair_to_date(cur, tdb);
    return JPL_OK;
}

/* The ordinary body-center path needs these evaluations in one order:
 * light-time solve, observer state at reception, Sun leg at reception,
 * then the emission epoch by compensated subtraction. Returns them as one
 * atomic value so callers cannot mix reception and emission inputs. */
static jpl_status jpl_compute_geometry(const jpl_ephemeris *e, const jpl_date *tdb,
                                       int target, int center, jpl_geometry *out) {
    jpl_geometry g;
    jpl_state observer_state, sun_state;
    double tdb_arr[2], emission_arr[2];
    jpl_status s;
    if (!e || !tdb || !out) return JPL_ERROR_ARGUMENT;
    s = jpl_get_state_lighttime(e, tdb, target, center, &g.rel_state, &g.light_time_days);
    if (s != JPL_OK) return s;
    s = jpl_get_state(e, tdb, center, JPL_SSB, &observer_state);
    if (s != JPL_OK) return s;
    g.observer_vel[0] = observer_state.vx;
    g.observer_vel[1] = observer_state.vy;
    g.observer_vel[2] = observer_state.vz;
    if (center == JPL_SUN) {
        g.sun_vec[0] = g.sun_vec[1] = g.sun_vec[2] = 0.0;
    } else {
        s = jpl_get_state(e, tdb, JPL_SUN, center, &sun_state);
        if (s != JPL_OK) return s;
        g.sun_vec[0] = sun_state.x;
        g.sun_vec[1] = sun_state.y;
        g.sun_vec[2] = sun_state.z;
    }
    jpl_date_to_pair(tdb, tdb_arr);
    s = jpl_date_add_days(tdb_arr, -g.light_time_days, emission_arr);
    if (s != JPL_OK) return s;
    jpl_pair_to_date(emission_arr, &g.emission);
    *out = g;
    return JPL_OK;
}

static jpl_status convenience_epoch(const jpl_ephemeris *e, const jpl_date *jd_tt,
                                    jpl_date *tt, jpl_date *tdb) {
    const jpl_header *header;
    jpl_entry_ctx entry;
    double jd_tt_arr[2], tdb_arr[2];
    jpl_status s;
    if (!e || !jd_tt || !tt || !tdb) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd_tt->hi) || !isfinite(jd_tt->lo)) return JPL_ERROR_ARGUMENT;
    jpl_date_to_pair(jd_tt, jd_tt_arr);
    s = jpl_entry_prepare(e, jd_tt_arr, &entry);
    if (s != JPL_OK) return s;
    memcpy(jd_tt_arr, entry.norm, sizeof jd_tt_arr);
    jpl_pair_to_date(jd_tt_arr, tt);
    s = jpl_tt_to_tdb_policy(e, tt, tdb);
    if (s != JPL_OK) return s;
    header = jpl_get_header(e);
    if (!header) return JPL_ERROR_ARGUMENT;
    jpl_date_to_pair(tdb, tdb_arr);
    /* Coverage is checked on the TDB epoch the data reads use, so an
     * out-of-coverage query stays RANGE; the frame gate below stays last
     * (UNSUPPORTED). */
    s = jpl_entry_coverage(&entry, tdb_arr);
    if (s != JPL_OK) return s;
    if (header->reference_frame != JPL_FRAME_ICRF) return JPL_ERROR_UNSUPPORTED;
    return JPL_OK;
}

jpl_status jpl_planet_position(const jpl_ephemeris *e, const jpl_date *jd_tt, int planet, int center, jpl_state *st) {
    jpl_date tt, tdb;
    jpl_state tmp;
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
        s = jpl_get_state_lighttime(e, &tdb, planet, center, &tmp, NULL);
        if(s!=JPL_OK) return s;
        *st = tmp;
        return JPL_OK;
    }
    /* Stellar aberration uses the observer's reception velocity; Sun
     * distance supplies its solar-potential term unless the observer
     * itself is the Sun. */
    {
        jpl_geometry geo;
        double sdist = 0.0;
        s = jpl_compute_geometry(e, &tdb, planet, center, &geo);
        if (s != JPL_OK) return s;
        tmp = geo.rel_state;
        if (eff_center != JPL_SUN) {
            sdist = jpl_vec_norm3(geo.sun_vec);
            if (!isfinite(sdist)) return JPL_ERROR_NUMERIC;
        }
        /* Finite positive Sun distance includes the solar-potential term;
         * 0.0 omits it (e.g. when the observer is the Sun). */
        s = jpl_apply_aberration(&tmp, geo.observer_vel, sdist, &tmp);
        if (s != JPL_OK) return s;
    }
    *st = tmp;
    return JPL_OK;
}

jpl_status jpl_moon_position(const jpl_ephemeris *e, const jpl_date *jd_tt, jpl_state *st) {
    return jpl_planet_position(e, jd_tt, JPL_MOON, JPL_EARTH, st);
}

/* Solar-deflection stage (eraLd with one solar mass): bend the unit
 * observer-to-target direction around the Sun and return the bent
 * direction (unit; the caller restores the geometric range, which
 * deflection preserves). The eraLd argument order is pinned here, the
 * single place it can be wrong: p = observer->target, q = Sun->target,
 * e = Sun->observer (-to_sun), em = observer->Sun distance (AU). Directions
 * arrive constructed, so malformed vectors fail before eraLd instead of
 * bending silently. */
static jpl_status apparent_apply_deflection(const jpl_direction *p,
                                            const jpl_direction *q,
                                            const jpl_direction *to_sun,
                                            double sun_distance, double dlim,
                                            jpl_direction *bent) {
    jpl_direction from_sun;
    double pv[3], qv[3], ev[3], bv[3], bent_norm;
    if (!p || !q || !to_sun || !bent) return JPL_ERROR_ARGUMENT;
    if (!isfinite(sun_distance) || !isfinite(dlim)) return JPL_ERROR_ARGUMENT;
    jpl_direction_negate(to_sun, &from_sun);
    /* eraLd takes mutable arrays; copy the validated directions so the
     * stage owns its inputs (same transactional discipline as elsewhere:
     * bent is untouched unless the deflection succeeds). */
    memcpy(pv, p->v, sizeof pv);
    memcpy(qv, q->v, sizeof qv);
    memcpy(ev, from_sun.v, sizeof ev);
    eraLd(1.0, pv, qv, ev, sun_distance, dlim, bv);
    bent_norm = jpl_vec_norm3(bv);
    if (!isfinite(bent_norm) || !(bent_norm > 0.0)) return JPL_ERROR_NUMERIC;
    bent->v[0] = bv[0] / bent_norm;
    bent->v[1] = bv[1] / bent_norm;
    bent->v[2] = bv[2] / bent_norm;
    return JPL_OK;
}
/* True-ecliptic projection stage: rotate the aberrated ICRS state to true
 * ecliptic of date and report longitude/latitude in degrees. The /norm
 * renormalization is the direction constructor (same arithmetic as the
 * manual division it replaces, so Horizons goldens cannot move). */
static jpl_status apparent_project_ecliptic(const jpl_state *aberrated,
                                            const jpl_date *tt,
                                            double *longitude_deg,
                                            double *latitude_deg) {
    jpl_direction unit;
    double aberrated_arr[6], ecliptic[3], u[3];
    double longitude, latitude, angular_distance;
    jpl_status s;
    if (!aberrated || !tt || !longitude_deg || !latitude_deg) return JPL_ERROR_ARGUMENT;
    jpl_state_to_array(aberrated, aberrated_arr);
    s = jpl_icrs_to_true_ecliptic_position(aberrated_arr, tt, ecliptic);
    if (s != JPL_OK) return s;
    s = jpl_direction_from_raw(ecliptic, &unit);
    if (s != JPL_OK) return s;
    /* eraP2s takes a mutable array; the direction is already validated,
     * so this copy cannot fail. */
    memcpy(u, unit.v, sizeof u);
    eraP2s(u, &longitude, &latitude, &angular_distance);
    *longitude_deg = eraAnp(longitude) * ERFA_DR2D;
    if (*longitude_deg >= 360.0) *longitude_deg = 0.0;
    *latitude_deg = latitude * ERFA_DR2D;
    return JPL_OK;
}
static jpl_status apparent_position_at_tt(const jpl_ephemeris *e,
                                          const jpl_date *jd_tt, int target,
                                          double position[3], double *radial_km_per_s) {
    jpl_date tt, tdb;
    jpl_geometry geo;
    jpl_state state, sun_to_target;
    jpl_state aberrated;
    jpl_direction to_sun, bent;
    double pos[3];
    double distance, sun_distance, longitude, latitude;
    double projected_radial = 0.0;
    double result[3];
    jpl_status s;
    if (!e || !jd_tt || !position) return JPL_ERROR_ARGUMENT;
    if (!isfinite(jd_tt->hi) || !isfinite(jd_tt->lo)) return JPL_ERROR_ARGUMENT;
    if (target < JPL_MERCURY || target > JPL_SUN || target == JPL_EARTH)
        return JPL_ERROR_ARGUMENT;
    s = convenience_epoch(e, jd_tt, &tt, &tdb);
    if (s != JPL_OK) return s;
    s = jpl_compute_geometry(e, &tdb, target, JPL_EARTH, &geo);
    if (s != JPL_OK) return s;
    state = geo.rel_state;
    pos[0] = state.x; pos[1] = state.y; pos[2] = state.z;
    distance = jpl_vec_norm3(pos);
    sun_distance = jpl_vec_norm3(geo.sun_vec);
    if (!isfinite(distance) || !(distance > 0.0) ||
        !isfinite(sun_distance) || !(sun_distance > 0.0)) return JPL_ERROR_NUMERIC;
    if (radial_km_per_s) {
        double au_per_day = (state.x*state.vx + state.y*state.vy +
                              state.z*state.vz) / distance;
        projected_radial = au_per_day * (JPL_AU_KM / JPL_DAYSEC);
        if (!isfinite(projected_radial)) return JPL_ERROR_NUMERIC;
    }
    if (target != JPL_SUN) {
        jpl_direction p, q;
        double tpos[3], limb, dlim;
        s = jpl_get_state(e, &geo.emission, target, JPL_SUN, &sun_to_target);
        if (s != JPL_OK) return s;
        tpos[0] = sun_to_target.x; tpos[1] = sun_to_target.y; tpos[2] = sun_to_target.z;
        /* Geometric states -> Directions (constructor normalizes once and
         * rejects zero/nonfinite vectors before eraLd). */
        s = jpl_direction_from_raw(pos, &p);
        if (s != JPL_OK) return s;
        s = jpl_direction_from_raw(tpos, &q);
        if (s != JPL_OK) return s;
        s = jpl_direction_from_raw(geo.sun_vec, &to_sun);
        if (s != JPL_OK) return s;
        limb = atan2(JPL_SOLAR_RADIUS_KM / JPL_AU_KM, sun_distance);
        dlim = 0.5 * limb * limb;
        if (!isfinite(dlim) || !(dlim > 0.0)) return JPL_ERROR_NUMERIC;
        s = apparent_apply_deflection(&p, &q, &to_sun, sun_distance, dlim, &bent);
        if (s != JPL_OK) return s;
        /* Deflection preserves range: restore it from the geometric solve. */
        state.x = bent.v[0] * distance;
        state.y = bent.v[1] * distance;
        state.z = bent.v[2] * distance;
    }
    s = jpl_apply_aberration(&state, geo.observer_vel, sun_distance, &aberrated);
    if (s != JPL_OK) return s;
    s = apparent_project_ecliptic(&aberrated, &tt, &longitude, &latitude);
    if (s != JPL_OK) return s;
    result[0] = longitude;
    result[1] = latitude;
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
    double date_arr[2], sample_arr[2];
    jpl_status s;
    jpl_date_to_pair(jd_tt, date_arr);
    s = jpl_date_add_days(date_arr, days, sample_arr);
    if (s != JPL_OK) return s;
    jpl_pair_to_date(sample_arr, &sample_tt);
    return apparent_position_at_tt(e, &sample_tt, target, position, NULL);
}

/* Production sampler adapter: the estimator's abstract (ctx, tt, days) ->
 * position call is one ephemeris plus one target plus the existing offset
 * helper. */
struct apparent_sample_ctx {
    const jpl_ephemeris *e;
    int target;
};
static jpl_status apparent_sample_position(void *ctx, const jpl_date *tt,
                                           double days, double position[3]) {
    const struct apparent_sample_ctx *c = (const struct apparent_sample_ctx *)ctx;
    if (!c) return JPL_ERROR_ARGUMENT;
    return apparent_position_offset(c->e, tt, c->target, days, position);
}
static double longitude_delta(double a_deg, double b_deg) {
    return remainder(a_deg - b_deg, 360.0);
}

jpl_status jpl_estimate_longitude_rate(jpl_ecliptic_sampler sample, void *ctx,
                                       const jpl_date *tt,
                                       double center_longitude_deg,
                                       double *rate_out) {
    double earlier[3], later[3], farther[3];
    double h = 0.01;
    jpl_status s, before, after;
    int attempt;
    if (!sample || !tt || !rate_out) return JPL_ERROR_ARGUMENT;
    if (!isfinite(center_longitude_deg)) return JPL_ERROR_ARGUMENT;
    for (attempt = 0; attempt <= 12; ++attempt) {
        before = sample(ctx, tt, -h, earlier);
        after = sample(ctx, tt, h, later);
        if (before != JPL_OK && before != JPL_ERROR_RANGE) return before;
        if (after != JPL_OK && after != JPL_ERROR_RANGE) return after;
        if (before != JPL_ERROR_RANGE || after != JPL_ERROR_RANGE) break;
        h *= 0.5;
    }
    if (before == JPL_ERROR_RANGE && after == JPL_ERROR_RANGE) return JPL_ERROR_RANGE;
    if (before == JPL_OK && after == JPL_OK) {
        *rate_out = longitude_delta(later[0], earlier[0]) / (2.0 * h);
    } else if (after == JPL_OK) {
        s = sample(ctx, tt, 2.0 * h, farther);
        if (s == JPL_OK) {
            *rate_out = (4.0 * longitude_delta(later[0], center_longitude_deg) -
                         longitude_delta(farther[0], center_longitude_deg)) / (2.0 * h);
        } else if (s == JPL_ERROR_RANGE) {
            *rate_out = longitude_delta(later[0], center_longitude_deg) / h;
        } else {
            return s;
        }
    } else {
        s = sample(ctx, tt, -2.0 * h, farther);
        if (s == JPL_OK) {
            *rate_out = (-4.0 * longitude_delta(earlier[0], center_longitude_deg) +
                         longitude_delta(farther[0], center_longitude_deg)) / (2.0 * h);
        } else if (s == JPL_ERROR_RANGE) {
            *rate_out = longitude_delta(center_longitude_deg, earlier[0]) / h;
        } else {
            return s;
        }
    }
    return JPL_OK;
}

jpl_status jpl_apparent_ecliptic_position(const jpl_ephemeris *e,
                                           const jpl_date *jd_tt, int target,
                                           jpl_apparent_ecliptic_result *out) {
    double center[3], longitude_rate, projected_radial;
    jpl_apparent_ecliptic_result local;
    struct apparent_sample_ctx sctx;
    jpl_status s;
    if (!out) return JPL_ERROR_ARGUMENT;
    s = apparent_position_at_tt(e, jd_tt, target, center, &projected_radial);
    if (s != JPL_OK) return s;
    sctx.e = e;
    sctx.target = target;
    s = jpl_estimate_longitude_rate(apparent_sample_position, &sctx, jd_tt,
                                    center[0], &longitude_rate);
    if (s != JPL_OK) return s;
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
