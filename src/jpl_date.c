/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_private.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* Maximum exactly representable integer index/count (2^53). */
#define JPL_DATE_MAX_EXACT UINT64_C(9007199254740992)

static void two_sum(double a, double b, double *s, double *e) {
    double x = a + b;
    double bv = x - a;
    *s = x;
    *e = (a - (x - bv)) + (b - bv);
}

static int pair_finite(const double p[2]) {
    return isfinite(p[0]) && isfinite(p[1]);
}

jpl_status jpl_date_normalize(const double in[2], double out[2]) {
    double s, e;
    if (in == NULL || out == NULL) {
        return JPL_ERROR_ARGUMENT;
    }
    if (!pair_finite(in)) {
        return JPL_ERROR_ARGUMENT;
    }
    two_sum(in[0], in[1], &s, &e);
    if (!isfinite(s) || !isfinite(e)) {
        return JPL_ERROR_NUMERIC;
    }
    if (s == 0.0) {
        s = 0.0;
        e = 0.0;
    } else if (e == 0.0) {
        e = 0.0;
    }
    out[0] = s;
    out[1] = e;
    return JPL_OK;
}

/* All input pairs here are already normalized finite locals. */
static jpl_status add_core(const double a[2], const double b[2], double result[2]) {
    double s, e, t, f, u, v, h, l, inner, tail, r0, r1;
    JPL_ASSERT_NORMALIZED(a);
    JPL_ASSERT_NORMALIZED(b);
    two_sum(a[0], b[0], &s, &e);
    two_sum(a[1], b[1], &t, &f);
    if (!isfinite(s) || !isfinite(e) || !isfinite(t) || !isfinite(f)) {
        return JPL_ERROR_NUMERIC;
    }
    two_sum(e, t, &u, &v);
    if (!isfinite(u) || !isfinite(v)) {
        return JPL_ERROR_NUMERIC;
    }
    two_sum(s, u, &h, &l);
    if (!isfinite(h) || !isfinite(l)) {
        return JPL_ERROR_NUMERIC;
    }
    inner = v + f;
    if (!isfinite(inner)) {
        return JPL_ERROR_NUMERIC;
    }
    tail = l + inner;
    if (!isfinite(tail)) {
        return JPL_ERROR_NUMERIC;
    }
    two_sum(h, tail, &r0, &r1);
    if (!isfinite(r0) || !isfinite(r1)) {
        return JPL_ERROR_NUMERIC;
    }
    if (r0 == 0.0) {
        r0 = 0.0;
        r1 = 0.0;
    } else if (r1 == 0.0) {
        r1 = 0.0;
    }
    result[0] = r0;
    result[1] = r1;
    return JPL_OK;
}

jpl_status jpl_date_add(const double a[2], const double b[2], double out[2]) {
    double na[2], nb[2], result[2];
    jpl_status status;
    if (a == NULL || b == NULL || out == NULL) {
        return JPL_ERROR_ARGUMENT;
    }
    if (!pair_finite(a) || !pair_finite(b)) {
        return JPL_ERROR_ARGUMENT;
    }
    status = jpl_date_normalize(a, na);
    if (status != JPL_OK) {
        return status;
    }
    status = jpl_date_normalize(b, nb);
    if (status != JPL_OK) {
        return status;
    }
    status = add_core(na, nb, result);
    if (status != JPL_OK) {
        return status;
    }
    out[0] = result[0];
    out[1] = result[1];
    return JPL_OK;
}

jpl_status jpl_date_subtract(const double a[2], const double b[2], double out[2]) {
    double na[2], nb[2], neg[2], nneg[2], result[2];
    jpl_status status;
    if (a == NULL || b == NULL || out == NULL) {
        return JPL_ERROR_ARGUMENT;
    }
    if (!pair_finite(a) || !pair_finite(b)) {
        return JPL_ERROR_ARGUMENT;
    }
    status = jpl_date_normalize(a, na);
    if (status != JPL_OK) {
        return status;
    }
    status = jpl_date_normalize(b, nb);
    if (status != JPL_OK) {
        return status;
    }
    neg[0] = -nb[0];
    neg[1] = -nb[1];
    status = jpl_date_normalize(neg, nneg);
    if (status != JPL_OK) {
        return status;
    }
    status = add_core(na, nneg, result);
    if (status != JPL_OK) {
        return status;
    }
    out[0] = result[0];
    out[1] = result[1];
    return JPL_OK;
}

