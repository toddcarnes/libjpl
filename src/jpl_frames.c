/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_frames.h"
#include "jpl_private.h"
#include "erfa.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Iteration policy for the TDB-to-TT inverse: residual target 1e-15 day,
 * at most 10 corrections. Failure to meet it is CONVERGENCE, never a
 * rounded identity. */
#define JPL_TDBTT_TOL_DAYS 1e-15
#define JPL_TDBTT_MAX_ITER 10

/* Opaque frame context structure */
struct jpl_frame_context {
    double matrix[3][3];  /* Cached ecliptic->ICRS rotation matrix */
};

/* Check a jpl_date for finiteness */
static jpl_status check_date(const jpl_date *d) {
    if (!d) return JPL_ERROR_ARGUMENT;
    if (!isfinite(d->hi) || !isfinite(d->lo)) return JPL_ERROR_ARGUMENT;
    return JPL_OK;
}

/* Check a jpl_state for finiteness */
static jpl_status check_state(const jpl_state *s) {
    if (!s) return JPL_ERROR_ARGUMENT;
    if (!isfinite(s->x) || !isfinite(s->y) || !isfinite(s->z) ||
        !isfinite(s->vx) || !isfinite(s->vy) || !isfinite(s->vz))
        return JPL_ERROR_ARGUMENT;
    return JPL_OK;
}

jpl_status jpl_icrs_to_true_ecliptic_position(const double in[3],
                                              const jpl_date *jd_tt, double out[3]) {
    double tt[2], m[3][3], p[3], rotated[3], dpsi, deps, eps;
    jpl_status status;
    size_t i, k;
    if (!in || !jd_tt || !out) return JPL_ERROR_ARGUMENT;
    for (i = 0; i < 3; ++i) {
        if (!isfinite(in[i])) return JPL_ERROR_ARGUMENT;
        p[i] = in[i];
    }
    status = check_date(jd_tt);
    if (status != JPL_OK) return status;
    double jd_tt_arr[2];
    jpl_date_to_pair(jd_tt, jd_tt_arr);
    status = jpl_date_normalize(jd_tt_arr, tt);
    if (status != JPL_OK) return status;
    eraPnm06a(tt[0], tt[1], m);
    eraNut06a(tt[0], tt[1], &dpsi, &deps);
    eps = eraObl06(tt[0], tt[1]) + deps;
    if (!isfinite(eps) || !isfinite(dpsi)) return JPL_ERROR_NUMERIC;
    eraRx(eps, m);
    for (i = 0; i < 3; ++i) {
        for (k = 0; k < 3; ++k) {
            if (!isfinite(m[i][k])) return JPL_ERROR_NUMERIC;
        }
    }
    eraRxp(m, p, rotated);
    for (i = 0; i < 3; ++i) {
        if (!isfinite(rotated[i])) return JPL_ERROR_NUMERIC;
    }
    memcpy(out, rotated, sizeof rotated);
    return JPL_OK;
}

jpl_status jpl_frame_rotation_matrix(const jpl_date *jd_tt, double R[3][3]) {
    double norm[2], ecm[3][3], local[3][3];
    jpl_status status;
    size_t i, j;
    if (!jd_tt || !R) return JPL_ERROR_ARGUMENT;
    status = check_date(jd_tt);
    if (status != JPL_OK) return status;
    double jd_tt_arr[2];
    jpl_date_to_pair(jd_tt, jd_tt_arr);
    status = jpl_date_normalize(jd_tt_arr, norm);
    if (status != JPL_OK) return status == JPL_ERROR_ARGUMENT ? JPL_ERROR_ARGUMENT : JPL_ERROR_NUMERIC;
    /* Pass both parts to ERFA: eraEcm06(date1, date2) keeps the small
     * residual alive instead of rounding a scalar full epoch. Compute
     * into a local so R stays unchanged on failure; validate finiteness
     * of the matrix before publishing. */
    eraEcm06(norm[0], norm[1], ecm);
    eraTr(ecm, local);
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            if (!isfinite(local[i][j])) return JPL_ERROR_NUMERIC;
        }
    }
    memcpy(R, local, sizeof local);
    return JPL_OK;
}

