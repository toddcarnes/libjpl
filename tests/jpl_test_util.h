/* Shared mock-ephemeris builders and constants for libjpl tests.
 * Each test keeps its own record layout; only the byte-level helpers
 * and header field offsets are shared here. */
#ifndef LIBJPL_TEST_UTIL_H
#define LIBJPL_TEST_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define JPL_TEST_RECORD_BYTES 2856
#define JPL_TEST_AU_KM 149597870.7
#define JPL_TEST_MASS_RATIO 81.30056

static void test_put_u32(unsigned char *at, uint32_t v) {
    at[0] = (unsigned char)v;
    at[1] = (unsigned char)(v >> 8);
    at[2] = (unsigned char)(v >> 16);
    at[3] = (unsigned char)(v >> 24);
}

static void test_put_f64(unsigned char *at, double v) { memcpy(at, &v, sizeof v); }

/* IPT coefficient-pointer entry: idx is the 0-based target slot. */
static void test_put_ptr(unsigned char *rec, int idx, uint32_t start, uint32_t coeff, uint32_t sub) {
    test_put_u32(rec + 2696 + idx * 12, start);
    test_put_u32(rec + 2696 + idx * 12 + 4, coeff);
    test_put_u32(rec + 2696 + idx * 12 + 8, sub);
}

/* Standard mock header prologue: coverage, step, constant count, AU, EM ratio. */
static void test_mock_header(unsigned char *rec, double start_jd, double end_jd,
                             double step_days, uint32_t nconst) {
    test_put_f64(rec + 2652, start_jd);
    test_put_f64(rec + 2660, end_jd);
    test_put_f64(rec + 2668, step_days);
    test_put_u32(rec + 2676, nconst);
    test_put_f64(rec + 2680, JPL_TEST_AU_KM);
    test_put_f64(rec + 2688, JPL_TEST_MASS_RATIO);
}

static double test_norm3(const double v[3]) {
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

#endif
