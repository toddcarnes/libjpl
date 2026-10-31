/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_private.h"
#include "jpl_testing.h"
#include "jpl_test_util.h"

#include <float.h>
#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

enum { PARSER_DOUBLES = 360, PARSER_BYTES = PARSER_DOUBLES * 8 };

/* Baseline mock: header-consistent dates, two constants, and a usable
 * one-coefficient nutation pointer (slots 2..7, Chebyshev c0 only in
 * each of the two components... plus recognizable slots after the two
 * date doubles: coefficient slots carry (double)i with dates at 0/1).
 * Nutation interpolation at mid-coverage returns state[0] = 2.0,
 * state[1] = 3.0 with zero rates. */
static void baseline_header(unsigned char *hdr) {
    memset(hdr, 0, PARSER_BYTES);
    memcpy(hdr, "parser baseline", 15);
    test_mock_header(hdr, 2451545.0, 2451546.0, 1.0, 2);
    test_put_ptr(hdr, 11, 3, 1, 1); /* nutations: 1-based slot 3 -> records[2..3] */
    test_put_ptr(hdr, 12, 441, 0, 0); /* DENUM + absent libration */
}

static void baseline_buffers(double constants[PARSER_DOUBLES], double records[PARSER_DOUBLES]) {
    size_t i;
    for (i = 0; i < PARSER_DOUBLES; ++i) constants[i] = 0.0;
    constants[0] = 11.0;
    constants[1] = 22.0;
    for (i = 0; i < PARSER_DOUBLES; ++i) records[i] = 0.0;
    records[0] = 2451545.0;
    records[1] = 2451546.0;
    records[2] = 2.0;
    records[3] = 3.0;
}

static int write_baseline(const char *path, jpl_test_mock *mock, unsigned char *hdr,
                          double constants[PARSER_DOUBLES], double records[PARSER_DOUBLES]) {
    baseline_header(hdr);
    baseline_buffers(constants, records);
    memset(mock, 0, sizeof *mock);
    mock->record_doubles = PARSER_DOUBLES;
    mock->advertised_records = 1;
    mock->denum = 441;
    mock->constant_count = 2;
    mock->start[0] = 2451545.0;
    mock->end[0] = 2451546.0;
    mock->step_days = 1.0;
    mock->au_km = JPL_TEST_AU_KM;
    mock->emrat = JPL_TEST_MASS_RATIO;
    mock->header = hdr;
    mock->constants = constants;
    mock->records = records;
    return test_write_mock(path, mock);
}

static int open_both_must_fail(const char *path) {
    jpl_ephemeris *e = (jpl_ephemeris *)(uintptr_t)1;
    JPL_TEST_CHECK(jpl_open(path, &e) == JPL_ERROR_FORMAT);
    JPL_TEST_CHECK(e == NULL);
    e = (jpl_ephemeris *)(uintptr_t)1;
    JPL_TEST_CHECK(jpl_open_mmap(path, &e) == JPL_ERROR_FORMAT);
    JPL_TEST_CHECK(e == NULL);
    return 0;
}

static int check_constant_names(const char *path, int big_endian) {
    enum { NCONST = 645, RDBL = 1018, RBYTES = RDBL * 8 };
    static unsigned char hdr[RBYTES];
    static double constants[RDBL], records[RDBL];
    jpl_test_mock mock;
    jpl_ephemeris *e = NULL, *m = NULL, *clone = NULL;
    const jpl_ephemeris *handles[3];
    char name[JPL_CONSTANT_NAME_LENGTH + 1];
    size_t i;

    memset(hdr, 0, sizeof hdr);
    memset(constants, 0, sizeof constants);
    memset(records, 0, sizeof records);
    memcpy(hdr + 252, "FIRST ", 6);
    memcpy(hdr + 252 + 399 * 6, "EDGE39", 6);
    memcpy(hdr + 2856, "OVER00", 6);
    memcpy(hdr + 2856 + 6, "OVER01", 6);
    memcpy(hdr + 2856 + (NCONST - 401) * 6, "FINAL ", 6);
    constants[NCONST - 1] = 42.0;
    records[0] = 2451545.0;
    records[1] = 2451546.0;
    memset(&mock, 0, sizeof mock);
    mock.record_doubles = RDBL;
    mock.advertised_records = 1;
    mock.big_endian = big_endian;
    mock.denum = 440;
    mock.constant_count = NCONST;
    mock.start[0] = 2451545.0;
    mock.end[0] = 2451546.0;
    mock.step_days = 1.0;
    mock.au_km = JPL_TEST_AU_KM;
    mock.emrat = JPL_TEST_MASS_RATIO;
    mock.header = hdr;
    mock.constants = constants;
    mock.records = records;
    JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
    JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
    JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
    JPL_TEST_CHECK(jpl_ephemeris_clone(e, &clone) == JPL_OK);
    handles[0] = e;
    handles[1] = m;
    handles[2] = clone;
    for (i = 0; i < 3; ++i) {
        double values[NCONST];
        JPL_TEST_CHECK(jpl_get_header(handles[i])->constant_count == NCONST);
        JPL_TEST_CHECK(strcmp(jpl_get_header(handles[i])->constant_names[399], "EDGE39") == 0);
        JPL_TEST_CHECK(jpl_read_constants(handles[i], values, NCONST) == JPL_OK);
        JPL_TEST_CHECK(values[NCONST - 1] == 42.0);
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], 0, name) == JPL_OK && strcmp(name, "FIRST") == 0);
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], 399, name) == JPL_OK && strcmp(name, "EDGE39") == 0);
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], 400, name) == JPL_OK && strcmp(name, "OVER00") == 0);
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], 401, name) == JPL_OK && strcmp(name, "OVER01") == 0);
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], NCONST - 1, name) == JPL_OK && strcmp(name, "FINAL") == 0);
        strcpy(name, "KEEP");
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], NCONST, name) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(strcmp(name, "KEEP") == 0);
        JPL_TEST_CHECK(jpl_get_constant_name(handles[i], SIZE_MAX, name) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(strcmp(name, "KEEP") == 0);
    }
    JPL_TEST_CHECK(jpl_get_constant_name(NULL, 0, name) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(strcmp(name, "KEEP") == 0);
    JPL_TEST_CHECK(jpl_get_constant_name(e, 0, NULL) == JPL_ERROR_ARGUMENT);
    jpl_close(clone);
    jpl_close(m);
    jpl_close(e);
    return 0;
}