jpl_status jpl_ecliptic_to_icrs(const jpl_state *state, const jpl_date *jd_tt, jpl_state *out_state) {
    double R[3][3], pr[3], vr[3];
    double m[3][3];
    jpl_status status;
    if (!state || !jd_tt || !out_state) return JPL_ERROR_ARGUMENT;
    status = check_state(state);
    if (status != JPL_OK) return status;
    status = jpl_frame_rotation_matrix(jd_tt, R);
    if (status != JPL_OK) return status;
    /* Compute into locals first: outputs stay unchanged on failure and
     * in-place (aliasing) calls stay exact. Publish position only after
     * velocity also succeeds: both legs share one transactional commit. */
    memcpy(m, R, sizeof m);
    {
        double p[3], v[3];
        size_t i;
        p[0] = state->x; p[1] = state->y; p[2] = state->z;
        v[0] = state->vx; v[1] = state->vy; v[2] = state->vz;
        eraRxp(m, p, pr);
        eraRxp(m, v, vr);
        for (i = 0; i < 3; ++i) {
            if (!isfinite(pr[i]) || !isfinite(vr[i])) return JPL_ERROR_NUMERIC;
        }
    }
    out_state->x = pr[0]; out_state->y = pr[1]; out_state->z = pr[2];
    out_state->vx = vr[0]; out_state->vy = vr[1]; out_state->vz = vr[2];
    return JPL_OK;
}

jpl_status jpl_icrs_to_ecliptic(const jpl_state *state, const jpl_date *jd_tt, jpl_state *out_state) {
    double norm[2], ecm[3][3], pr[3], vr[3];
    double m[3][3];
    jpl_status status;
    if (!state || !jd_tt || !out_state) return JPL_ERROR_ARGUMENT;
    status = check_state(state);
    if (status != JPL_OK) return status;
    status = check_date(jd_tt);
    if (status != JPL_OK) return status;
    double jd_tt_arr[2];
    jpl_date_to_pair(jd_tt, jd_tt_arr);
    status = jpl_date_normalize(jd_tt_arr, norm);
    if (status != JPL_OK) return status == JPL_ERROR_ARGUMENT ? JPL_ERROR_ARGUMENT : JPL_ERROR_NUMERIC;
    eraEcm06(norm[0], norm[1], ecm);
    /* Mirror the rotation-matrix finiteness check: do not publish a
     * nonfinite local matrix from a finite date. */
    {
        size_t i, k;
        for (i = 0; i < 3; ++i) {
            for (k = 0; k < 3; ++k) {
                if (!isfinite(ecm[i][k])) return JPL_ERROR_NUMERIC;
            }
        }
    }
    memcpy(m, ecm, sizeof m);
    {
        double p[3], v[3];
        size_t i;
        p[0] = state->x; p[1] = state->y; p[2] = state->z;
        v[0] = state->vx; v[1] = state->vy; v[2] = state->vz;
        eraRxp(m, p, pr);
        eraRxp(m, v, vr);
        for (i = 0; i < 3; ++i) {
            if (!isfinite(pr[i]) || !isfinite(vr[i])) return JPL_ERROR_NUMERIC;
        }
    }
    out_state->x = pr[0]; out_state->y = pr[1]; out_state->z = pr[2];
    out_state->vx = vr[0]; out_state->vy = vr[1]; out_state->vz = vr[2];
    return JPL_OK;
}

jpl_status jpl_tt_to_tdb(const jpl_date *jd_tt, jpl_date *jd_tdb) {
    double norm[2], correction;
    jpl_status status;
    if (!jd_tt || !jd_tdb) return JPL_ERROR_ARGUMENT;
    status = check_date(jd_tt);
    if (status != JPL_OK) return status;
    double jd_tt_arr[2];
    jpl_date_to_pair(jd_tt, jd_tt_arr);
    status = jpl_date_normalize(jd_tt_arr, norm);
    if (status != JPL_OK) return status == JPL_ERROR_ARGUMENT ? JPL_ERROR_ARGUMENT : JPL_ERROR_NUMERIC;
    correction = eraDtdb(norm[0], norm[1], 0.0, 0.0, 0.0, 0.0) / JPL_DAYSEC;
    if (!isfinite(correction)) return JPL_ERROR_NUMERIC;
    /* Exact aliasing: compute into a local, then copy out. */
    {
        double out_arr[2];
        status = jpl_date_add_days(norm, correction, out_arr);
        if (status != JPL_OK) return status;
        jd_tdb->hi = out_arr[0];
        jd_tdb->lo = out_arr[1];
    }
    return JPL_OK;
}