jpl_status jpl_date_add_days(const double date[2], double days, double out[2]) {
    double step[2];
    if (date == NULL || out == NULL) {
        return JPL_ERROR_ARGUMENT;
    }
    if (!pair_finite(date) || !isfinite(days)) {
        return JPL_ERROR_ARGUMENT;
    }
    step[0] = days;
    step[1] = 0.0;
    return jpl_date_add(date, step, out);
}

jpl_status jpl_date_add_scaled(const double date[2], double step,
                               size_t index, double out[2]) {
    double dindex, p, r, offset[2];
    if (date == NULL || out == NULL) {
        return JPL_ERROR_ARGUMENT;
    }
    if (!pair_finite(date) || !isfinite(step)) {
        return JPL_ERROR_ARGUMENT;
    }
    /* On 64-bit size_t the index can exceed the exactly representable
     * range; on 32-bit size_t every value fits, so no comparison is
     * needed (and comparing would be vacuous). The exact round-trip
     * check below still applies on both widths. */
#if SIZE_MAX > JPL_DATE_MAX_EXACT
    if ((uintmax_t)index > JPL_DATE_MAX_EXACT) {
        return JPL_ERROR_ARGUMENT;
    }
#endif
    dindex = (double)index;
    if (!isfinite(dindex) || (uintmax_t)dindex != (uintmax_t)index) {
        return JPL_ERROR_ARGUMENT;
    }
    p = dindex * step;
    r = fma(dindex, step, -p);
    if (!isfinite(p) || !isfinite(r)) {
        return JPL_ERROR_NUMERIC;
    }
    offset[0] = p;
    offset[1] = r;
    return jpl_date_add(date, offset, out);
}

jpl_status jpl_date_partition(const double begin[2], const double end[2],
                              size_t count, size_t index, double out[2]) {
    double n, di, span[2], qhi, qlo, hip, res, lop, tail, offset[2];
    jpl_status status;
    if (begin == NULL || end == NULL || out == NULL) {
        return JPL_ERROR_ARGUMENT;
    }
    if (!pair_finite(begin) || !pair_finite(end)) {
        return JPL_ERROR_ARGUMENT;
    }
    /* Same width-conditional bound as jpl_date_add_scaled: vacuous on
     * 32-bit size_t, load-bearing on 64-bit. */
#if SIZE_MAX > JPL_DATE_MAX_EXACT
    if (count == 0 || (uintmax_t)count > JPL_DATE_MAX_EXACT || index > count) {
        return JPL_ERROR_ARGUMENT;
    }
#else
    if (count == 0 || index > count) {
        return JPL_ERROR_ARGUMENT;
    }
#endif
    if (index == 0) {
        out[0] = begin[0];
        out[1] = begin[1];
        return JPL_OK;
    }
    if (index == count) {
        out[0] = end[0];
        out[1] = end[1];
        return JPL_OK;
    }
    n = (double)count;
    di = (double)index;
    if (!isfinite(n) || !isfinite(di) || (uintmax_t)n != (uintmax_t)count ||
        (uintmax_t)di != (uintmax_t)index) {
        return JPL_ERROR_ARGUMENT;
    }
    status = jpl_date_subtract(end, begin, span);
    if (status != JPL_OK) {
        return status;
    }
    qhi = span[0] / n;
    qlo = (fma(-qhi, n, span[0]) + span[1]) / n;
    if (!isfinite(qhi) || !isfinite(qlo)) {
        return JPL_ERROR_NUMERIC;
    }
    hip = di * qhi;
    res = fma(di, qhi, -hip);
    if (!isfinite(hip) || !isfinite(res)) {
        return JPL_ERROR_NUMERIC;
    }
    lop = di * qlo;
    if (!isfinite(lop)) {
        return JPL_ERROR_NUMERIC;
    }
    tail = res + lop;
    if (!isfinite(tail)) {
        return JPL_ERROR_NUMERIC;
    }
    offset[0] = hip;
    offset[1] = tail;
    return jpl_date_add(begin, offset, out);
}

int jpl_date_compare(const double a[2], const double b[2]) {
    assert(isfinite(a[0]) && isfinite(a[1]) && isfinite(b[0]) && isfinite(b[1]));
    if (a[0] < b[0]) {
        return -1;
    }
    if (a[0] > b[0]) {
        return 1;
    }
    if (a[1] < b[1]) {
        return -1;
    }
    if (a[1] > b[1]) {
        return 1;
    }
    return 0;
}