/* Optional-tail policy on a 360-double record. The raw T15 words
 * at byte 2868 are tentative until geometry is known: anything but a
 * fully positive, checked, in-record span means absent, and the
 * classic file stays readable with usable states. Words are passed as
 * uint32 so unrepresentable (> INT_MAX) encodings stay expressible.
 * The equality case {358,3,1} (end 360 == width 360) is the documented
 * boundary: the final subinterval is fully populated, so an in-coverage
 * query succeeds (see the boundary-present TT-TDB test in
 * test_tt_tdb.c). The {359,3,1} case below (end 361 > width 360) is the
 * genuinely out-of-record triple that must read as absent. */
static int check_360_t15_absent(const char *path, uint32_t s, uint32_t c, uint32_t l) {
    static unsigned char h360[PARSER_BYTES];
    static double const360[PARSER_DOUBLES];
    static double rec360[PARSER_DOUBLES];
    jpl_test_mock m360;
    jpl_ephemeris *e = NULL, *m = NULL;
    double got[2], state[6];
    size_t i;
    memset(h360, 0, sizeof h360);
    memcpy(h360, "t06 opt tail", 12);
    test_mock_header(h360, 2451545.0, 2451546.0, 1.0, 2);
    test_put_ptr(h360, 3, 3, 1, 1); /* Mars: 1-based slot 3 */
    test_put_ptr(h360, 12, 441, 0, 0); /* DENUM + absent libration */
    test_put_u32(h360 + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET, s);
    test_put_u32(h360 + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET + 4, c);
    test_put_u32(h360 + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET + 8, l);
    for (i = 0; i < PARSER_DOUBLES; ++i) const360[i] = 0.0;
    const360[0] = 11.0;
    const360[1] = 22.0;
    for (i = 0; i < PARSER_DOUBLES; ++i) rec360[i] = 0.0;
    rec360[0] = 2451545.0;
    rec360[1] = 2451546.0;
    rec360[2] = 5.0;
    rec360[3] = 6.0;
    rec360[4] = 7.0;
    memset(&m360, 0, sizeof m360);
    m360.record_doubles = PARSER_DOUBLES;
    m360.advertised_records = 1;
    m360.denum = 441;
    m360.constant_count = 2;
    m360.start[0] = 2451545.0;
    m360.end[0] = 2451546.0;
    m360.step_days = 1.0;
    m360.au_km = JPL_TEST_AU_KM;
    m360.emrat = JPL_TEST_MASS_RATIO;
    m360.header = h360;
    m360.constants = const360;
    m360.records = rec360;
    JPL_TEST_CHECK(test_write_mock(path, &m360) == 0);
    JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
    JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
    JPL_TEST_CHECK(jpl_get_header(e)->record_double_count == 360);
    JPL_TEST_CHECK(jpl_get_header(e)->has_tt_tdb == 0);
    JPL_TEST_CHECK(jpl_get_header(m)->record_double_count == 360);
    JPL_TEST_CHECK(jpl_get_header(m)->has_tt_tdb == 0);
    JPL_TEST_CHECK(jpl_record_count(m) == jpl_record_count(e));
    JPL_TEST_CHECK(jpl_has_tt_tdb(m) == 0);
    JPL_TEST_CHECK(jpl_read_constants(m, got, 2) == JPL_OK);
    JPL_TEST_CHECK(got[0] == 11.0 && got[1] == 22.0);
    JPL_TEST_CHECK(jpl_has_tt_tdb(e) == 0);
    JPL_TEST_CHECK(jpl_read_constants(e, got, 2) == JPL_OK);
    JPL_TEST_CHECK(got[0] == 11.0 && got[1] == 22.0);
    JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
    JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
    JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
    JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
    jpl_close(m);
    jpl_close(e);
    return 0;
}