jpl_status jpl_tdb_to_tt(const jpl_date *jd_tdb, jpl_date *jd_tt) {
    double input[2], candidate[2], predicted[2], residual[2], next[2];
    double correction;
    jpl_status status;
    int i;
    if (!jd_tdb || !jd_tt) return JPL_ERROR_ARGUMENT;
    status = check_date(jd_tdb);
    if (status != JPL_OK) return status;
    double jd_tdb_arr[2];
    jpl_date_to_pair(jd_tdb, jd_tdb_arr);
    status = jpl_date_normalize(jd_tdb_arr, input);
    if (status != JPL_OK) return status == JPL_ERROR_ARGUMENT ? JPL_ERROR_ARGUMENT : JPL_ERROR_NUMERIC;
    /* Start at TDB, then correct: predicted = candidate + dtdb(candidate),
     * residual = predicted - input, next = candidate - residual. */
    candidate[0] = input[0];
    candidate[1] = input[1];
    for (i = 0; i < JPL_TDBTT_MAX_ITER; ++i) {
        double resid_days;
        correction = eraDtdb(candidate[0], candidate[1], 0.0, 0.0, 0.0, 0.0) / JPL_DAYSEC;
        if (!isfinite(correction)) return JPL_ERROR_NUMERIC;
        status = jpl_date_add_days(candidate, correction, predicted);
        if (status != JPL_OK) return status;
        status = jpl_date_subtract(predicted, input, residual);
        if (status != JPL_OK) return status;
        resid_days = residual[0] + residual[1];
        if (!isfinite(resid_days)) return JPL_ERROR_NUMERIC;
        if (fabs(resid_days) <= JPL_TDBTT_TOL_DAYS) {
            jd_tt->hi = candidate[0];
            jd_tt->lo = candidate[1];
            return JPL_OK;
        }
        status = jpl_date_subtract(candidate, residual, next);
        if (status != JPL_OK) return status;
        candidate[0] = next[0];
        candidate[1] = next[1];
    }
    return JPL_ERROR_CONVERGENCE;
}

jpl_status jpl_frame_context_create(const jpl_date *jd_tt, jpl_frame_context **out_context) {
    double norm[2], ecm[3][3], local[3][3];
    jpl_status status;
    size_t i, j;
    if (!jd_tt || !out_context) return JPL_ERROR_ARGUMENT;
    status = check_date(jd_tt);
    if (status != JPL_OK) return status;
    double jd_tt_arr[2];
    jpl_date_to_pair(jd_tt, jd_tt_arr);
    status = jpl_date_normalize(jd_tt_arr, norm);
    if (status != JPL_OK) return status == JPL_ERROR_ARGUMENT ? JPL_ERROR_ARGUMENT : JPL_ERROR_NUMERIC;
    eraEcm06(norm[0], norm[1], ecm);
    eraTr(ecm, local);
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            if (!isfinite(local[i][j])) return JPL_ERROR_NUMERIC;
        }
    }
    jpl_frame_context *ctx = malloc(sizeof *ctx);
    if (!ctx) return JPL_ERROR_MEMORY;
    memcpy(ctx->matrix, local, sizeof local);
    *out_context = ctx;
    return JPL_OK;
}

void jpl_frame_context_destroy(jpl_frame_context *context) {
    if (context) free(context);
}

jpl_status jpl_frame_context_rotate_position(jpl_frame_context *context, const double in[3], double out[3]) {
    double pr[3];
    if (!context || !in || !out) return JPL_ERROR_ARGUMENT;
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(in[i])) return JPL_ERROR_ARGUMENT;
    }
    eraRxp(context->matrix, in, pr);
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(pr[i])) return JPL_ERROR_NUMERIC;
    }
    out[0] = pr[0]; out[1] = pr[1]; out[2] = pr[2];
    return JPL_OK;
}

jpl_status jpl_frame_context_rotate_velocity(jpl_frame_context *context, const double in[3], double out[3]) {
    double vr[3];
    if (!context || !in || !out) return JPL_ERROR_ARGUMENT;
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(in[i])) return JPL_ERROR_ARGUMENT;
    }
    eraRxp(context->matrix, in, vr);
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(vr[i])) return JPL_ERROR_NUMERIC;
    }
    out[0] = vr[0]; out[1] = vr[1]; out[2] = vr[2];
    return JPL_OK;
}

jpl_status jpl_frame_context_rotate_state(jpl_frame_context *context, const jpl_state *state, jpl_state *out_state) {
    double pr[3], vr[3];
    if (!context || !state || !out_state) return JPL_ERROR_ARGUMENT;
    jpl_status s = check_state(state);
    if (s != JPL_OK) return s;
    double p[3] = {state->x, state->y, state->z};
    double v[3] = {state->vx, state->vy, state->vz};
    eraRxp(context->matrix, p, pr);
    eraRxp(context->matrix, v, vr);
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(pr[i]) || !isfinite(vr[i])) return JPL_ERROR_NUMERIC;
    }
    out_state->x = pr[0]; out_state->y = pr[1]; out_state->z = pr[2];
    out_state->vx = vr[0]; out_state->vy = vr[1]; out_state->vz = vr[2];
    return JPL_OK;
}