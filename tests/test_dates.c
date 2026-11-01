/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_private.h"
#include "jpl_test_util.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Exact 2^-40 day step (~79 ns): representable, but lost by scalar
 * addition to large Julian dates such as 8000016.5. */
#define DATE_TICK 0x1p-40
#define DATE_SENTINEL 1.2345678901234567e300

static int check_pair(const double got[2], double want_hi, double want_lo, int line) {
    if (got[0] != want_hi || got[1] != want_lo) {
        fprintf(stderr, "CHECK FAILED %s:%d: got {%.17g, %.17g}, want {%.17g, %.17g}\n",
                __FILE__, line, got[0], got[1], want_hi, want_lo);
        return 0;
    }
    return 1;
}

#define CHECK_PAIR(got, hi, lo) JPL_TEST_CHECK(check_pair((got), (hi), (lo), __LINE__))

static void fill_sentinel(double out[2]) {
    out[0] = DATE_SENTINEL;
    out[1] = DATE_SENTINEL;
}

int main(void) {
    static const double plus_zero[2] = {0.0, 0.0};
    static const double epochs[3] = {-3100015.5, 2451545.0, 8000016.5};
    size_t i, k;

    /* New status vocabulary from spec section 2.2. */
    {
        const char *unsup = jpl_status_string(JPL_ERROR_UNSUPPORTED);
        const char *conv = jpl_status_string(JPL_ERROR_CONVERGENCE);
        const char *num = jpl_status_string(JPL_ERROR_NUMERIC);
        JPL_TEST_CHECK(unsup != NULL && conv != NULL && num != NULL);
        JPL_TEST_CHECK(unsup != conv && conv != num && unsup != num);
        JPL_TEST_CHECK(jpl_status_string(JPL_OK) != NULL);
        JPL_TEST_CHECK(jpl_status_string(JPL_ERROR_MEMORY) != NULL);
    }

    /* Exact low-part regression: a tick invisible to scalar addition
     * must survive add_days and come back out of subtract. */
    {
        double a[2] = {8000016.5, 0.0}, b[2], d[2];
        const double tick = 0x1p-40;
        JPL_TEST_CHECK(jpl_date_add_days(a, tick, b) == JPL_OK);
        JPL_TEST_CHECK(b[0] == a[0] && b[1] == tick);
        JPL_TEST_CHECK(jpl_date_subtract(b, a, d) == JPL_OK);
        JPL_TEST_CHECK(d[0] == tick && d[1] == 0.0);
        JPL_TEST_CHECK(jpl_date_compare(b, a) > 0);
    }

    /* Tick preservation across epochs, including negative Julian dates. */
    for (i = 0; i < 3; ++i) {
        double a[2] = {epochs[i], 0.0}, b[2], d[2];
        JPL_TEST_CHECK(jpl_date_add_days(a, DATE_TICK, b) == JPL_OK);
        CHECK_PAIR(b, epochs[i], DATE_TICK);
        JPL_TEST_CHECK(jpl_date_subtract(b, a, d) == JPL_OK);
        CHECK_PAIR(d, DATE_TICK, 0.0);
        JPL_TEST_CHECK(jpl_date_compare(b, a) > 0);
        JPL_TEST_CHECK(jpl_date_compare(a, b) < 0);
        JPL_TEST_CHECK(jpl_date_compare(a, a) == 0);
    }

    /* Cancellation of nearly equal large epochs. */
    {
        double hi[2] = {8000016.5, DATE_TICK}, lo[2] = {8000016.5, 0.0}, d[2];
        JPL_TEST_CHECK(jpl_date_subtract(hi, lo, d) == JPL_OK);
        CHECK_PAIR(d, DATE_TICK, 0.0);
    }

    /* Reversed and offset splits normalize to the same epoch. */
    {
        double r[2] = {0.0, 8000016.5}, n[2];
        JPL_TEST_CHECK(jpl_date_normalize(r, n) == JPL_OK);
        CHECK_PAIR(n, 8000016.5, 0.0);
    }
    {
        double off[2] = {2451545.0, -0.5}, n[2];
        JPL_TEST_CHECK(jpl_date_normalize(off, n) == JPL_OK);
        CHECK_PAIR(n, 2451544.5, 0.0);
    }
    {
        double split[2] = {2400000.5, 51544.5}, canon[2] = {2451545.0, 0.0}, n[2];
        JPL_TEST_CHECK(jpl_date_normalize(split, n) == JPL_OK);
        CHECK_PAIR(n, 2451545.0, 0.0);
        JPL_TEST_CHECK(jpl_date_compare(n, canon) == 0);
    }

    /* Idempotence: normalizing a normalized pair is a fixed point.
     * Nested entry points (e.g. convenience_epoch into jpl_tt_to_tdb)
     * normalize the same epoch twice in one path; this guarantees the
     * second pass is a no-op, bit for bit, including in-place calls. */
    {
        static const double cases[][2] = {
            {8000016.5, DATE_TICK},
            {2451545.0, -0.5},
            {0.0, 2451545.0},
            {1e16, 1.0},
            {DBL_MAX, 1e-10},
            {-123.25, 0.0625},
        };
        size_t c;
        for (c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
            double once[2], twice[2], inout[2];
            JPL_TEST_CHECK(jpl_date_normalize(cases[c], once) == JPL_OK);
            JPL_TEST_CHECK(jpl_date_normalize(once, twice) == JPL_OK);
            JPL_TEST_CHECK(memcmp(once, twice, sizeof once) == 0);
            memcpy(inout, cases[c], sizeof inout);
            JPL_TEST_CHECK(jpl_date_normalize(inout, inout) == JPL_OK);
            JPL_TEST_CHECK(memcmp(inout, once, sizeof once) == 0);
        }
    }

    /* Signed zero canonicalizes to +0 in both parts. */
    {
        double z[2] = {0.0, -0.0}, zn[2] = {DATE_SENTINEL, DATE_SENTINEL};
        double nz[2] = {-0.0, -0.0}, nzn[2];
        double s[2];
        JPL_TEST_CHECK(jpl_date_normalize(z, zn) == JPL_OK);
        JPL_TEST_CHECK(memcmp(zn, plus_zero, sizeof zn) == 0);
        JPL_TEST_CHECK(jpl_date_normalize(nz, nzn) == JPL_OK);
        JPL_TEST_CHECK(memcmp(nzn, plus_zero, sizeof nzn) == 0);
        JPL_TEST_CHECK(jpl_date_compare(zn, plus_zero) == 0);
        JPL_TEST_CHECK(jpl_date_add(plus_zero, nz, s) == JPL_OK);
        JPL_TEST_CHECK(memcmp(s, plus_zero, sizeof s) == 0);
    }

    /* Exact aliasing: output may reuse either input buffer. */
    {
        double x[2] = {2451545.0, 0.0}, y[2] = {1.5, 0.25};
        JPL_TEST_CHECK(jpl_date_add(x, y, x) == JPL_OK);
        CHECK_PAIR(x, 2451546.75, 0.0);
        JPL_TEST_CHECK(jpl_date_subtract(x, y, y) == JPL_OK);
        CHECK_PAIR(y, 2451545.0, 0.0);
    }
    {
        double w[2] = {0.0, 2451545.0};
        JPL_TEST_CHECK(jpl_date_normalize(w, w) == JPL_OK);
        CHECK_PAIR(w, 2451545.0, 0.0);
    }
    {
        double x[2] = {2451545.0, 0.0};
        JPL_TEST_CHECK(jpl_date_add_days(x, 0.5, x) == JPL_OK);
        CHECK_PAIR(x, 2451545.5, 0.0);
    }
    {
        double d[2] = {2451545.0, 0.0};
        JPL_TEST_CHECK(jpl_date_add_scaled(d, 1.0, 2, d) == JPL_OK);
        CHECK_PAIR(d, 2451547.0, 0.0);
    }
    {
        double beg[2] = {2451545.0, 0.0}, end[2] = {2451546.0, 0.0};
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 4, 2, beg) == JPL_OK);
        CHECK_PAIR(beg, 2451545.5, 0.0);
    }

    /* Scaled-duration addition: exact and low-part steps. */
    {
        double date[2] = {2451545.0, 0.0}, out[2];
        JPL_TEST_CHECK(jpl_date_add_scaled(date, 1.0, 3, out) == JPL_OK);
        CHECK_PAIR(out, 2451548.0, 0.0);
        JPL_TEST_CHECK(jpl_date_add_scaled(date, 1.0, 0, out) == JPL_OK);
        CHECK_PAIR(out, 2451545.0, 0.0);
    }
    {
        double date[2] = {8000016.5, 0.0}, out[2];
        JPL_TEST_CHECK(jpl_date_add_scaled(date, DATE_TICK, 1, out) == JPL_OK);
        CHECK_PAIR(out, 8000016.5, DATE_TICK);
    }
    {
        /* 2^53 is exactly representable and allowed; 2^53+1 is not. */
        double date[2] = {2451545.0, 0.0}, out[2];
        size_t exact = (size_t)UINT64_C(9007199254740992);
        size_t over = (size_t)(UINT64_C(9007199254740992) + 1u);
        JPL_TEST_CHECK(jpl_date_add_scaled(date, 0.0, exact, out) == JPL_OK);
        CHECK_PAIR(out, 2451545.0, 0.0);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_add_scaled(date, 1.0, over, out) == JPL_ERROR_ARGUMENT);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_add_scaled(date, NAN, 1, out) == JPL_ERROR_ARGUMENT);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
    }

    /* Partitions: exact endpoint copies, midpoints, thirds, powers of two,
     * and endpoints differing only in the low part. Ordering assertions
     * use the pair via jpl_date_compare, never part[0]+part[1]. */
    {
        double beg[2] = {2451545.0, 0.0}, end[2] = {2451546.0, 0.0}, part[2];
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 4, 0, part) == JPL_OK);
        JPL_TEST_CHECK(memcmp(part, beg, sizeof part) == 0);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 4, 4, part) == JPL_OK);
        JPL_TEST_CHECK(memcmp(part, end, sizeof part) == 0);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 4, 2, part) == JPL_OK);
        CHECK_PAIR(part, 2451545.5, 0.0);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 2, 1, part) == JPL_OK);
        CHECK_PAIR(part, 2451545.5, 0.0);
    }
    {
        double beg[2] = {2451545.0, 0.0}, end[2] = {2451547.0, 0.0}, part[2];
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 2, 1, part) == JPL_OK);
        CHECK_PAIR(part, 2451546.0, 0.0);
    }
    {
        double beg[2] = {2451545.0, 0.0}, end[2] = {2451546.0, 0.0}, p1[2], p2[2];
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 3, 1, p1) == JPL_OK);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 3, 2, p2) == JPL_OK);
        JPL_TEST_CHECK(jpl_date_compare(p1, beg) > 0);
        JPL_TEST_CHECK(jpl_date_compare(p1, p2) < 0);
        JPL_TEST_CHECK(jpl_date_compare(p2, end) < 0);
    }
    {
        double beg[2] = {8000016.5, 0.0}, end[2] = {8000016.5, 2 * DATE_TICK}, part[2];
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 2, 1, part) == JPL_OK);
        CHECK_PAIR(part, 8000016.5, DATE_TICK);
        JPL_TEST_CHECK(jpl_date_compare(part, beg) > 0);
        JPL_TEST_CHECK(jpl_date_compare(part, end) < 0);
    }
    {
        /* Invalid counts and indices leave output untouched. */
        double beg[2] = {2451545.0, 0.0}, end[2] = {2451546.0, 0.0}, out[2];
        size_t big = (size_t)(UINT64_C(9007199254740992) + 1u);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 0, 0, out) == JPL_ERROR_ARGUMENT);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, 3, 4, out) == JPL_ERROR_ARGUMENT);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_partition(beg, end, big, 0, out) == JPL_ERROR_ARGUMENT);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
    }

    /* Lexicographic comparison: high part dominates the low part. */
    {
        double a[2] = {8000017.0, -1.0e10}, b[2] = {8000016.5, DATE_TICK};
        JPL_TEST_CHECK(jpl_date_compare(a, b) > 0);
        JPL_TEST_CHECK(jpl_date_compare(b, a) < 0);
    }

    /* NULL pointers report ARGUMENT. */
    {
        double good[2] = {2451545.0, 0.0}, out[2];
        JPL_TEST_CHECK(jpl_date_normalize(NULL, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_normalize(good, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add(NULL, good, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add(good, NULL, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add(good, good, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_subtract(NULL, good, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_subtract(good, NULL, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_subtract(good, good, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add_days(NULL, 1.0, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add_days(good, 1.0, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add_scaled(NULL, 1.0, 1, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_add_scaled(good, 1.0, 1, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_partition(NULL, good, 1, 0, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_partition(good, NULL, 1, 0, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_date_partition(good, good, 1, 0, NULL) == JPL_ERROR_ARGUMENT);
    }

    /* NaN/Inf in either part reports ARGUMENT and preserves outputs. */
    {
        double good[2] = {2451545.0, 0.0}, out[2];
        double bads[4][2];
        bads[0][0] = NAN; bads[0][1] = 0.0;
        bads[1][0] = 0.0; bads[1][1] = NAN;
        bads[2][0] = INFINITY; bads[2][1] = 0.0;
        bads[3][0] = 0.0; bads[3][1] = -INFINITY;
        for (k = 0; k < 4; ++k) {
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_normalize(bads[k], out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_add(bads[k], good, out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_add(good, bads[k], out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_subtract(bads[k], good, out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_subtract(good, bads[k], out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_add_days(bads[k], 1.0, out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_add_scaled(bads[k], 1.0, 1, out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_partition(bads[k], good, 2, 1, out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
            fill_sentinel(out);
            JPL_TEST_CHECK(jpl_date_partition(good, bads[k], 2, 1, out) == JPL_ERROR_ARGUMENT);
            CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        }
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_add_days(good, INFINITY, out) == JPL_ERROR_ARGUMENT);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
    }

    /* Finite inputs with unrepresentable results report NUMERIC. */
    {
        double big[2] = {DBL_MAX, 0.0}, negbig[2] = {-DBL_MAX, 0.0};
        double huge[2] = {DBL_MAX, DBL_MAX}, out[2];
        double nbig[2] = {-DBL_MAX, 0.0}, pbig[2] = {DBL_MAX, 0.0};
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_add(big, big, out) == JPL_ERROR_NUMERIC);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_add_days(big, DBL_MAX, out) == JPL_ERROR_NUMERIC);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_subtract(big, negbig, out) == JPL_ERROR_NUMERIC);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_normalize(huge, out) == JPL_ERROR_NUMERIC);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_add_scaled(big, DBL_MAX, 1, out) == JPL_ERROR_NUMERIC);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
        fill_sentinel(out);
        JPL_TEST_CHECK(jpl_date_partition(nbig, pbig, 2, 1, out) == JPL_ERROR_NUMERIC);
        CHECK_PAIR(out, DATE_SENTINEL, DATE_SENTINEL);
    }

    puts("dates tests passed");
    return 0;
}
