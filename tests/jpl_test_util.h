/* Shared mock-ephemeris builders and constants for libjpl tests.
 * Each test keeps its own record layout; only the byte-level helpers
 * and header field offsets are shared here. */
#ifndef LIBJPL_TEST_UTIL_H
#define LIBJPL_TEST_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define JPL_TEST_RECORD_BYTES 2856
#define JPL_TEST_HEADER_BYTES 2880
#define JPL_TEST_AU_KM 149597870.7
#define JPL_TEST_MASS_RATIO 81.30056
#define JPL_TEST_C_AU_PER_DAY 173.144632684
#define JPL_TEST_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static inline void test_put_u32(unsigned char *at, uint32_t v) {
    at[0] = (unsigned char)v;
    at[1] = (unsigned char)(v >> 8);
    at[2] = (unsigned char)(v >> 16);
    at[3] = (unsigned char)(v >> 24);
}

static inline void test_put_f64(unsigned char *at, double v) { memcpy(at, &v, sizeof v); }

static inline void test_put_u32be(unsigned char *at, uint32_t v) {
    at[0] = (unsigned char)(v >> 24);
    at[1] = (unsigned char)(v >> 16);
    at[2] = (unsigned char)(v >> 8);
    at[3] = (unsigned char)v;
}

static inline void test_put_f64be(unsigned char *at, double v) {
    unsigned char tmp[8];
    size_t i;
    memcpy(tmp, &v, sizeof tmp);
    for (i = 0; i < 8; ++i) at[i] = tmp[7 - i];
}

/* IPT coefficient-pointer entry: idx 0-11 addresses file offset 2696+idx*12;
 * pseudo-slot 12 addresses the DENUM+libration words at 2840 (DENUM first),
 * and pseudo-slots 13/14 address IPT entries 14/15 at 2856/2868. */
static inline void test_put_ptr(unsigned char *rec, int idx, uint32_t start, uint32_t coeff, uint32_t sub) {
    /* Programming-error trap for out-of-range slots (negative idx would
     * otherwise wrap to a huge size_t and corrupt memory). JPL_TEST_CHECK
     * cannot signal failure from a void helper, and assert() vanishes in
     * Release (NDEBUG), so fail loudly on every build flavor. */
    if (idx < 0 || idx > 14) { fprintf(stderr, "test_put_ptr: bad idx %d\n", idx); abort(); }
    size_t off = idx <= 11 ? (size_t)2696 + (size_t)idx * 12u :
                 idx == 12 ? (size_t)2840 : (size_t)2856 + (size_t)(idx - 13) * 12u;
    test_put_u32(rec + off, start);
    test_put_u32(rec + off + 4, coeff);
    test_put_u32(rec + off + 8, sub);
}

/* Standard mock header prologue: coverage, step, constant count, AU, EM ratio. */
static inline void test_mock_header(unsigned char *rec, double start_jd, double end_jd,
                             double step_days, uint32_t nconst) {
    test_put_f64(rec + 2652, start_jd);
    test_put_f64(rec + 2660, end_jd);
    test_put_f64(rec + 2668, step_days);
    test_put_u32(rec + 2676, nconst);
    test_put_f64(rec + 2680, JPL_TEST_AU_KM);
    test_put_f64(rec + 2688, JPL_TEST_MASS_RATIO);
}

/* BE variants to stop hand-rolled offsets. */
static inline void test_put_ptr_be(unsigned char *rec, int idx, uint32_t start, uint32_t coeff, uint32_t sub) {
    /* Same programming-error trap as test_put_ptr (see comment there). */
    if (idx < 0 || idx > 14) { fprintf(stderr, "test_put_ptr: bad idx %d\n", idx); abort(); }
    size_t off = idx <= 11 ? (size_t)2696 + (size_t)idx * 12u :
                 idx == 12 ? (size_t)2840 : (size_t)2856 + (size_t)(idx - 13) * 12u;
    test_put_u32be(rec + off, start);
    test_put_u32be(rec + off + 4, coeff);
    test_put_u32be(rec + off + 8, sub);
}

static inline void test_mock_header_be(unsigned char *rec, double start_jd, double end_jd,
                                       double step_days, uint32_t nconst) {
    test_put_f64be(rec + 2652, start_jd);
    test_put_f64be(rec + 2660, end_jd);
    test_put_f64be(rec + 2668, step_days);
    test_put_u32be(rec + 2676, nconst);
    test_put_f64be(rec + 2680, JPL_TEST_AU_KM);
    test_put_f64be(rec + 2688, JPL_TEST_MASS_RATIO);
}

static inline double test_norm3(const double v[3]) {
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

#endif
