/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_frames.h"
#include "jpl_private.h"
#include "erfa.h"
#include <math.h>
#include <string.h>

/* Iteration policy for the TDB-to-TT inverse: residual target 1e-15 day,
 * at most 10 corrections. Failure to meet it is CONVERGENCE, never a
 * rounded identity. */
#define JPL_TDBTT_TOL_DAYS 1e-15
#define JPL_TDBTT_MAX_ITER 10

static jpl_status check_pair(const double p[2]) {
    if (!p) return JPL_ERROR_ARGUMENT;
    if (!isfinite(p[0]) || !isfinite(p[1])) return JPL_ERROR_ARGUMENT;
    return JPL_OK;
}

jpl_status jpl_frame_rotation_matrix(const double jd_tt[2], double R[3][3]) {
    double norm[2], ecm[3][3], local[3][3];
    jpl_status status;
    size_t i, j;
    if (!jd_tt || !R) return JPL_ERROR_ARGUMENT;
    status = check_pair(jd_tt);
    if (status != JPL_OK) return status;
    status = jpl_date_normalize(jd_tt, norm);
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

jpl_status jpl_ecliptic_to_icrs(const double s[6], const double jd_tt[2], double o[6]) {
    double R[3][3], pr[3], vr[3];
    double m[3][3];
    jpl_status status;
    if (!s || !jd_tt || !o) return JPL_ERROR_ARGUMENT;
    status = jpl_frame_rotation_matrix(jd_tt, R);
    if (status != JPL_OK) return status;
    /* Compute into locals first: outputs stay unchanged on failure and
     * in-place (aliasing) calls stay exact. Publish position only after
     * velocity also succeeds: both legs share one transactional commit. */
    memcpy(m, R, sizeof m);
    {
        double p[3], v[3];
        size_t i;
        for (i = 0; i < 6; ++i) {
            if (!isfinite(s[i])) return JPL_ERROR_ARGUMENT;
        }
        p[0] = s[0]; p[1] = s[1]; p[2] = s[2];
        v[0] = s[3]; v[1] = s[4]; v[2] = s[5];
        eraRxp(m, p, pr);
        eraRxp(m, v, vr);
        for (i = 0; i < 3; ++i) {
            if (!isfinite(pr[i]) || !isfinite(vr[i])) return JPL_ERROR_NUMERIC;
        }
    }
    memcpy(o, pr, 3 * sizeof(double));
    memcpy(o + 3, vr, 3 * sizeof(double));
    return JPL_OK;
}

jpl_status jpl_icrs_to_ecliptic(const double s[6], const double jd_tt[2], double o[6]) {
    double norm[2], ecm[3][3], pr[3], vr[3];
    double m[3][3];
    jpl_status status;
    if (!s || !jd_tt || !o) return JPL_ERROR_ARGUMENT;
    status = check_pair(jd_tt);
    if (status != JPL_OK) return status;
    status = jpl_date_normalize(jd_tt, norm);
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
        for (i = 0; i < 6; ++i) {
            if (!isfinite(s[i])) return JPL_ERROR_ARGUMENT;
        }
        p[0] = s[0]; p[1] = s[1]; p[2] = s[2];
        v[0] = s[3]; v[1] = s[4]; v[2] = s[5];
        eraRxp(m, p, pr);
        eraRxp(m, v, vr);
        for (i = 0; i < 3; ++i) {
            if (!isfinite(pr[i]) || !isfinite(vr[i])) return JPL_ERROR_NUMERIC;
        }
    }
    memcpy(o, pr, 3 * sizeof(double));
    memcpy(o + 3, vr, 3 * sizeof(double));
    return JPL_OK;
}

jpl_status jpl_tt_to_tdb(const double jd_tt[2], double jd_tdb[2]) {
    double norm[2], correction;
    jpl_status status;
    if (!jd_tt || !jd_tdb) return JPL_ERROR_ARGUMENT;
    status = check_pair(jd_tt);
    if (status != JPL_OK) return status;
    status = jpl_date_normalize(jd_tt, norm);
    if (status != JPL_OK) return status == JPL_ERROR_ARGUMENT ? JPL_ERROR_ARGUMENT : JPL_ERROR_NUMERIC;
    correction = eraDtdb(norm[0], norm[1], 0.0, 0.0, 0.0, 0.0) / JPL_DAYSEC;
    if (!isfinite(correction)) return JPL_ERROR_NUMERIC;
    /* Exact aliasing: compute into a local, then copy out. */
    {
        double out[2];
        status = jpl_date_add_days(norm, correction, out);
        if (status != JPL_OK) return status;
        jd_tdb[0] = out[0];
        jd_tdb[1] = out[1];
    }
    return JPL_OK;
}

jpl_status jpl_tdb_to_tt(const double jd_tdb[2], double jd_tt[2]) {
    double input[2], candidate[2], predicted[2], residual[2], next[2];
    double correction;
    jpl_status status;
    int i;
    if (!jd_tdb || !jd_tt) return JPL_ERROR_ARGUMENT;
    status = check_pair(jd_tdb);
    if (status != JPL_OK) return status;
    status = jpl_date_normalize(jd_tdb, input);
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
            jd_tt[0] = candidate[0];
            jd_tt[1] = candidate[1];
            return JPL_OK;
        }
        status = jpl_date_subtract(candidate, residual, next);
        if (status != JPL_OK) return status;
        candidate[0] = next[0];
        candidate[1] = next[1];
    }
    return JPL_ERROR_CONVERGENCE;
}
