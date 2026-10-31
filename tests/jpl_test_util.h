/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
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
/* Independent exact inputs for physical expectations: kilometre
 * distance and SI light speed. Tests derive light speed as
 * C_KM_PER_SECOND * DAYSEC / AU_KM and never use a rounded c literal. */
#define JPL_TEST_C_KM_PER_SECOND 299792.458
#define JPL_TEST_DAYSEC 86400.0
/* Deliberately nonmodern file AU (km) making the modern/file-AU
 * distinction measurable in mock headers. */
#define JPL_TEST_NONMODERN_AU_KM 150000000.0
#define JPL_TEST_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

#define JPL_TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        abort(); \
    } \
} while (0)

static inline void test_put_u32(unsigned char *at, uint32_t v) {
    at[0] = (unsigned char)v;
    at[1] = (unsigned char)(v >> 8);
    at[2] = (unsigned char)(v >> 16);
    at[3] = (unsigned char)(v >> 24);
}

/* Raw double codec: doubles are IEEE-754 binary64 on the wire. Each value
 * is copied to a uint64_t bit pattern and emitted byte-by-byte with shifts,
 * so the encoding is independent of host byte order (memcpy alone would
 * preserve host order). */
static inline void test_put_f64(unsigned char *at, double v) {
    uint64_t u;
    size_t i;
    memcpy(&u, &v, sizeof u);
    for (i = 0; i < 8; ++i) at[i] = (unsigned char)(u >> (8 * i));
}

static inline void test_put_u32be(unsigned char *at, uint32_t v) {
    at[0] = (unsigned char)(v >> 24);
    at[1] = (unsigned char)(v >> 16);
    at[2] = (unsigned char)(v >> 8);
    at[3] = (unsigned char)v;
}

static inline void test_put_f64be(unsigned char *at, double v) {
    uint64_t u;
    size_t i;
    memcpy(&u, &v, sizeof u);
    for (i = 0; i < 8; ++i) at[i] = (unsigned char)(u >> (8 * (7 - i)));
}

/* IPT coefficient-pointer entry: idx 0-11 addresses file offset 2696+idx*12;
 * pseudo-slot 12 addresses the DENUM+libration words at 2840 (DENUM first),
 * and pseudo-slots 13/14 address IPT entries 14/15 at 2856/2868. */
static inline void test_put_ptr(unsigned char *rec, int idx, uint32_t start, uint32_t coeff, uint32_t sub) {
    /* Programming-error trap for out-of-range slots (negative idx would
     * otherwise wrap to a huge size_t and corrupt memory). JPL_TEST_CHECK
     * cannot signal failure from a void helper, and assert() vanishes in
     * Release (NDEBUG), so fail loudly on every build flavor. */
    JPL_TEST_ASSERT(idx >= 0 && idx <= 14);
    size_t off = idx <= 11 ? (size_t)2696 + (size_t)idx * 12u :
                 idx == 12 ? (size_t)2840 : (size_t)2856 + (size_t)(idx - 13) * 12u;
    test_put_u32(rec + off, start);
    test_put_u32(rec + off + 4, coeff);
    test_put_u32(rec + off + 8, sub);
}

/* Standard mock header prologue: coverage, step, constant count, AU, EM ratio.
 * Scalar header epochs are written as supplied; test_write_mock serializes
 * two-part start/end by summation at the wire-format boundary (the on-disk
 * scalar cannot preserve an arbitrary residual). */
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
    JPL_TEST_ASSERT(idx >= 0 && idx <= 14);
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

/* Minimum physical record width in bytes (classic 2856-byte header). Used
 * to bound record-count conversions by file geometry. */
#define JPL_TEST_MIN_RECORD_BYTES 2856