int main(void) {
    static unsigned char hdr[PARSER_BYTES];
    static double constants[PARSER_DOUBLES], records[PARSER_DOUBLES];
    jpl_test_mock mock;
    const char *path = "jpl_test_parser.bin";

    /* Baseline write/open/read: the writer produces a readable file and
     * both opens decode it. */
    /* DENUM 441 classifies as ICRF through both open paths; the big-endian
     * DENUM-102 case below also checks dynamical-1950 classification. */
    {
        jpl_ephemeris *e = NULL, *m = NULL;
        const jpl_header *h;
        double got[2], state[6], rec[PARSER_DOUBLES];
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->constant_count == 2 &&
                       h->record_double_count == PARSER_DOUBLES &&
                       jpl_record_count(e) == 1);
        JPL_TEST_CHECK(h->ephemeris_number == 441);
        JPL_TEST_CHECK(h->reference_frame == JPL_FRAME_ICRF);
        JPL_TEST_CHECK(jpl_get_header(m)->reference_frame == JPL_FRAME_ICRF);
        JPL_TEST_CHECK(jpl_read_constants(e, got, 2) == JPL_OK);
        JPL_TEST_CHECK(got[0] == 11.0 && got[1] == 22.0);
        JPL_TEST_CHECK(jpl_read_record(e, 0, rec, PARSER_DOUBLES) == JPL_OK);
        JPL_TEST_CHECK(rec[0] == 2451545.0 && rec[1] == 2451546.0 && rec[2] == 2.0);
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 2.0 && state[1] == 3.0 && state[3] == 0.0);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 2.0 && state[1] == 3.0);
        jpl_close(m);
        jpl_close(e);
    }

    /* Table-driven header/param mutations: step, AU/EMRAT, dimensions,
     * and huge count ratios must all fail on both opens with NULL
     * handles. Endpoints stay valid so the mutated field is exercised. */
    {
        static const double steps[] = { NAN, INFINITY, 0.0, -1.0, 0x1p-1000 };
        static const double phys[] = { NAN, INFINITY, -INFINITY, 0.0, -1.0 };
        size_t i;
        for (i = 0; i < sizeof steps / sizeof steps[0]; ++i) {
            JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
            mock.step_days = steps[i];
            JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
            JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        }
        for (i = 0; i < sizeof phys / sizeof phys[0]; ++i) {
            JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
            mock.au_km = phys[i];
            JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
            JPL_TEST_CHECK(open_both_must_fail(path) == 0);
            JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
            mock.emrat = phys[i];
            JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
            JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        }
        /* Reversed header dates. */
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        mock.start[0] = 2451546.0;
        mock.end[0] = 2451545.0;
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        /* Nonfinite endpoints. */
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        mock.start[0] = NAN;
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        /* Degenerate pointer: positive start with zero coefficients.
         * An absent classic dataset is legal, so assert the file stays
         * readable rather than forcing FORMAT. */
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        test_put_ptr(hdr, 11, 2, 0, 1);
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        {
            jpl_ephemeris *e = NULL;
            JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
            jpl_close(e);
        }
        /* Huge count ratio: tiny step implies far more records than the
         * file geometry can hold. */
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        mock.step_days = 1e-12;
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        /* Bad dimensions: oversized coefficient count, subinterval count,
         * and start index each push the pointer span end past any record
         * the file geometry can hold, failing the span guards on both
         * opens (parse-time product/end checks plus layout validation). */
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        test_put_ptr(hdr, 11, 3, 2147483647u, 1);
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        test_put_ptr(hdr, 11, 3, 1, 2147483647u);
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        test_put_ptr(hdr, 11, 2147483647u, 1, 1);
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
    }

    /* Finite-query and NaN-query checks against a valid file. */
    {
        jpl_ephemeris *e = NULL;
        double state[6] = {9.0, 9.0, 9.0, 9.0, 9.0, 9.0};
        static const double q_ok[2] = {2451545.5, 0.0};
        static const double q_nan[2] = {NAN, 0.0};
        static const double q_inf[2] = {INFINITY, 0.0};
        static const double q_lo[2] = {2451545.5 - 10.0, 0.0};
        static const double q_nonfinite_lo[2] = {2451545.5, NAN};
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, q_ok, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 2.0);
        JPL_TEST_CHECK(jpl_get_state(e, q_nan, JPL_NUTATIONS, 0, state) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, q_inf, JPL_NUTATIONS, 0, state) == JPL_ERROR_ARGUMENT);
        /* Nonfinite low part is ARGUMENT too, not RANGE. */
        JPL_TEST_CHECK(jpl_get_state(e, q_nonfinite_lo, JPL_NUTATIONS, 0, state) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, q_lo, JPL_NUTATIONS, 0, state) == JPL_ERROR_RANGE);
        /* Normalized pair comparisons retain a sub-ULP low part when
         * deciding whether a query lies inside coverage. */
        { static const double cov_start[2] = {2451545.0, 0.0};
          static const double cov_end[2] = {2451546.0, 0.0};
          static const double inside[2] = {2451545.0, 0x1p-40};
          JPL_TEST_CHECK(jpl_date_compare(inside, cov_start) > 0);
          JPL_TEST_CHECK(jpl_date_compare(inside, cov_end) < 0); }
        jpl_close(e);
    }

    /* NaN record-start in an otherwise valid file: open must fail
     * (FORMAT) since no clean covered endpoint anchors the layout; the
     * lazy FORMAT path is covered by the interior-corrupt test below,
     * where first/last records are clean. */
    {
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        records[0] = NAN;
        records[1] = 2451546.0;
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
    }

    /* Interior corrupt record in a three-record file: open-time
     * first/last checks pass, lazy-read validation catches record 1. */
    {
        static double three[3 * PARSER_DOUBLES];
        jpl_ephemeris *e = NULL, *m = NULL;
        double state[6], rec[PARSER_DOUBLES];
        size_t k;
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        mock.start[0] = 2451545.0;
        mock.end[0] = 2451548.0;
        mock.advertised_records = 3;
        mock.records = three;
        for (k = 0; k < 3; ++k) {
            for (size_t i = 0; i < PARSER_DOUBLES; ++i) three[k * PARSER_DOUBLES + i] = 0.0;
            three[k * PARSER_DOUBLES + 0] = 2451545.0 + (double)k;
            three[k * PARSER_DOUBLES + 1] = 2451546.0 + (double)k;
            three[k * PARSER_DOUBLES + 2] = 2.0;
            three[k * PARSER_DOUBLES + 3] = 3.0;
        }
        three[1 * PARSER_DOUBLES + 0] = NAN; /* interior corrupt start */
        JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_record_count(e) == 3);
        JPL_TEST_CHECK(jpl_read_record(e, 0, rec, PARSER_DOUBLES) == JPL_OK);
        JPL_TEST_CHECK(rec[0] == 2451545.0);
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451546.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_ERROR_FORMAT);
        jpl_close(e);
        /* Same file via mmap: interior corruption also surfaces lazily. */
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        JPL_TEST_CHECK(jpl_record_count(m) == 3);
        JPL_TEST_CHECK(jpl_read_record(m, 0, rec, PARSER_DOUBLES) == JPL_OK);
        JPL_TEST_CHECK(rec[0] == 2451545.0);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451546.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_ERROR_FORMAT);
        jpl_close(m);
    }

    /* A 360-double mock with one covered interval plus a trailing record
     * also admits a 480-double file-size divisor. Complete candidate
     * validation must select 360 doubles so constants use the right offset. */
    {
        static unsigned char hdr360[PARSER_BYTES];
        static double const360[PARSER_DOUBLES];
        static double rec360[2 * PARSER_DOUBLES];
        jpl_test_mock m360;
        jpl_ephemeris *e = NULL, *m = NULL;
        double got[2], state[6];
        size_t r, i;
        memset(hdr360, 0, sizeof hdr360);
        memcpy(hdr360, "trailing 360", 12);
        test_mock_header(hdr360, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(hdr360, 3, 3, 1, 1); /* Mars: 1-based slot 3 */
        test_put_ptr(hdr360, 12, 441, 0, 0); /* DENUM + absent libration */
        for (i = 0; i < PARSER_DOUBLES; ++i) const360[i] = 0.0;
        const360[0] = 11.0;
        const360[1] = 22.0;
        for (r = 0; r < 2; ++r) {
            for (i = 0; i < PARSER_DOUBLES; ++i) rec360[r * PARSER_DOUBLES + i] = 0.0;
            rec360[r * PARSER_DOUBLES + 0] = 2451545.0 + (double)r;
            rec360[r * PARSER_DOUBLES + 1] = 2451546.0 + (double)r;
            /* Covered vs trailing coefficients must differ so the
             * final-endpoint query proves which record was selected. */
            rec360[r * PARSER_DOUBLES + 2] = (r == 0) ? 5.0 : 99.0;
            rec360[r * PARSER_DOUBLES + 3] = 6.0;
            rec360[r * PARSER_DOUBLES + 4] = 7.0;
        }
        memset(&m360, 0, sizeof m360);
        m360.record_doubles = PARSER_DOUBLES;
        m360.advertised_records = 1;
        m360.trailing_record = 1;
        m360.denum = 441;
        m360.constant_count = 2;
        m360.start[0] = 2451545.0;
        m360.end[0] = 2451546.0;
        m360.step_days = 1.0;
        m360.au_km = JPL_TEST_AU_KM;
        m360.emrat = JPL_TEST_MASS_RATIO;
        m360.header = hdr360;
        m360.constants = const360;
        m360.records = rec360;
        JPL_TEST_CHECK(test_write_mock(path, &m360) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_header(e)->record_double_count == 360);
        JPL_TEST_CHECK(jpl_get_header(m)->record_double_count == 360);
        JPL_TEST_CHECK(jpl_record_count(e) == 2);
        JPL_TEST_CHECK(jpl_record_count(m) == 2);
        JPL_TEST_CHECK(jpl_read_constants(e, got, 2) == JPL_OK);
        JPL_TEST_CHECK(got[0] == 11.0 && got[1] == 22.0);
        JPL_TEST_CHECK(jpl_read_constants(m, got, 2) == JPL_OK);
        JPL_TEST_CHECK(got[0] == 11.0 && got[1] == 22.0);
        /* Mars {3,1,1}: one coefficient per component, so the state
         * is the c0 value scaled by 1/AU. */
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM && state[3] == 0.0);
        JPL_TEST_CHECK(state[1] == 6.0 / JPL_TEST_AU_KM && state[2] == 7.0 / JPL_TEST_AU_KM);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM && state[3] == 0.0);
        /* The exact coverage end selects the covered interval, never
         * the physical trailing record. */
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451546.0, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451546.0, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
        jpl_close(m);
        jpl_close(e);
    }

    /* A header-only 2880-byte file must fail on both opens. */
    {
        FILE *f;
        JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
        f = fopen(path, "r+b");
        JPL_TEST_CHECK(f != NULL);
#if defined(_WIN32)
        JPL_TEST_CHECK(_chsize_s(_fileno(f), PARSER_BYTES) == 0);
#else
        JPL_TEST_CHECK(ftruncate(fileno(f), (off_t)PARSER_BYTES) == 0);
#endif
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
    }

    /* A classic 2856-byte-record file must succeed without
     * reading extension bytes from its constants record. The constants
     * record opens with raw words {0,0,0,1000,1,1}, which would decode
     * as a positive TT-TDB triple {1000,1,1} if misread as a header
     * tail at byte 2856; the validated geometry must ignore it. */    {
        enum { C357 = 357, CB357 = C357 * 8 };
        static unsigned char h357[CB357];
        static double c357[C357], d357[C357];
        jpl_test_mock m357;
        jpl_ephemeris *e = NULL, *m = NULL;
        const jpl_header *h;
        FILE *f;
        size_t i;
        uint64_t b1;
        double got[2], state[6];
        memset(h357, 0, sizeof h357);
        memcpy(h357, "classic 2856", 12);
        test_put_ptr(h357, 11, 3, 1, 1); /* nutations */
        test_put_ptr(h357, 12, 441, 0, 0);
        for (i = 0; i < C357; ++i) c357[i] = 0.0;
        for (i = 0; i < C357; ++i) d357[i] = 0.0;
        d357[0] = 2451545.0;
        d357[1] = 2451546.0;
        d357[2] = 2.0;
        d357[3] = 3.0;
        memset(&m357, 0, sizeof m357);
        m357.record_doubles = C357;
        m357.advertised_records = 1;
        m357.denum = 441;
        m357.constant_count = 2;
        m357.start[0] = 2451545.0;
        m357.end[0] = 2451546.0;
        m357.step_days = 1.0;
        m357.au_km = JPL_TEST_AU_KM;
        m357.emrat = JPL_TEST_MASS_RATIO;
        m357.header = h357;
        m357.constants = c357;
        m357.records = d357;
        JPL_TEST_CHECK(test_write_mock(path, &m357) == 0);
        /* Overwrite the constants record's first 24 bytes with the raw
         * triple {0,0,0 / 1000,1,1}: doubles[0] = 0, doubles[1] carries
         * hi-word 1000, doubles[2] carries words {1,1}. */
        f = fopen(path, "r+b");
        JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fseek(f, CB357, SEEK_SET) == 0);
        {
            unsigned char trap[24] = {0};
            test_put_u32(trap + 12, 1000u);
            test_put_u32(trap + 16, 1u);
            test_put_u32(trap + 20, 1u);
            JPL_TEST_CHECK(fwrite(trap, 1, sizeof trap, f) == sizeof trap);
        }
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->record_double_count == C357);
        JPL_TEST_CHECK(jpl_record_count(e) == 1);
        JPL_TEST_CHECK(h->has_tt_tdb == 0);
        JPL_TEST_CHECK(jpl_read_constants(e, got, 2) == JPL_OK);
        memcpy(&b1, got + 1, sizeof b1);
        JPL_TEST_CHECK(got[0] == 0.0 && b1 == ((uint64_t)1000u << 32));
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 2.0 && state[1] == 3.0);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 2.0 && state[1] == 3.0);
        jpl_close(m);
        jpl_close(e);
    }

    /* A numeric constant equal to the record start date is valid data,
     * not evidence of a phantom layout. One-record little-endian Mars mock
     * with constant_count = 2, constants[0] == start_jd, valid distinct
     * record endpoints and coefficients: both opens must succeed, constants
     * must preserve the equal value, and state interpolation must succeed. */
    {
        static unsigned char hcol[PARSER_BYTES];
        static double ccol[PARSER_DOUBLES], rcol[PARSER_DOUBLES];
        jpl_test_mock mcol;
        jpl_ephemeris *e = NULL, *m = NULL;
        double got[2], state[6];
        size_t i;
        memset(hcol, 0, sizeof hcol);
        memcpy(hcol, "constants collision", 19);
        test_mock_header(hcol, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(hcol, 3, 3, 1, 1); /* Mars: 1-based slot 3 */
        test_put_ptr(hcol, 12, 441, 0, 0);
        for (i = 0; i < PARSER_DOUBLES; ++i) ccol[i] = 0.0;
        ccol[0] = 2451545.0;
        ccol[1] = 22.0;
        for (i = 0; i < PARSER_DOUBLES; ++i) rcol[i] = 0.0;
        rcol[0] = 2451545.0;
        rcol[1] = 2451546.0;
        rcol[2] = 5.0;
        rcol[3] = 6.0;
        rcol[4] = 7.0;
        memset(&mcol, 0, sizeof mcol);
        mcol.record_doubles = PARSER_DOUBLES;
        mcol.advertised_records = 1;
        mcol.denum = 441;
        mcol.constant_count = 2;
        mcol.start[0] = 2451545.0;
        mcol.end[0] = 2451546.0;
        mcol.step_days = 1.0;
        mcol.au_km = JPL_TEST_AU_KM;
        mcol.emrat = JPL_TEST_MASS_RATIO;
        mcol.header = hcol;
        mcol.constants = ccol;
        mcol.records = rcol;
        JPL_TEST_CHECK(test_write_mock(path, &mcol) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        JPL_TEST_CHECK(jpl_read_constants(e, got, 2) == JPL_OK);
        JPL_TEST_CHECK(got[0] == 2451545.0 && got[1] == 22.0);
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
        jpl_close(m);
        jpl_close(e);
    }

    /* Large-epoch rounding regression. A valid mock near JD 80000000
     * with a non-binary step must open via FILE and mmap when its scheduled
     * data endpoints are independently encoded as binary64; perturbing one
     * endpoint by at least eight local ULPs must fail both opens. Before
     * the fix, the valid mock fails under the fixed modern-epoch
     * tolerance. The coverage span is a whole number of steps (3 x 1/3
     * day) so the serialize-boundary sum stays exact in the count check;
     * the scheduled endpoint (start + 1/3) itself is inexact in binary64,
     * which is the rounding under test. */
    {
        static const double big_start = 80000000.0;
        static const double big_step = 1.0 / 3.0;
        static unsigned char hbig[PARSER_BYTES];
        static double cbig[PARSER_DOUBLES];
        jpl_test_mock mbig;
        jpl_ephemeris *e = NULL, *m = NULL;
        double enc_end, ulp, bad_end;
        size_t i;
        memset(hbig, 0, sizeof hbig);
        memcpy(hbig, "large epoch", 11);
        test_mock_header(hbig, big_start, big_start + 3.0 * big_step, big_step, 2);
        test_put_ptr(hbig, 3, 3, 1, 1); /* Mars: 1-based slot 3 */
        test_put_ptr(hbig, 12, 441, 0, 0);
        for (i = 0; i < PARSER_DOUBLES; ++i) cbig[i] = 0.0;
        cbig[0] = 11.0;
        cbig[1] = 22.0;
        /* Three one-step records: endpoints are scheduled start + k*step,
         * each independently encoded as binary64 (the writer serializes
         * verbatim, so plant the rounded values directly). */
        {
            static double bigrec[3 * PARSER_DOUBLES];
            size_t r, k;
            for (r = 0; r < 3; ++r) {
                for (k = 0; k < PARSER_DOUBLES; ++k) bigrec[r * PARSER_DOUBLES + k] = 0.0;
                bigrec[r * PARSER_DOUBLES + 0] = big_start + (double)r * big_step;
                bigrec[r * PARSER_DOUBLES + 1] = big_start + (double)(r + 1) * big_step;
                bigrec[r * PARSER_DOUBLES + 2] = 5.0;
                bigrec[r * PARSER_DOUBLES + 3] = 6.0;
                bigrec[r * PARSER_DOUBLES + 4] = 7.0;
            }
            enc_end = bigrec[1];
            memset(&mbig, 0, sizeof mbig);
            mbig.records = bigrec;
            mbig.record_doubles = PARSER_DOUBLES;
        }
        mbig.advertised_records = 3;
        mbig.denum = 441;
        mbig.constant_count = 2;
        mbig.start[0] = big_start;
        mbig.end[0] = big_start + 3.0 * big_step;
        mbig.step_days = big_step;
        mbig.au_km = JPL_TEST_AU_KM;
        mbig.emrat = JPL_TEST_MASS_RATIO;
        mbig.header = hbig;
        mbig.constants = cbig;
        /* records already points at bigrec; the negative leg below mutates
         * bigrec in place through the same pointer. */
        JPL_TEST_CHECK(test_write_mock(path, &mbig) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        jpl_close(m);
        jpl_close(e);
        e = NULL;
        m = NULL;
        /* Perturb the first record end by at least eight local ULPs: both
         * opens must now fail. nextafter() walks one ULP per call.
         * bigrec lives in the block scope above; re-derive the pointer
         * from the mock instead of touching rbig. */
        ulp = enc_end;
        for (i = 0; i < 8; ++i) ulp = nextafter(ulp, INFINITY);
        bad_end = ulp;
        JPL_TEST_CHECK(bad_end != enc_end);
        mbig.records[1] = bad_end;
        JPL_TEST_CHECK(test_write_mock(path, &mbig) == 0);
        e = (jpl_ephemeris *)(uintptr_t)1;
        m = (jpl_ephemeris *)(uintptr_t)1;
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_ERROR_FORMAT);
        JPL_TEST_CHECK(e == NULL);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_ERROR_FORMAT);
        JPL_TEST_CHECK(m == NULL);
    }

    /* Zero-constant foreign-endian coverage. A valid big-endian
     * zero-constant mock must read constants with a non-NULL one-element
     * destination through FILE and mmap, leaving the sentinel unchanged. */
    {
        static unsigned char hz[PARSER_BYTES];
        static double rz[PARSER_DOUBLES];
        jpl_test_mock mz;
        jpl_ephemeris *e = NULL, *m = NULL;
        double sentinel = 1234.5;
        size_t i;
        memset(hz, 0, sizeof hz);
        memcpy(hz, "zero constants be", 17);
        test_mock_header_be(hz, 2451545.0, 2451546.0, 1.0, 0);
        test_put_ptr_be(hz, 3, 3, 1, 1); /* Mars */
        test_put_ptr_be(hz, 12, 441, 0, 0);
        for (i = 0; i < PARSER_DOUBLES; ++i) rz[i] = 0.0;
        rz[0] = 2451545.0;
        rz[1] = 2451546.0;
        rz[2] = 5.0;
        rz[3] = 6.0;
        rz[4] = 7.0;
        memset(&mz, 0, sizeof mz);
        mz.record_doubles = PARSER_DOUBLES;
        mz.advertised_records = 1;
        mz.big_endian = 1;
        mz.denum = 441;
        mz.constant_count = 0;
        mz.start[0] = 2451545.0;
        mz.end[0] = 2451546.0;
        mz.step_days = 1.0;
        mz.au_km = JPL_TEST_AU_KM;
        mz.emrat = JPL_TEST_MASS_RATIO;
        mz.header = hz;
        mz.constants = NULL;
        mz.records = rz;
        JPL_TEST_CHECK(test_write_mock(path, &mz) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        JPL_TEST_CHECK(jpl_read_constants(e, &sentinel, 1) == JPL_OK);
        JPL_TEST_CHECK(sentinel == 1234.5);
        JPL_TEST_CHECK(jpl_read_constants(m, &sentinel, 1) == JPL_OK);
        JPL_TEST_CHECK(sentinel == 1234.5);
        jpl_close(m);
        jpl_close(e);
    }

    /* A preliminary LE reading of this BE case ([0,1], step 1, DENUM
     * 102, two constants, {3,1,1}) must succeed as big-endian after
     * full LE-candidate validation fails. */
    {
        static unsigned char hbe[PARSER_BYTES];
        static double cbe[PARSER_DOUBLES], rbe[PARSER_DOUBLES];
        jpl_test_mock mbe;
        jpl_ephemeris *e = NULL, *m = NULL;
        const jpl_header *h;
        double got[2], state[6];
        size_t i;
        memset(hbe, 0, sizeof hbe);
        memcpy(hbe, "be ambiguous", 12);
        test_put_ptr_be(hbe, 3, 3, 1, 1); /* Mars */
        for (i = 0; i < PARSER_DOUBLES; ++i) cbe[i] = 0.0;
        cbe[0] = 11.0;
        cbe[1] = 22.0;
        for (i = 0; i < PARSER_DOUBLES; ++i) rbe[i] = 0.0;
        rbe[0] = 0.0;
        rbe[1] = 1.0;
        rbe[2] = 5.0;
        rbe[3] = 6.0;
        rbe[4] = 7.0;
        memset(&mbe, 0, sizeof mbe);
        mbe.record_doubles = PARSER_DOUBLES;
        mbe.advertised_records = 1;
        mbe.big_endian = 1;
        mbe.denum = 102;
        mbe.constant_count = 2;
        mbe.start[0] = 0.0;
        mbe.end[0] = 1.0;
        mbe.step_days = 1.0;
        mbe.au_km = JPL_TEST_AU_KM;
        mbe.emrat = JPL_TEST_MASS_RATIO;
        mbe.header = hbe;
        mbe.constants = cbe;
        mbe.records = rbe;
        JPL_TEST_CHECK(test_write_mock(path, &mbe) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->byte_order == JPL_BYTE_ORDER_BIG_ENDIAN);
        JPL_TEST_CHECK(h->ephemeris_number == 102);
        JPL_TEST_CHECK(h->reference_frame == JPL_FRAME_DYNAMICAL_1950);
        JPL_TEST_CHECK(h->record_double_count == PARSER_DOUBLES);
        JPL_TEST_CHECK(jpl_record_count(e) == 1);
        JPL_TEST_CHECK(jpl_read_constants(e, got, 2) == JPL_OK);
        JPL_TEST_CHECK(got[0] == 11.0 && got[1] == 22.0);
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){0.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM && state[1] == 6.0 / JPL_TEST_AU_KM &&
                       state[2] == 7.0 / JPL_TEST_AU_KM);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){0.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM && state[1] == 6.0 / JPL_TEST_AU_KM &&
                       state[2] == 7.0 / JPL_TEST_AU_KM);
        jpl_close(m);
        jpl_close(e);
    }

    /* A deliberately ambiguous file must fail rather than select
     * whichever width validates first. Two intervals [S,S+2] with a
     * 14400-byte file admit both W=3600/extra=2 and W=2880/extra=3,
     * with matching dates planted at every candidate offset. Geometry and
     * scheduled endpoints alone admit both views, so only the split rule
     * (at most one split may validate) can reject it. */
    {
        static unsigned char amb[14400];
        FILE *f;
        const double S = 2451545.0;
        memset(amb, 0, sizeof amb);
        memcpy(amb, "ambiguous widths", 16);
        test_mock_header(amb, S, S + 2.0, 1.0, 2);
        test_put_ptr(amb, 3, 3, 1, 1); /* Mars */
        test_put_ptr(amb, 12, 441, 0, 0);
        /* Constants for the W=2880 view at [2880,5760): slots 0/1 are
         * 11/22, distinct from the data start S. */
        test_put_f64(amb + 2880, 11.0);
        test_put_f64(amb + 2888, 22.0);
        /* The W=3600 view reads its constants at [3600,7200); that region
         * holds 0.0 (memset) -- payload values never discriminate, and
         * both views validate on geometry alone -> FORMAT.
         * NOTE: the W=3600 view's constants record overlaps the W=2880
         * view's constants record tail (3600 = 2880 + 720 = slot 90):
         * keep slot 90/91 of the W=2880 constants record at 0.0 (do NOT
         * plant 11/22 there) so the W=2880 re-parse triple stays absent
         * in both views. */
        /* Endpoints for the W=2880 view: data at 5760, 8640; trailing
         * record at 11520 starts at the scheduled index-2 date (S+2). */
        test_put_f64(amb + 5760, S);
        test_put_f64(amb + 5768, S + 1.0);
        test_put_f64(amb + 8640, S + 1.0);
        test_put_f64(amb + 8648, S + 2.0);
        test_put_f64(amb + 11520, S + 2.0);
        test_put_f64(amb + 11528, S + 3.0);
        /* Endpoints for the W=3600 view: data at 7200, 10800. */
        test_put_f64(amb + 7200, S);
        test_put_f64(amb + 7208, S + 1.0);
        test_put_f64(amb + 10800, S + 1.0);
        test_put_f64(amb + 10808, S + 2.0);
        f = fopen(path, "wb");
        JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(amb, 1, sizeof amb, f) == sizeof amb);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(open_both_must_fail(path) == 0);
    }

    /* Legacy padding {1000,1,1} at the optional TT-TDB slot of a
     * 360-double record must leave the file readable with
     * has_tt_tdb == 0; likewise zeros, partial triples,
     * INT_MAX-exceeding words, overflowing products, and spans beyond
     * the record. The equality boundary {358,3,1} (end 360 == width
     * 360) is the usable present case covered by the boundary-present
     * TT-TDB test in test_tt_tdb.c. The tail-absent-at-boundary 357-double
      * (2856-byte effective width) case is covered by the
     * classic-2856 constants-trap test above, which keeps the 24-byte
     * tail outside the 2856-byte header record so {1000,1,1}
     * extension bytes in the constants record stay ignored. */
    JPL_TEST_CHECK(check_360_t15_absent(path, 1000u, 1u, 1u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 0u, 0u, 0u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 1000u, 0u, 1u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 1000u, 1u, 0u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 0u, 1u, 1u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 2147483648u, 1u, 1u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 1000u, 2147483647u, 2147483647u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 359u, 3u, 1u) == 0);
    JPL_TEST_CHECK(check_360_t15_absent(path, 2u, 173u, 14u) == 0);

    /* Wide-record regression. A valid mock with a 1 MiB record
     * width, one advertised interval, and ordinary fixed header/endpoints.
     * With the parser read limit at 2880 bytes, both opens must succeed
      * (no parser read exceeds 2880 bytes). Before the fix, full-record
      * reads under the limit were not bounded; the hook is test-only and
      * declared in include/jpl_testing.h under LIBJPL_TESTING. */
    {
        enum { WDOUBLES = 131072, WBYTES = WDOUBLES * 8 };
        static unsigned char *whdr = NULL;
        static double *wconst = NULL, *wrec = NULL;
        jpl_test_mock wm;
        jpl_ephemeris *e = NULL, *m = NULL;
        double state[6];
        whdr = (unsigned char *)calloc(1, WBYTES);
        wconst = (double *)calloc(WDOUBLES, sizeof *wconst);
        wrec = (double *)calloc(WDOUBLES, sizeof *wrec);
        JPL_TEST_CHECK(whdr != NULL && wconst != NULL && wrec != NULL);
        memcpy(whdr, "wide record 1MiB", 16);
        test_mock_header(whdr, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(whdr, 3, 3, 1, 1); /* Mars: 1-based slot 3 */
        test_put_ptr(whdr, 12, 441, 0, 0);
        wconst[0] = 11.0;
        wconst[1] = 22.0;
        wrec[0] = 2451545.0;
        wrec[1] = 2451546.0;
        wrec[2] = 5.0;
        wrec[3] = 6.0;
        wrec[4] = 7.0;
        memset(&wm, 0, sizeof wm);
        wm.record_doubles = WDOUBLES;
        wm.advertised_records = 1;
        wm.denum = 441;
        wm.constant_count = 2;
        wm.start[0] = 2451545.0;
        wm.end[0] = 2451546.0;
        wm.step_days = 1.0;
        wm.au_km = JPL_TEST_AU_KM;
        wm.emrat = JPL_TEST_MASS_RATIO;
        wm.header = whdr;
        wm.constants = wconst;
        wm.records = wrec;
        JPL_TEST_CHECK(test_write_mock(path, &wm) == 0);
        /* The parser read limit proves opening never requests the full
         * 1 MiB record. Both backends must succeed below the 2880-byte cap. */
        jpl_test_set_parser_read_limit(2880);
        {
            jpl_status se = jpl_open(path, &e);
            jpl_status sm = jpl_open_mmap(path, &m);
            jpl_test_set_parser_read_limit(0);
            JPL_TEST_CHECK(se == JPL_OK);
            JPL_TEST_CHECK(sm == JPL_OK);
        }
        JPL_TEST_CHECK(jpl_get_header(e)->record_double_count == WDOUBLES);
        JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
        JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_MARS, 0, state) == JPL_OK);
        JPL_TEST_CHECK(state[0] == 5.0 / JPL_TEST_AU_KM);
        jpl_close(m);
        jpl_close(e);
        free(wrec);
        free(wconst);
        free(whdr);
    }

    /* DENUM allowlist classification sampled through complete
     * open paths (unknown values stay readable by core). DENUM 200/202
     * map to dynamical-2000, 999 maps to unknown. */
    {
        static const struct { int denum; jpl_reference_frame frame; } cases[] = {
            {200, JPL_FRAME_DYNAMICAL_2000},
            {202, JPL_FRAME_DYNAMICAL_2000},
            {999, JPL_FRAME_UNKNOWN},
        };
        size_t c;
        for (c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
            jpl_ephemeris *e = NULL, *m = NULL;
            double state[6];
            JPL_TEST_CHECK(write_baseline(path, &mock, hdr, constants, records) == 0);
            mock.denum = cases[c].denum;
            JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
            JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
            JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_header(e)->ephemeris_number == cases[c].denum);
            JPL_TEST_CHECK(jpl_get_header(e)->reference_frame == cases[c].frame);
            JPL_TEST_CHECK(jpl_get_header(m)->reference_frame == cases[c].frame);
            /* Unknown/legacy DENUM stays readable in its native frame. */
            JPL_TEST_CHECK(jpl_get_state(e, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
            JPL_TEST_CHECK(state[0] == 2.0 && state[1] == 3.0);
            JPL_TEST_CHECK(jpl_get_state(m, (const double[2]){2451545.5, 0.0}, JPL_NUTATIONS, 0, state) == JPL_OK);
            JPL_TEST_CHECK(state[0] == 2.0 && state[1] == 3.0);
            jpl_close(m);
            jpl_close(e);
        }
    }

    JPL_TEST_CHECK(check_constant_names(path, 0) == 0);
    JPL_TEST_CHECK(check_constant_names(path, 1) == 0);
    JPL_TEST_CHECK(remove(path) == 0);
    puts("parser tests passed");
    return 0;
}
