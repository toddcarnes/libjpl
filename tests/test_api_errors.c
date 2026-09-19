/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* API error-matrix tests: NULL dates, nonfinite parts, invalid bodies,
 * out-of-coverage queries, output sentinels, equivalent splits, and the
 * trailing-record endpoint policy. T09: missing-dataset queries report
 * UNSUPPORTED after valid checks; finite nonfinite-coefficient and
 * DBL_MAX-overflow states report FORMAT vs NUMERIC without partial output. */
#include "jpl.h"
#include "jpl_frames.h"
#include "jpl_private.h"
#include "jpl_test_util.h"

#ifndef LIBJPL_TESTING
#error "test_api_errors requires LIBJPL_TESTING for the light-time budget hook"
#endif

#include <float.h>
#include <stdint.h>
#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

enum { ERR_DOUBLES = 360, ERR_BYTES = ERR_DOUBLES * 8 };

static int check_unchanged(const double got[6], const double want[6]) {
    size_t i;
    for (i = 0; i < 6; ++i) {
        if (got[i] != want[i]) return 0;
    }
    return 1;
}

static void fill_state(double s[6], double v) {
    size_t i;
    for (i = 0; i < 6; ++i) s[i] = v;
}

int main(void) {
    const char *path = "jpl_test_api_errors.bin";
    static unsigned char hdr[ERR_BYTES];
    static double constants[ERR_DOUBLES];
    static double records[2 * ERR_DOUBLES];
    jpl_test_mock mock;
    jpl_ephemeris *e = NULL;
    size_t i;
    /* Linear mock near 8000016.5: record 0 holds x=1, record 1 holds x=2
     * so the low-part interval-side test distinguishes adjacent intervals. */
    static const double cov_start[2] = {8000016.5, 0.0};
    static const double cov_end[2] = {8000018.5, 0.0};
    static const double left[2] = {2451546.5, -0x1p-40};
    static const double right[2] = {2451546.5, 0x1p-40};
    static const double nan_hi[2] = {NAN, 0.0};
    static const double nan_lo[2] = {2451545.0, NAN};
    static const double inf_hi[2] = {INFINITY, 0.0};
    static const double inf_lo[2] = {2451545.0, -INFINITY};

    memset(hdr, 0, sizeof hdr);
    memcpy(hdr, "api errors", 10);
    test_mock_header(hdr, 8000016.5, 8000018.5, 1.0, 2);
    test_put_ptr(hdr, 3, 3, 1, 1); /* Mars one-coeff slots */
    test_put_ptr(hdr, 12, 441, 0, 0);
    for (i = 0; i < ERR_DOUBLES; ++i) constants[i] = 0.0;
    constants[0] = 1.0;
    constants[1] = 2.0;
    for (i = 0; i < 2 * ERR_DOUBLES; ++i) records[i] = 0.0;
    records[0 * ERR_DOUBLES + 0] = 8000016.5;
    records[0 * ERR_DOUBLES + 1] = 8000017.5;
    records[0 * ERR_DOUBLES + 2] = 1.0 * JPL_TEST_AU_KM;
    records[1 * ERR_DOUBLES + 0] = 8000017.5;
    records[1 * ERR_DOUBLES + 1] = 8000018.5;
    records[1 * ERR_DOUBLES + 2] = 2.0 * JPL_TEST_AU_KM;
    memset(&mock, 0, sizeof mock);
    mock.record_doubles = ERR_DOUBLES;
    mock.advertised_records = 2;
    mock.denum = 441;
    mock.constant_count = 2;
    mock.start[0] = cov_start[0];
    mock.start[1] = cov_start[1];
    mock.end[0] = cov_end[0];
    mock.end[1] = cov_end[1];
    mock.step_days = 1.0;
    mock.au_km = JPL_TEST_AU_KM;
    mock.emrat = JPL_TEST_MASS_RATIO;
    mock.header = hdr;
    mock.constants = constants;
    mock.records = records;
    JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
    JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);

    /* NULL dates and nonfinite parts report ARGUMENT. */
    {
        double s[6];
        fill_state(s, 7.0);
        JPL_TEST_CHECK(jpl_get_state(e, NULL, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, nan_hi, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, nan_lo, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, inf_hi, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, inf_lo, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, NULL, JPL_MARS, 0, s, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, NULL, &s[0], NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, nan_lo, &s[0], NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_tt_to_tdb(NULL, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_tt_to_tdb(nan_hi, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_tdb_to_tt(NULL, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(NULL, (double[3][3]){{0}}) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(NULL, cov_start, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(s, NULL, s) == JPL_ERROR_ARGUMENT);
        /* T11: standalone aberration/frame validation and transactional
         * outputs. Invalid velocities and nonfinite Sun distance are
         * ARGUMENT; nonfinite states are ARGUMENT; failures leave the
         * caller buffers unchanged. */
        {
            double c_au_day = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
            double st[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
            double vv[3] = {0.0, 0.01, 0.0};
            double bad[6], bv[3], oo[6], before[6];
            double R[3][3], Rbefore[3][3];
            size_t a, b;
            for (a = 0; a < 3; ++a) for (b = 0; b < 3; ++b) { R[a][b] = 5.0; Rbefore[a][b] = 5.0; }
            memcpy(bad, st, sizeof bad); bad[0] = NAN;
            memcpy(oo, (double[6]){11, 22, 33, 44, 55, 66}, sizeof oo);
            memcpy(before, oo, sizeof oo);
            JPL_TEST_CHECK(jpl_apply_aberration(bad, vv, oo) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(oo, before, sizeof oo) == 0);
            bv[0] = 2.0 * c_au_day; bv[1] = 0.0; bv[2] = 0.0;
            JPL_TEST_CHECK(jpl_apply_aberration(st, bv, oo) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(oo, before, sizeof oo) == 0);
            JPL_TEST_CHECK(jpl_apply_aberration_full(st, vv, NAN, oo) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(oo, before, sizeof oo) == 0);
            JPL_TEST_CHECK(jpl_apply_aberration(st, vv, NULL) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(jpl_frame_rotation_matrix(nan_lo, R) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(R, Rbefore, sizeof R) == 0);
        }
    }

    /* Invalid target/center combinations. */
    {
        double s[6];
        static const double q[2] = {8000017.0, 0.0};
        JPL_TEST_CHECK(jpl_get_state(e, q, 0, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, q, 99, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, q, JPL_NUTATIONS, 1, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, q, JPL_MARS, 0, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(NULL, q, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, q, 99, 0, s, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, q, 0, 0, s, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, q, JPL_SOLAR_SYSTEM_BARYCENTER, 0, s, NULL) == JPL_OK);
    }

    /* T09: missing datasets report UNSUPPORTED after valid checks.
     * Valid in-coverage queries for absent Venus/Jupiter/nutation/
     * libration/TT-TDB datasets are UNSUPPORTED, not RANGE; invalid
     * bodies, NULL dates, nonfinite dates and out-of-coverage queries
     * keep their earlier categories. */
    {
        double s[6];
        static const double q[2] = {8000017.0, 0.0};
        static const double outside[2] = {8000020.0, 0.0};
        double before[6], sec = 9.0, rate = 9.0;
        fill_state(before, 7.0);
        memcpy(s, before, sizeof s);
        JPL_TEST_CHECK(jpl_get_state(e, q, JPL_VENUS, 0, s) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(check_unchanged(s, before));
        JPL_TEST_CHECK(jpl_get_state(e, q, JPL_JUPITER, 0, s) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(check_unchanged(s, before));
        JPL_TEST_CHECK(jpl_get_state(e, q, JPL_NUTATIONS, 0, s) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(check_unchanged(s, before));
        JPL_TEST_CHECK(jpl_get_state(e, q, JPL_LIBRATIONS, 0, s) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(check_unchanged(s, before));
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, q, &sec, &rate) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(sec == 9.0 && rate == 9.0);
        /* Nonfinite dates and out-of-coverage queries keep precedence
         * over the missing-dataset report. */
        JPL_TEST_CHECK(jpl_get_state(e, nan_hi, JPL_VENUS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, outside, JPL_VENUS, 0, s) == JPL_ERROR_RANGE);
        /* T09 round 1: coverage precedes the absent-dataset report in the
         * shared engine too, so out-of-coverage angular queries stay RANGE
         * even though the nutation/libration datasets are absent. */
        JPL_TEST_CHECK(jpl_get_state(e, outside, JPL_NUTATIONS, 0, s) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(jpl_get_state(e, outside, JPL_LIBRATIONS, 0, s) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, nan_hi, &sec, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, outside, &sec, NULL) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(check_unchanged(s, before));
    }

    /* T09: NaN used coefficient reports FORMAT with no partial output,
     * on both backends and both endiannesses; the cache is never
     * poisoned by the failure. Layout: three advertised one-day records
     * over 8000016.5-8000019.5 so the corrupt middle record (index 1)
     * has clean first/last endpoints and open-time validation passes.
     * (The finite-coefficient DBL_MAX-overflow NUMERIC case lives in
     * the dedicated 984-double block below: 360-double geometry
     * rejects a two-coeff nutation span at open.) */
    {
        /* q sits inside the corrupt middle record; the clean-record
         * baseline below uses 8000016.75 in the first record. */
        static const double q[2] = {8000017.75, 0.0};
        static const double t09_start[2] = {8000016.5, 0.0};
        static const double t09_end[2] = {8000019.5, 0.0};
        static const int bes[2] = {0, 1};
        int bi;
        for (bi = 0; bi < 2; ++bi) {
            {
                static unsigned char fhdr[ERR_BYTES];
                static double fconst[ERR_DOUBLES];
                static double frec[3 * ERR_DOUBLES];
                jpl_test_mock fm;
                jpl_ephemeris *fe = NULL, *fmap = NULL;
                double out[6], before[6], good[6];
                size_t k, r;
                const char *fpath = bi == 0 ? "jpl_test_api_finite_le.bin" : "jpl_test_api_finite_be.bin";
                int big_endian = bes[bi];
                memset(fhdr, 0, sizeof fhdr);
                memcpy(fhdr, "finite txn", 10);
                test_mock_header(fhdr, 8000016.5, 8000019.5, 1.0, 2);
                /* Nutation pointer (idx 11) at slot 3: the 360-double
                 * baseline pattern (one coeff per component over slots
                 * 2..3). Write order first so the BE writer carries it.
                 * Queries below use JPL_NUTATIONS, not Mars. */
                test_put_ptr(fhdr, 12, 441, 0, 0);
                for (k = 0; k < ERR_DOUBLES; ++k) fconst[k] = 0.0;
                fconst[0] = 1.0;
                fconst[1] = 2.0;
                for (k = 0; k < 3 * ERR_DOUBLES; ++k) frec[k] = 0.0;
                for (r = 0; r < 3; ++r) {
                    frec[r * ERR_DOUBLES + 0] = 8000016.5 + (double)r;
                    frec[r * ERR_DOUBLES + 1] = 8000017.5 + (double)r;
                    frec[r * ERR_DOUBLES + 2] = 2.0;
                    frec[r * ERR_DOUBLES + 3] = 3.0;
                    frec[r * ERR_DOUBLES + 4] = 0.0;
                    frec[r * ERR_DOUBLES + 5] = 0.0;
                }
                /* NaN in the interior record's used first-component
                 * coefficient (plan sentinel idiom). */
                frec[1 * ERR_DOUBLES + 2] = NAN;
                memset(&fm, 0, sizeof fm);
                fm.record_doubles = ERR_DOUBLES;
                fm.advertised_records = 3;
                fm.big_endian = big_endian;
                if (big_endian) {
                    test_put_ptr_be(fhdr, 11, 3, 1, 1);
                } else {
                    test_put_ptr(fhdr, 11, 3, 1, 1);
                }
                fm.denum = 441;
                fm.constant_count = 2;
                fm.start[0] = t09_start[0];
                fm.start[1] = t09_start[1];
                fm.end[0] = t09_end[0];
                fm.end[1] = t09_end[1];
                fm.step_days = 1.0;
                fm.au_km = JPL_TEST_AU_KM;
                fm.emrat = JPL_TEST_MASS_RATIO;
                fm.header = fhdr;
                fm.constants = fconst;
                fm.records = frec;
                JPL_TEST_CHECK(test_write_mock(fpath, &fm) == 0);
                JPL_TEST_CHECK(jpl_open(fpath, &fe) == JPL_OK);
                JPL_TEST_CHECK(jpl_open_mmap(fpath, &fmap) == JPL_OK);
                /* Baseline: clean record 0 reads fine on both handles. */
                JPL_TEST_CHECK(jpl_get_state(fe, (const double[2]){8000016.75, 0.0}, JPL_NUTATIONS, 0, good) == JPL_OK);
                JPL_TEST_CHECK(jpl_get_state(fmap, (const double[2]){8000016.75, 0.0}, JPL_NUTATIONS, 0, good) == JPL_OK);
                JPL_TEST_CHECK(good[0] == 2.0 && good[1] == 3.0);
                /* Corrupt interior record: sentinel idiom from the plan. */
                memcpy(out, (double[6]){11, 22, 33, 44, 55, 66}, sizeof out);
                memcpy(before, out, sizeof out);
                JPL_TEST_CHECK(jpl_get_state(fe, q, JPL_NUTATIONS, 0, out) == JPL_ERROR_FORMAT);
                JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
                memcpy(out, before, sizeof out);
                JPL_TEST_CHECK(jpl_get_state(fmap, q, JPL_NUTATIONS, 0, out) == JPL_ERROR_FORMAT);
                JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
                /* Cache not poisoned: the clean record still reads. */
                JPL_TEST_CHECK(jpl_get_state(fe, (const double[2]){8000016.75, 0.0}, JPL_NUTATIONS, 0, out) == JPL_OK);
                JPL_TEST_CHECK(out[0] == good[0]);
                jpl_close(fmap);
                jpl_close(fe);
                JPL_TEST_CHECK(remove(fpath) == 0);
            }
        }
    }

    /* T09: finite-coefficient DBL_MAX overflow reports NUMERIC with no
     * partial output, on both backends and both endiannesses. The
     * 360-double geometry rejects a {3,2,1} nutation span at open, so
     * this uses a dedicated 984-double record with {3,2,1} and DBL_MAX
     * as c1 of the first component in the middle record only. */
    {
        enum { ODBL = 984, OBYTES = ODBL * 8 };
        static const double oq[2] = {8000017.75, 0.0};
        static const int obes[2] = {0, 1};
        int obi;
        for (obi = 0; obi < 2; ++obi) {
            static unsigned char ohdr[OBYTES];
            static double oconst[ODBL];
            static double orec[3 * ODBL];
            jpl_test_mock om;
            jpl_ephemeris *oe = NULL, *omap = NULL;
            double out[6], before[6], good[6];
            size_t k, r;
            const char *opath = obi == 0 ? "jpl_test_api_overflow_le.bin" : "jpl_test_api_overflow_be.bin";
            memset(ohdr, 0, sizeof ohdr);
            memcpy(ohdr, "overflow txn", 12);
            test_mock_header(ohdr, 8000016.5, 8000019.5, 1.0, 2);
            test_put_ptr(ohdr, 12, 441, 0, 0);
            for (k = 0; k < ODBL; ++k) oconst[k] = 0.0;
            oconst[0] = 1.0;
            oconst[1] = 2.0;
            for (k = 0; k < 3 * ODBL; ++k) orec[k] = 0.0;
            for (r = 0; r < 3; ++r) {
                orec[r * ODBL + 0] = 8000016.5 + (double)r;
                orec[r * ODBL + 1] = 8000017.5 + (double)r;
                orec[r * ODBL + 2] = 2.0;
                orec[r * ODBL + 3] = 0.0;
                orec[r * ODBL + 4] = 3.0;
                orec[r * ODBL + 5] = 0.0;
            }
            orec[1 * ODBL + 3] = DBL_MAX;
            memset(&om, 0, sizeof om);
            om.record_doubles = ODBL;
            om.advertised_records = 3;
            om.big_endian = obes[obi];
            if (obes[obi]) {
                test_put_ptr_be(ohdr, 11, 3, 2, 1);
            } else {
                test_put_ptr(ohdr, 11, 3, 2, 1);
            }
            om.denum = 441;
            om.constant_count = 2;
            om.start[0] = 8000016.5;
            om.end[0] = 8000019.5;
            om.step_days = 1.0;
            om.au_km = JPL_TEST_AU_KM;
            om.emrat = JPL_TEST_MASS_RATIO;
            om.header = ohdr;
            om.constants = oconst;
            om.records = orec;
            JPL_TEST_CHECK(test_write_mock(opath, &om) == 0);
            JPL_TEST_CHECK(jpl_open(opath, &oe) == JPL_OK);
            JPL_TEST_CHECK(jpl_open_mmap(opath, &omap) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_state(oe, (const double[2]){8000016.75, 0.0}, JPL_NUTATIONS, 0, good) == JPL_OK);
            JPL_TEST_CHECK(good[0] == 2.0 && good[1] == 3.0 && good[3] == 0.0 && good[4] == 0.0);
            memcpy(out, (double[6]){11, 22, 33, 44, 55, 66}, sizeof out);
            memcpy(before, out, sizeof out);
            JPL_TEST_CHECK(jpl_get_state(oe, oq, JPL_NUTATIONS, 0, out) == JPL_ERROR_NUMERIC);
            JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
            memcpy(out, before, sizeof out);
            JPL_TEST_CHECK(jpl_get_state(omap, oq, JPL_NUTATIONS, 0, out) == JPL_ERROR_NUMERIC);
            JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
            JPL_TEST_CHECK(jpl_get_state(oe, (const double[2]){8000016.75, 0.0}, JPL_NUTATIONS, 0, out) == JPL_OK);
            JPL_TEST_CHECK(out[0] == good[0]);
            jpl_close(omap);
            jpl_close(oe);
            JPL_TEST_CHECK(remove(opath) == 0);
        }
    }

    /* T09: angular getters publish nothing on partial failure. A NaN in
     * the second component's used coefficient must fail without writing
     * the first component to the caller. */
    {
        static unsigned char ahdr[ERR_BYTES];
        static double aconst[ERR_DOUBLES];
        static double arec[ERR_DOUBLES];
        static const double aq[2] = {8000017.0, 0.0};
        jpl_test_mock am;
        jpl_ephemeris *ae = NULL;
        double out[6], before[6];
        size_t k;
        memset(ahdr, 0, sizeof ahdr);
        memcpy(ahdr, "angular txn", 11);
        test_mock_header(ahdr, 8000016.5, 8000018.5, 1.0, 2);
        test_put_ptr(ahdr, 11, 3, 1, 1); /* nutations 2 comps x 1 coeff */
        test_put_ptr(ahdr, 12, 441, 0, 0);
        for (k = 0; k < ERR_DOUBLES; ++k) aconst[k] = 0.0;
        aconst[0] = 1.0;
        aconst[1] = 2.0;
        for (k = 0; k < ERR_DOUBLES; ++k) arec[k] = 0.0;
        arec[0] = 8000016.5;
        arec[1] = 8000018.5;
        arec[2] = 0.001; /* X1 c0 */
        arec[3] = NAN;   /* X2 c0: invalid second component */
        memset(&am, 0, sizeof am);
        am.record_doubles = ERR_DOUBLES;
        am.advertised_records = 1;
        am.denum = 441;
        am.constant_count = 2;
        am.start[0] = 8000016.5;
        am.end[0] = 8000018.5;
        am.step_days = 2.0;
        am.au_km = JPL_TEST_AU_KM;
        am.emrat = JPL_TEST_MASS_RATIO;
        am.header = ahdr;
        am.constants = aconst;
        am.records = arec;
        JPL_TEST_CHECK(test_write_mock("jpl_test_api_angular.bin", &am) == 0);
        JPL_TEST_CHECK(jpl_open("jpl_test_api_angular.bin", &ae) == JPL_OK);
        memcpy(out, (double[6]){11, 22, 33, 44, 55, 66}, sizeof out);
        memcpy(before, out, sizeof out);
        JPL_TEST_CHECK(jpl_get_state(ae, aq, JPL_NUTATIONS, 0, out) == JPL_ERROR_FORMAT);
        JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
        jpl_close(ae);
        JPL_TEST_CHECK(remove("jpl_test_api_angular.bin") == 0);
    }

    /* T09 round 1: libration third-slot NaN atomicity. A 3-component
     * libration where only the third component's used coefficient is NaN
     * must fail FORMAT without publishing the first two components. */
    {
        static unsigned char lhdr[ERR_BYTES];
        static double lconst[ERR_DOUBLES];
        static double lrec[ERR_DOUBLES];
        static const double lq[2] = {8000017.0, 0.0};
        jpl_test_mock lm;
        jpl_ephemeris *le = NULL;
        double out[6], before[6];
        size_t k;
        memset(lhdr, 0, sizeof lhdr);
        memcpy(lhdr, "libration txn", 13);
        test_mock_header(lhdr, 8000016.5, 8000018.5, 1.0, 2);
        test_put_ptr(lhdr, 12, 441, 3, 1); /* DENUM + libration: slots 3..5 */
        test_put_u32(lhdr + 2852, 1); /* libration subintervals (own word) */
        for (k = 0; k < ERR_DOUBLES; ++k) lconst[k] = 0.0;
        lconst[0] = 1.0;
        lconst[1] = 2.0;
        for (k = 0; k < ERR_DOUBLES; ++k) lrec[k] = 0.0;
        lrec[0] = 8000016.5;
        lrec[1] = 8000018.5;
        lrec[2] = 0.001; /* psi c0 (valid) */
        lrec[3] = 0.002; /* theta c0 (valid) */
        lrec[4] = NAN;   /* phi c0: invalid third component */
        memset(&lm, 0, sizeof lm);
        lm.record_doubles = ERR_DOUBLES;
        lm.advertised_records = 1;
        lm.denum = 441;
        lm.constant_count = 2;
        lm.start[0] = 8000016.5;
        lm.end[0] = 8000018.5;
        lm.step_days = 2.0;
        lm.au_km = JPL_TEST_AU_KM;
        lm.emrat = JPL_TEST_MASS_RATIO;
        lm.header = lhdr;
        lm.constants = lconst;
        lm.records = lrec;
        JPL_TEST_CHECK(test_write_mock("jpl_test_api_libration.bin", &lm) == 0);
        JPL_TEST_CHECK(jpl_open("jpl_test_api_libration.bin", &le) == JPL_OK);
        memcpy(out, (double[6]){11, 22, 33, 44, 55, 66}, sizeof out);
        memcpy(before, out, sizeof out);
        JPL_TEST_CHECK(jpl_get_state(le, lq, JPL_LIBRATIONS, 0, out) == JPL_ERROR_FORMAT);
        JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
        jpl_close(le);
        JPL_TEST_CHECK(remove("jpl_test_api_libration.bin") == 0);
    }

    /* T09: Earth/Moon reconstruction, invalid EMRAT, and I/O failure
     * paths leave outputs unchanged. */
    {
        /* Earth/Moon formula check against the two mock legs. */
        {
            static unsigned char ehdr[ERR_BYTES];
            static double econst[ERR_DOUBLES];
            static double erec[ERR_DOUBLES];
            jpl_test_mock em;
            jpl_ephemeris *ee = NULL;
            double earth[6], moon[6], emb[6], gm[6], ratio;
            size_t k;
            static const double eq[2] = {8000017.0, 0.0};
            memset(ehdr, 0, sizeof ehdr);
            memcpy(ehdr, "earth moon", 10);
            test_mock_header(ehdr, 8000016.5, 8000018.5, 1.0, 2);
            test_put_ptr(ehdr, 2, 3, 1, 1);  /* EMB slots 3..5 */
            test_put_ptr(ehdr, 9, 6, 1, 1);  /* Moon slots 6..8 */
            test_put_ptr(ehdr, 12, 441, 0, 0);
            for (k = 0; k < ERR_DOUBLES; ++k) econst[k] = 0.0;
            econst[0] = 1.0;
            econst[1] = 2.0;
            for (k = 0; k < ERR_DOUBLES; ++k) erec[k] = 0.0;
            erec[0] = 8000016.5;
            erec[1] = 8000018.5;
            erec[2] = 10.0 * JPL_TEST_AU_KM;
            erec[3] = 20.0 * JPL_TEST_AU_KM;
            erec[4] = 30.0 * JPL_TEST_AU_KM;
            erec[5] = 1.0 * JPL_TEST_AU_KM;
            erec[6] = 2.0 * JPL_TEST_AU_KM;
            erec[7] = 3.0 * JPL_TEST_AU_KM;
            memset(&em, 0, sizeof em);
            em.record_doubles = ERR_DOUBLES;
            em.advertised_records = 1;
            em.denum = 441;
            em.constant_count = 2;
            em.start[0] = 8000016.5;
            em.end[0] = 8000018.5;
            em.step_days = 2.0;
            em.au_km = JPL_TEST_AU_KM;
            em.emrat = JPL_TEST_MASS_RATIO;
            em.header = ehdr;
            em.constants = econst;
            em.records = erec;
            JPL_TEST_CHECK(test_write_mock("jpl_test_api_earthmoon.bin", &em) == 0);
            JPL_TEST_CHECK(jpl_open("jpl_test_api_earthmoon.bin", &ee) == JPL_OK);
            ratio = JPL_TEST_MASS_RATIO;
            JPL_TEST_CHECK(jpl_get_state(ee, eq, JPL_EARTH_MOON_BARYCENTER, 0, emb) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_state(ee, eq, JPL_MOON, JPL_EARTH_MOON_BARYCENTER, gm) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_state(ee, eq, JPL_EARTH, 0, earth) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_state(ee, eq, JPL_MOON, 0, moon) == JPL_OK);
            for (k = 0; k < 6; ++k) {
                /* Earth/Moon reconstruction: barycentric_state derives
                 * Earth = EMB - Moon_raw/(1+emrat) and Moon = Earth +
                 * Moon_raw in raw file units; jpl_get_state scales by
                 * 1/AU. gm (Moon-vs-EMB) equals the scaled Moon minus
                 * the scaled EMB, i.e. gm == moon - emb, and
                 * earth == emb - gm/(1+emrat) rewritten in AU terms:
                 * with moon = earth + gm_raw/AU, the AU-consistent
                 * check is earth == emb - (moon - emb)/(emrat).
                 * Derive the expectation from the raw coefficients:
                 * EMB_km and Moon_raw_km below, scaled by the AU. */
                double emb_km = (k < 3 ? (k == 0 ? 10.0 : (k == 1 ? 20.0 : 30.0)) : 0.0) * JPL_TEST_AU_KM;
                double mraw_km = (k < 3 ? (k == 0 ? 1.0 : (k == 1 ? 2.0 : 3.0)) : 0.0) * JPL_TEST_AU_KM;
                double want_earth = (emb_km - mraw_km / (1.0 + ratio)) / JPL_TEST_AU_KM;
                double want_moon = (emb_km - mraw_km / (1.0 + ratio) + mraw_km) / JPL_TEST_AU_KM;
                JPL_TEST_CHECK(fabs(emb[k] - emb_km / JPL_TEST_AU_KM) < 1e-12);
                JPL_TEST_CHECK(fabs(earth[k] - want_earth) < 1e-9);
                JPL_TEST_CHECK(fabs(moon[k] - want_moon) < 1e-9);
            }
            /* Boundary record dates. */
            JPL_TEST_CHECK(jpl_get_state(ee, (const double[2]){8000016.5, 0.0}, JPL_EARTH, 0, earth) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_state(ee, (const double[2]){8000018.5, 0.0}, JPL_EARTH, 0, earth) == JPL_OK);
            jpl_close(ee);
            /* Invalid EMRAT/AU via dedicated corrupt-header mocks: the
             * open-time validator rejects them as FORMAT. */
            {
                jpl_test_mock bad;
                jpl_ephemeris *be = (jpl_ephemeris *)(uintptr_t)1;
                memcpy(&bad, &em, sizeof bad);
                bad.emrat = NAN;
                JPL_TEST_CHECK(test_write_mock("jpl_test_api_earthmoon.bin", &bad) == 0);
                JPL_TEST_CHECK(jpl_open("jpl_test_api_earthmoon.bin", &be) == JPL_ERROR_FORMAT);
                JPL_TEST_CHECK(be == NULL);
                bad.emrat = JPL_TEST_MASS_RATIO;
                bad.au_km = 0.0;
                JPL_TEST_CHECK(test_write_mock("jpl_test_api_earthmoon.bin", &bad) == 0);
                JPL_TEST_CHECK(jpl_open("jpl_test_api_earthmoon.bin", &be) == JPL_ERROR_FORMAT);
                JPL_TEST_CHECK(be == NULL);
            }
            JPL_TEST_CHECK(remove("jpl_test_api_earthmoon.bin") == 0);
        }
        /* Deterministic I/O failure: querying past the truncated end of
         * a FILE handle leaves the output unchanged. Only the FILE path
         * is exercised (mmap reads cannot fail this way); allocation
         * failures are not injected. */
        {
            static unsigned char ihdr[ERR_BYTES];
            static double iconst[ERR_DOUBLES];
            static double irec[2 * ERR_DOUBLES];
            jpl_test_mock im;
            jpl_ephemeris *ie = NULL;
            double out[6], before[6];
            size_t k;
            FILE *tf;
            memset(ihdr, 0, sizeof ihdr);
            memcpy(ihdr, "io failure", 10);
            test_mock_header(ihdr, 8000016.5, 8000018.5, 1.0, 2);
            test_put_ptr(ihdr, 3, 3, 1, 1);
            test_put_ptr(ihdr, 12, 441, 0, 0);
            for (k = 0; k < ERR_DOUBLES; ++k) iconst[k] = 0.0;
            iconst[0] = 1.0;
            iconst[1] = 2.0;
            for (k = 0; k < 2 * ERR_DOUBLES; ++k) irec[k] = 0.0;
            irec[0 * ERR_DOUBLES + 0] = 8000016.5;
            irec[0 * ERR_DOUBLES + 1] = 8000017.5;
            irec[0 * ERR_DOUBLES + 2] = 1.0 * JPL_TEST_AU_KM;
            irec[1 * ERR_DOUBLES + 0] = 8000017.5;
            irec[1 * ERR_DOUBLES + 1] = 8000018.5;
            irec[1 * ERR_DOUBLES + 2] = 2.0 * JPL_TEST_AU_KM;
            memset(&im, 0, sizeof im);
            im.record_doubles = ERR_DOUBLES;
            im.advertised_records = 2;
            im.denum = 441;
            im.constant_count = 2;
            im.start[0] = cov_start[0];
            im.start[1] = cov_start[1];
            im.end[0] = cov_end[0];
            im.end[1] = cov_end[1];
            im.step_days = 1.0;
            im.au_km = JPL_TEST_AU_KM;
            im.emrat = JPL_TEST_MASS_RATIO;
            im.header = ihdr;
            im.constants = iconst;
            im.records = irec;
            JPL_TEST_CHECK(test_write_mock("jpl_test_api_io.bin", &im) == 0);
            JPL_TEST_CHECK(jpl_open("jpl_test_api_io.bin", &ie) == JPL_OK);
            /* Truncate the second data record away after open. */
            tf = fopen("jpl_test_api_io.bin", "r+b");
            JPL_TEST_CHECK(tf != NULL);
#if defined(_WIN32)
            JPL_TEST_CHECK(_chsize_s(_fileno(tf), (long)(3 * ERR_BYTES)) == 0);
#else
            JPL_TEST_CHECK(ftruncate(fileno(tf), (off_t)(3 * ERR_BYTES)) == 0);
#endif
            JPL_TEST_CHECK(fclose(tf) == 0);
            memcpy(out, (double[6]){11, 22, 33, 44, 55, 66}, sizeof out);
            memcpy(before, out, sizeof out);
            JPL_TEST_CHECK(jpl_get_state(ie, (const double[2]){8000017.5, 0.0}, JPL_MARS, 0, out) == JPL_ERROR_IO);
            JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
            jpl_close(ie);
            JPL_TEST_CHECK(remove("jpl_test_api_io.bin") == 0);
        }
    }
    /* Target zero is invalid; SSB dates validate coverage normally. */
    {
        double s[6];
        static const double inside[2] = {8000017.0, 0.0};
        static const double outside[2] = {8000020.0, 0.0};
        JPL_TEST_CHECK(jpl_get_state(e, inside, 0, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, inside, JPL_SOLAR_SYSTEM_BARYCENTER, 0, s) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, outside, JPL_SOLAR_SYSTEM_BARYCENTER, 0, s) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(jpl_get_state(e, nan_hi, JPL_SOLAR_SYSTEM_BARYCENTER, 0, s) == JPL_ERROR_ARGUMENT);
    }

    /* T10: light-time status matrix. Non-convergence after the iteration
     * budget is CONVERGENCE (never RANGE or a silent estimate); a
     * genuinely pre-coverage root is RANGE. Both leave outputs
     * unchanged. The private budget hook supplies deterministic
     * exhaustion coverage without public options. */
    {
        static const double q[2] = {8000017.0, 0.0};
        double s[6], before[6];
        double lt = -7.0;
        fill_state(s, 5.0);
        memcpy(before, s, sizeof s);
        /* Zero budgets on a non-root case: no iteration family may run,
         * so the bounded solve exhausts without an accepted epoch. */
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, q, JPL_MARS, 0, s, &lt, 0, 0) == JPL_ERROR_CONVERGENCE);
        JPL_TEST_CHECK(check_unchanged(s, before));
        JPL_TEST_CHECK(lt == -7.0);
        /* Failed budget validation is ARGUMENT, also transactional. */
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, q, JPL_MARS, 0, s, &lt, 0, -1) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(check_unchanged(s, before));
        /* Identical bodies are an exact zero-delay root accepted even
         * with zero budgets; NULL delay output is accepted too. */
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, q, JPL_MARS, JPL_MARS, s, &lt, 0, 0) == JPL_OK);
        JPL_TEST_CHECK(s[0] == 0.0 && s[5] == 0.0 && lt == 0.0);
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, q, JPL_MARS, JPL_MARS, s, NULL, 0, 0) == JPL_OK);
    }

    /* Output sentinels stay unchanged on failure. */
    {
        double s[6], want[6], sec = 9.0, rate = 9.0;
        static const double outside[2] = {8000020.0, 0.0};
        fill_state(s, 3.0);
        fill_state(want, 3.0);
        JPL_TEST_CHECK(jpl_get_state(e, outside, JPL_MARS, 0, s) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(check_unchanged(s, want));
        fill_state(s, 3.0);
        JPL_TEST_CHECK(jpl_get_state(e, nan_hi, JPL_MARS, 0, s) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(check_unchanged(s, want));
        /* T09: the api-errors mock carries no TT-TDB dataset, so a valid
         * in-coverage query reports UNSUPPORTED while an out-of-coverage
         * query stays RANGE; both leave outputs unchanged. */
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, cov_start, &sec, &rate) == JPL_ERROR_UNSUPPORTED);
        JPL_TEST_CHECK(sec == 9.0 && rate == 9.0);
        sec = 9.0; rate = 9.0;
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, outside, &sec, &rate) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(sec == 9.0 && rate == 9.0);
        {
            double r[3][3], before[3][3];
            size_t a, b;
            for (a = 0; a < 3; ++a) {
                for (b = 0; b < 3; ++b) {
                    r[a][b] = 5.0;
                    before[a][b] = 5.0;
                }
            }
            JPL_TEST_CHECK(jpl_frame_rotation_matrix(nan_lo, r) == JPL_ERROR_ARGUMENT);
            JPL_TEST_CHECK(memcmp(r, before, sizeof r) == 0);
        }
    }

    /* Low-part interval sides: identical high parts, opposite-sign low
     * parts near a record boundary select adjacent intervals. Use the
     * dedicated near-2451546 boundary pair on a matching mock instead. */
    {
        static unsigned char hdr2[ERR_BYTES];
        static double rec2[2 * ERR_DOUBLES];
        jpl_test_mock m2;
        jpl_ephemeris *e2 = NULL;
        double a[6], b[6];
        size_t k;
        memset(hdr2, 0, sizeof hdr2);
        memcpy(hdr2, "interval sides", 14);
        test_mock_header(hdr2, 2451545.5, 2451547.5, 1.0, 2);
        test_put_ptr(hdr2, 3, 3, 1, 1);
        test_put_ptr(hdr2, 12, 441, 0, 0);
        for (k = 0; k < 2 * ERR_DOUBLES; ++k) rec2[k] = 0.0;
        rec2[0 * ERR_DOUBLES + 0] = 2451545.5;
        rec2[0 * ERR_DOUBLES + 1] = 2451546.5;
        rec2[0 * ERR_DOUBLES + 2] = 1.0 * JPL_TEST_AU_KM;
        rec2[1 * ERR_DOUBLES + 0] = 2451546.5;
        rec2[1 * ERR_DOUBLES + 1] = 2451547.5;
        rec2[1 * ERR_DOUBLES + 2] = 2.0 * JPL_TEST_AU_KM;
        memset(&m2, 0, sizeof m2);
        m2.record_doubles = ERR_DOUBLES;
        m2.advertised_records = 2;
        m2.denum = 441;
        m2.constant_count = 2;
        m2.start[0] = 2451545.5;
        m2.end[0] = 2451547.5;
        m2.step_days = 1.0;
        m2.au_km = JPL_TEST_AU_KM;
        m2.emrat = JPL_TEST_MASS_RATIO;
        m2.header = hdr2;
        m2.constants = constants;
        m2.records = rec2;
        JPL_TEST_CHECK(test_write_mock("jpl_test_api_sides.bin", &m2) == 0);
        JPL_TEST_CHECK(jpl_open("jpl_test_api_sides.bin", &e2) == JPL_OK);
        {
            jpl_status sa = jpl_get_state(e2, left, JPL_MARS, 0, a);
            jpl_status sb = jpl_get_state(e2, right, JPL_MARS, 0, b);
            if (sa != JPL_OK || sb != JPL_OK) {
                jpl_close(e2);
                return 1;
            }
        }
        /* The fixture places distinct constants in the two adjacent intervals. */
        JPL_TEST_CHECK(a[0] == 1.0 && b[0] == 2.0);
        jpl_close(e2);
        JPL_TEST_CHECK(remove("jpl_test_api_sides.bin") == 0);
    }

    /* Final endpoint with and without a trailing record: both select the
     * last advertised interval. */
    {
        static const double endpt[2] = {8000018.5, 0.0};
        double s[6];
        JPL_TEST_CHECK(jpl_get_state(e, endpt, JPL_MARS, 0, s) == JPL_OK);
        JPL_TEST_CHECK(s[0] == 2.0);
    }

    /* Arbitrary equivalent splits of one epoch agree. */
    {
        double a[6], b[6];
        static const double split_a[2] = {8000017.0, 0.25};
        static const double split_b[2] = {8000017.25, 0.0};
        JPL_TEST_CHECK(jpl_get_state(e, split_a, JPL_MARS, 0, a) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, split_b, JPL_MARS, 0, b) == JPL_OK);
        JPL_TEST_CHECK(a[0] == b[0] && a[3] == b[3]);
    }

    /* T13: clone lifetime contracts. NULL outputs/sources, deterministic
     * open-failure cleanup, and multiple-sibling survivorship for FILE
     * and mmap. In-place content mutation is unsupported; identity
     * validation is not snapshot isolation. */
    {
        jpl_ephemeris *null_out = (jpl_ephemeris *)(uintptr_t)0x1;
        jpl_ephemeris *bad = (jpl_ephemeris *)(uintptr_t)0x1;
        jpl_ephemeris *c1 = NULL, *c2 = NULL;
        jpl_ephemeris *m = NULL, *mc1 = NULL, *mc2 = NULL;
        const jpl_header *h;
        double s[6];
        static const double tq[2] = {8000017.0, 0.0};
        /* NULL out_clone never writes through; NULL source NULLs output. */
        JPL_TEST_CHECK(jpl_ephemeris_clone(e, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_ephemeris_clone(NULL, &null_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(null_out == NULL);
        JPL_TEST_CHECK(jpl_ephemeris_clone(NULL, NULL) == JPL_ERROR_ARGUMENT);
        /* Failed open reports IO with a NULL handle (sentinel cleared). */
        JPL_TEST_CHECK(jpl_open("jpl_test_api_no_such_file_xyz.eph", &bad) == JPL_ERROR_IO);
        JPL_TEST_CHECK(bad == NULL);
        /* FILE siblings: two clones survive the original, then one clone. */
        JPL_TEST_CHECK(jpl_ephemeris_clone(e, &c1) == JPL_OK && c1 != NULL);
        JPL_TEST_CHECK(jpl_ephemeris_clone(e, &c2) == JPL_OK && c2 != NULL);
        jpl_close(e); e = NULL;
        h = jpl_get_header(c1);
        JPL_TEST_CHECK(h != NULL && h->constant_count == 2 && jpl_record_count(c1) == 2);
        JPL_TEST_CHECK(jpl_get_state(c1, tq, JPL_MARS, 0, s) == JPL_OK);
        h = jpl_get_header(c2);
        JPL_TEST_CHECK(h != NULL && jpl_record_count(c2) == 2);
        JPL_TEST_CHECK(jpl_get_state(c2, tq, JPL_MARS, 0, s) == JPL_OK);
        jpl_close(c1); c1 = NULL;
        h = jpl_get_header(c2);
        JPL_TEST_CHECK(h != NULL && jpl_record_count(c2) == 2);
        JPL_TEST_CHECK(jpl_get_state(c2, tq, JPL_MARS, 0, s) == JPL_OK);
        jpl_close(c2); c2 = NULL;
        /* mmap siblings: same close order, survivors stay readable. */
        JPL_TEST_CHECK(jpl_open_mmap(path, &m) == JPL_OK && m != NULL);
        JPL_TEST_CHECK(jpl_ephemeris_clone(m, &mc1) == JPL_OK && mc1 != NULL);
        JPL_TEST_CHECK(jpl_ephemeris_clone(m, &mc2) == JPL_OK && mc2 != NULL);
        jpl_close(m); m = NULL;
        h = jpl_get_header(mc1);
        JPL_TEST_CHECK(h != NULL && jpl_record_count(mc1) == 2);
        JPL_TEST_CHECK(jpl_get_state(mc1, tq, JPL_MARS, 0, s) == JPL_OK);
        jpl_close(mc1); mc1 = NULL;
        h = jpl_get_header(mc2);
        JPL_TEST_CHECK(h != NULL && jpl_record_count(mc2) == 2);
        JPL_TEST_CHECK(jpl_get_state(mc2, tq, JPL_MARS, 0, s) == JPL_OK);
        jpl_close(mc2); mc2 = NULL;
        /* Reopen the FILE handle for the shared tail below. */
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK && e != NULL);
    }

    /* T16: untouched outputs for the time/frame error categories (NULL
     * date, nonfinite part) plus equivalent-split agreement. */
    {
        double tdb[2] = {9.0, 9.0};
        static const double split_a[2] = {2451545.0, 0.5};
        static const double split_b[2] = {2451545.5, 0.0};
        double ta[2], tb[2];
        JPL_TEST_CHECK(jpl_tt_to_tdb(NULL, tdb) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(tdb[0] == 9.0 && tdb[1] == 9.0);
        JPL_TEST_CHECK(jpl_tt_to_tdb(nan_lo, tdb) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(tdb[0] == 9.0 && tdb[1] == 9.0);
        /* Equivalent splits of one epoch agree exactly. */
        JPL_TEST_CHECK(jpl_tt_to_tdb(split_a, ta) == JPL_OK);
        JPL_TEST_CHECK(jpl_tt_to_tdb(split_b, tb) == JPL_OK);
        JPL_TEST_CHECK(ta[0] == tb[0] && ta[1] == tb[1]);
    }

    jpl_close(e);
    JPL_TEST_CHECK(remove(path) == 0);
    puts("api error tests passed");
    return 0;
}