/* Configurable mock-ephemeris writer. The caller owns all
 * buffers. `header` is exactly `record_doubles * 8` bytes and carries raw
 * pointer/name mutations in final byte order; the writer overwrites only
 * the standard prologue fields (coverage, step, constant count, AU,
 * Earth-Moon ratio) and the DENUM word, in the requested byte order.
 * Header start/end are independent two-part inputs so a NaN-step
 * regression keeps otherwise-valid endpoints. `constants` is one full
 * record; `records` holds every advertised record plus the optional
 * trailing record. Data dates and coefficient values are serialized
 * verbatim so malformed-date tests stay expressible. Returns 0 on
 * success, 1 on failure (bad arguments or checked file-operation
 * failure). At the serialization boundary only, each pair is combined
 * into the required encoded scalar: the wire format cannot preserve an
 * arbitrary extra residual. */
typedef struct jpl_test_mock {
    size_t record_doubles, advertised_records;
    int trailing_record, big_endian, denum;
    uint32_t constant_count;
    double start[2], end[2], step_days, au_km, emrat;
    unsigned char *header;
    double *constants;
    double *records;
} jpl_test_mock;

static inline void test_write_f64_endian(unsigned char *at, double v, int big_endian) {
    if (big_endian) test_put_f64be(at, v);
    else test_put_f64(at, v);
}

static inline void test_write_u32_endian(unsigned char *at, uint32_t v, int big_endian) {
    if (big_endian) test_put_u32be(at, v);
    else test_put_u32(at, v);
}

static inline int test_write_mock(const char *path, const jpl_test_mock *mock) {
    unsigned char *hdr;
    FILE *f;
    size_t record_bytes, data_records, r, i;
    int ok = 1;
    double start_scalar, end_scalar;
    if (path == NULL || mock == NULL || mock->header == NULL || mock->records == NULL) return 1;
    if (mock->record_doubles == 0 || mock->record_doubles > SIZE_MAX / 8 ||
        mock->advertised_records == 0) return 1;
    record_bytes = mock->record_doubles * 8;
    if (record_bytes < JPL_TEST_MIN_RECORD_BYTES) return 1;
    data_records = mock->advertised_records + (mock->trailing_record ? (size_t)1 : (size_t)0);
    if (data_records < mock->advertised_records) return 1;
    hdr = (unsigned char *)malloc(record_bytes);
    if (hdr == NULL) return 1;
    memcpy(hdr, mock->header, record_bytes);
    start_scalar = mock->start[0] + mock->start[1];
    end_scalar = mock->end[0] + mock->end[1];
    test_write_f64_endian(hdr + 2652, start_scalar, mock->big_endian);
    test_write_f64_endian(hdr + 2660, end_scalar, mock->big_endian);
    test_write_f64_endian(hdr + 2668, mock->step_days, mock->big_endian);
    test_write_u32_endian(hdr + 2676, mock->constant_count, mock->big_endian);
    test_write_f64_endian(hdr + 2680, mock->au_km, mock->big_endian);
    test_write_f64_endian(hdr + 2688, mock->emrat, mock->big_endian);
    /* DENUM word only; the libration words beside it stay caller-owned. */
    test_write_u32_endian(hdr + 2840, (uint32_t)mock->denum, mock->big_endian);
    f = fopen(path, "wb");
    if (f == NULL) { free(hdr); return 1; }
    if (fwrite(hdr, 1, record_bytes, f) != record_bytes) ok = 0;
    free(hdr);
    for (i = 0; ok && i < mock->record_doubles; ++i) {
        unsigned char buf[8];
        double v = mock->constants != NULL ? mock->constants[i] : 0.0;
        test_write_f64_endian(buf, v, mock->big_endian);
        if (fwrite(buf, 1, sizeof buf, f) != sizeof buf) ok = 0;
    }
    for (r = 0; ok && r < data_records; ++r) {
        for (i = 0; ok && i < mock->record_doubles; ++i) {
            unsigned char buf[8];
            test_write_f64_endian(buf, mock->records[r * mock->record_doubles + i], mock->big_endian);
            if (fwrite(buf, 1, sizeof buf, f) != sizeof buf) ok = 0;
        }
    }
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : 1;
}

#endif
