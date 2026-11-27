/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_frames.h"
#include "jpl_private.h"
#include "jpl_testing.h"
#include "jpl_test_util.h"
#include "numerical_reference.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef LIBJPL_TESTING
#error "test_lighttime requires LIBJPL_TESTING for the private budget hook"
#endif

static int write_linear_mock(const char *path, double start, double end,
                             double slope_au_per_day, double intercept_au);
static int lighttime_t10_cases(void);

int main(void) {
    jpl_state st = {0, 0, 0, 0, 0, 0};
    static const jpl_date jd_null = {2451545.0, 0.0};
    int t10_rc;
    JPL_TEST_CHECK(jpl_get_state_lighttime(NULL, &jd_null, JPL_EARTH, JPL_SUN, &st, NULL) == JPL_ERROR_ARGUMENT);
    /* Nutations/librations are angles: no light-time meaning, must reject. */
    JPL_TEST_CHECK(jpl_get_state_lighttime(NULL, &jd_null, JPL_NUTATIONS, 0, &st, NULL) == JPL_ERROR_ARGUMENT);
    /* Mock ephemeris: EMB=(1 AU,0,0), Sun=origin, zero velocities.
     * Light-time must converge to range/c with velocity from Chebyshev. */
    {
        const char *path = "jpl_test_lighttime.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const jpl_date jd = {2451545.5, 0.0};
        jpl_state geo, app;
        double lt = -1.0;
        double range, expect_lt;
        int i;
        /* Two-component coefficient entry gives small records. */
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 2, 6, 1, 1);   /* EMB: slots 6..8, one coeff */
        test_put_ptr(rec, 10, 9, 1, 1);  /* Sun: slots 9..11, one coeff */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(lc, 1, sizeof lc, f) == sizeof lc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 5 * 8, 1.0); /* EMB x = 1 AU at slot 6, rest zero */
        /* Uniform 2880-byte records: data block padded with zeros. */
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_NUTATIONS, 0, &app, &lt) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_LIBRATIONS, 0, &app, &lt) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, &geo) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, &app, &lt) == JPL_OK);
        JPL_TEST_CHECK(lt > 0.0);
        { double geo_pos[3] = {geo.x, geo.y, geo.z}; range = test_norm3(geo_pos); }
        /* Independent physical expectation: km range over SI c. The AU
         * cancels algebraically but kilometres and SI light speed are
         * the reference inputs, not a rounded c literal. */
        expect_lt = range * JPL_TEST_AU_KM / JPL_TEST_C_KM_PER_SECOND / JPL_TEST_DAYSEC;
        /* Converged light-time matches range/c of the geometric vector.
         * Zero test velocities make the retarded epoch coincide with the
         * geometric one, so apparent must equal geometric exactly here. */
        JPL_TEST_CHECK(fabs(lt - expect_lt) < 1e-12);
        for (i = 0; i < 6; ++i) JPL_TEST_CHECK(test_state_component(&app, i) == test_state_component(&geo, i));
        for (i = 0; i < 6; ++i) JPL_TEST_CHECK(isfinite(test_state_component(&app, i)));
        /* The full solution is atomic: rel_state/light_time match the
         * wrapper, observer_state matches a direct barycentric read, and
         * emission is the epoch used for rel_state (residual bounded by
         * the solver tolerance), not tdb minus delay. */
        {
            jpl_lighttime_solution sol;
            jpl_state observer;
            double jd_arr[2], emission_arr[2], diff[2];
            double elapsed;
            JPL_TEST_CHECK(jpl_get_lighttime_solution(e, &jd, JPL_EARTH_MOON_BARYCENTER,
                                                      JPL_SUN, &sol) == JPL_OK);
            JPL_TEST_CHECK(sol.light_time_days == lt);
            JPL_TEST_CHECK(sol.rel_state.x == app.x && sol.rel_state.y == app.y &&
                           sol.rel_state.z == app.z && sol.rel_state.vx == app.vx &&
                           sol.rel_state.vy == app.vy && sol.rel_state.vz == app.vz);
            JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_SUN, JPL_SSB, &observer) == JPL_OK);
            JPL_TEST_CHECK(fabs(sol.observer_state.x - observer.x) < 1e-15);
            JPL_TEST_CHECK(fabs(sol.observer_state.y - observer.y) < 1e-15);
            JPL_TEST_CHECK(fabs(sol.observer_state.z - observer.z) < 1e-15);
            JPL_TEST_CHECK(fabs(sol.observer_state.vx - observer.vx) < 1e-15);
            JPL_TEST_CHECK(fabs(sol.observer_state.vy - observer.vy) < 1e-15);
            JPL_TEST_CHECK(fabs(sol.observer_state.vz - observer.vz) < 1e-15);
            jpl_date_to_pair(&jd, jd_arr);
            jpl_date_to_pair(&sol.emission, emission_arr);
            JPL_TEST_CHECK(jpl_date_subtract(jd_arr, emission_arr, diff) == JPL_OK);
            elapsed = diff[0] + diff[1];
            JPL_TEST_CHECK(fabs(elapsed - sol.light_time_days) <= JPL_LIGHTTIME_TOL_DAYS);
        }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Nonmodern file AU: stored coefficients are kilometres, so public
     * output divides by the modern AU while the header keeps file metadata.
     * File AU 150000000 km with a 149597870.7 km coefficient reads exactly
     * x == 1 modern AU; light-time is physical (km / SI c / day).
      * Dividing by the header's file AU instead would fail x == 1. */
    {
        const char *path = "jpl_test_lighttime_au.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        const jpl_header *h = NULL;
        static const jpl_date jd = {2451545.5, 0.0};
        jpl_state st_au;
        double raw[360] = {0}, lt = -1.0, expect_lt;
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_f64(rec + JPL_HDR_AU_KM_OFFSET, JPL_TEST_NONMODERN_AU_KM); /* override file AU */
        test_put_ptr(rec, 2, 6, 1, 1);   /* EMB: slots 6..8, one coeff */
        test_put_ptr(rec, 10, 9, 1, 1);  /* Sun: slots 9..11, one coeff */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(lc, 1, sizeof lc, f) == sizeof lc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 5 * 8, JPL_TEST_AU_KM); /* one modern AU in km */
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->astronomical_unit_km == JPL_TEST_NONMODERN_AU_KM);
        JPL_TEST_CHECK(h->record_double_count <= 360);
        JPL_TEST_CHECK(jpl_read_record(e, 0, raw, h->record_double_count) == JPL_OK);
        JPL_TEST_CHECK(raw[5] == JPL_TEST_AU_KM);
        JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_EARTH_MOON_BARYCENTER, 0, &st_au) == JPL_OK);
        JPL_TEST_CHECK(st_au.x == 1.0);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_EARTH_MOON_BARYCENTER, 0, &st_au, &lt) == JPL_OK);
        expect_lt = JPL_TEST_AU_KM / JPL_TEST_C_KM_PER_SECOND / JPL_TEST_DAYSEC;
        JPL_TEST_CHECK(fabs(lt - expect_lt) < 1e-12);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* 40-AU delay regression: a rounded c literal errs by ~1.125 us
     * at 40 AU, so the 5e-12-day (~0.43 us) tolerance rejects it while the
     * exact-derived light speed passes. Expected delay is computed from
     * kilometres and SI light speed, never the production literal. */
    {
        const char *path = "jpl_test_lighttime_40au.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const jpl_date jd = {2451545.5, 0.0};
        jpl_state st40;
        double lt = -1.0, expect_lt;
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 2, 6, 1, 1);   /* EMB: slots 6..8, one coeff */
        test_put_ptr(rec, 10, 9, 1, 1);  /* Sun: slots 9..11, one coeff */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(lc, 1, sizeof lc, f) == sizeof lc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 5 * 8, 40.0 * JPL_TEST_AU_KM);
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_EARTH_MOON_BARYCENTER, 0, &st40) == JPL_OK);
        JPL_TEST_CHECK(st40.x == 40.0);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_EARTH_MOON_BARYCENTER, 0, &st40, &lt) == JPL_OK);
        expect_lt = 40.0 * JPL_TEST_AU_KM / JPL_TEST_C_KM_PER_SECOND / JPL_TEST_DAYSEC;
        JPL_TEST_CHECK(fabs(lt - expect_lt) < 5e-12);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Non-zero center velocity: apparent must hold center at reception.
     * Correct: target(jd-lt) - center(jd); old bug: target(jd-lt) - center(jd-lt). */
    {
        const char *path = "jpl_test_lighttime_vel.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        static const jpl_date jd = {2451545.5, 0.0};
        jpl_state app, tgt_lt, ctr_now, ctr_lt, expect, wrong;
        jpl_date emi_date, ctr_lt_date;
        double jd_arr[2], emi_arr[2], ctr_lt_arr[2], lt = -1.0;
        size_t i;
        jpl_date_to_pair(&jd, jd_arr);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 2, 6, 2, 1);   /* EMB: slots 6..11, two coeffs */
        test_put_ptr(rec, 10, 12, 2, 1); /* Sun: slots 12..17, two coeffs */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char lc2[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc2, 1.0); test_put_f64(lc2 + 8, 2.0);
          JPL_TEST_CHECK(fwrite(lc2, 1, sizeof lc2, f) == sizeof lc2); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        /* EMB x(t): 1 AU + linear slope 1e6 km per unit norm (~0.0134 AU/day). */
        test_put_f64(rec + 5 * 8, JPL_TEST_AU_KM);
        test_put_f64(rec + 6 * 8, 1000000.0);
        /* Sun x(t): linear slope 5e5 km per unit norm (~0.0067 AU/day). */
        test_put_f64(rec + 11 * 8, 0.0);
        test_put_f64(rec + 12 * 8, 500000.0);
        /* Uniform 2880-byte records: data block padded with zeros. */
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, &app, &lt) == JPL_OK);
        JPL_TEST_CHECK(lt > 0.0);
        /* jpl_get_state already returns AU, so no further scaling here. */
        /* Retarded dates go through the compensated helper: emission is a
         * pair, never a rounded scalar difference. */
        JPL_TEST_CHECK(jpl_date_add_days(jd_arr, -lt, emi_arr) == JPL_OK);
        jpl_pair_to_date(emi_arr, &emi_date);
        JPL_TEST_CHECK(jpl_get_state(e, &emi_date, JPL_EARTH_MOON_BARYCENTER, 0, &tgt_lt) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_SUN, 0, &ctr_now) == JPL_OK);
        JPL_TEST_CHECK(jpl_date_add_days(jd_arr, -lt, ctr_lt_arr) == JPL_OK);
        jpl_pair_to_date(ctr_lt_arr, &ctr_lt_date);
        JPL_TEST_CHECK(jpl_get_state(e, &ctr_lt_date, JPL_SUN, 0, &ctr_lt) == JPL_OK);
        expect.x = tgt_lt.x - ctr_now.x; expect.y = tgt_lt.y - ctr_now.y; expect.z = tgt_lt.z - ctr_now.z;
        expect.vx = tgt_lt.vx - ctr_now.vx; expect.vy = tgt_lt.vy - ctr_now.vy; expect.vz = tgt_lt.vz - ctr_now.vz;
        wrong.x = tgt_lt.x - ctr_lt.x; wrong.y = tgt_lt.y - ctr_lt.y; wrong.z = tgt_lt.z - ctr_lt.z;
        wrong.vx = tgt_lt.vx - ctr_lt.vx; wrong.vy = tgt_lt.vy - ctr_lt.vy; wrong.vz = tgt_lt.vz - ctr_lt.vz;
        for (i = 0; i < 6; ++i) JPL_TEST_CHECK(fabs(test_state_component(&app, (int)i) - test_state_component(&expect, (int)i)) < 1e-9);
        JPL_TEST_CHECK(fabs(app.x - wrong.x) > 1e-9);
        /* Independent velocity oracle: the position residual check uses
         * range/c, while velocity must equal the mixed-epoch difference
         * v_target(emission) - v_center(reception), not a numerical
         * derivative of apparent position. Recompute the velocity slots
         * directly from the two legs to pin the formula. */
        JPL_TEST_CHECK(fabs(app.vx - (tgt_lt.vx - ctr_now.vx)) < 1e-9);
        JPL_TEST_CHECK(fabs(app.vy - (tgt_lt.vy - ctr_now.vy)) < 1e-9);
        JPL_TEST_CHECK(fabs(app.vz - (tgt_lt.vz - ctr_now.vz)) < 1e-9);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Pinned analytic delay for the oracle mock. The linear
     * target/observer legs reproduce case 0 of the independent oracle;
     * the public solver must match the pinned delay and satisfy the
     * independent range/c residual. */
    {
        const char *path = "jpl_test_lighttime_oracle.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        jpl_date tdb = {0.0, 0.0};
        jpl_state app;
        double lt = -1.0;
        size_t k;
        static const double P[3] = {1.0, 0.4, -0.2};
        static const double V[3] = {0.005, -0.003, 0.001};
        static const double O[3] = {0.1, -0.2, 0.03};
        static const double W[3] = {0.002, 0.004, -0.001};
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 3, 3, 2, 1);
        test_put_ptr(rec, 2, 9, 2, 1);
        test_put_ptr(rec, 10, 15, 2, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(lc, 1, sizeof lc, f) == sizeof lc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        for (k = 0; k < 3; ++k) {
            test_put_f64(rec + (2 + k * 2) * 8, P[k] * JPL_TEST_AU_KM);
            test_put_f64(rec + (3 + k * 2) * 8, (V[k] * 0.5) * JPL_TEST_AU_KM);
            test_put_f64(rec + (8 + k * 2) * 8, O[k] * JPL_TEST_AU_KM);
            test_put_f64(rec + (9 + k * 2) * 8, (W[k] * 0.5) * JPL_TEST_AU_KM);
        }
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        { jpl_date ref_tt = {jpl_apparent_references[0].tt[0], jpl_apparent_references[0].tt[1]};
          JPL_TEST_CHECK(jpl_tt_to_tdb(&ref_tt, &tdb) == JPL_OK); }
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &tdb, JPL_MARS, JPL_EARTH_MOON_BARYCENTER, &app, &lt) == JPL_OK);
        JPL_TEST_CHECK(fabs(lt - jpl_apparent_references[0].delay_days) < 1e-12);
        JPL_TEST_CHECK(fabs(lt - hypot(hypot(app.x, app.y), app.z) / c) < 1e-12);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("lighttime tests passed");
    t10_rc = lighttime_t10_cases();
    if (t10_rc != 0) return 1;
    puts("lighttime T10 cases passed");
    return 0;
}

static int write_linear_mock(const char *path, double start, double end,
                             double slope_au_per_day, double intercept_au) {
    unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
    FILE *f;
    double half_span = (end - start) / 2.0;
    double mid = (start + end) / 2.0;
    /* Chebyshev one-coeff-constant plus slope: x(t) = c0 + c1*T, with
     * T in [-1,1] over the record, so c0 = x(mid), c1 = slope*half_span.
     * Multi-record coverage needs one data record per header step day:
     * each record carries its own [start,end] bounds plus the same linear
     * model recentered on its midpoint. */
    double span = end - start;
    long nrec = (long)span;
    long r;
    double c0 = intercept_au + slope_au_per_day * (mid - start);
    double c1 = slope_au_per_day * half_span;
    if (span <= 0.0 || span != (double)nrec || nrec < 1) return 1;
    if (nrec == 1) {
        test_mock_header(rec, start, end, end - start, 2);
        test_put_ptr(rec, 3, 3, 2, 1);   /* Mars: slots 3..6, two coeffs */
        f = fopen(path, "wb"); if (!f) return 1;
        if (fwrite(rec, 1, sizeof rec, f) != sizeof rec) { fclose(f); return 1; }
        { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
          if (fwrite(lc, 1, sizeof lc, f) != sizeof lc) { fclose(f); return 1; } }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, start);
        test_put_f64(rec + 1 * 8, end);
        test_put_f64(rec + 2 * 8, c0 * JPL_TEST_AU_KM);
        test_put_f64(rec + 3 * 8, c1 * JPL_TEST_AU_KM);
        if (fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) != JPL_TEST_HEADER_BYTES) { fclose(f); return 1; }
        if (fclose(f) != 0) return 1;
        return 0;
    }
    test_mock_header(rec, start, end, 1.0, 2);
    test_put_ptr(rec, 3, 3, 2, 1);   /* Mars: slots 3..6, two coeffs */
    f = fopen(path, "wb"); if (!f) return 1;
    if (fwrite(rec, 1, sizeof rec, f) != sizeof rec) { fclose(f); return 1; }
    { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
      test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
      if (fwrite(lc, 1, sizeof lc, f) != sizeof lc) { fclose(f); return 1; } }
    for (r = 0; r < nrec; ++r) {
        double rs = start + (double)r, re = rs + 1.0, rmid = rs + 0.5;
        double rc0 = intercept_au + slope_au_per_day * (rmid - start);
        double rc1 = slope_au_per_day * 0.5;
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, rs);
        test_put_f64(rec + 1 * 8, re);
        test_put_f64(rec + 2 * 8, rc0 * JPL_TEST_AU_KM);
        test_put_f64(rec + 3 * 8, rc1 * JPL_TEST_AU_KM);
        if (fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) != JPL_TEST_HEADER_BYTES) { fclose(f); return 1; }
    }
    if (fclose(f) != 0) return 1;
    return 0;
}

static int lighttime_t10_cases(void) {
    /* Receding-target boundary: x(t)=1+0.01*(t-S) AU, S=2451545,
     * SSB observer, receive {2451545.0, 0.0057756184}. The analytic
     * delay solves tau = (1+0.01*dt)/(c+0.01), not the retarded delay;
      * state must satisfy the independent range/c residual too. The
      * reception-time initial trial falls ~20 ms before coverage even
      * though the solved emission epoch is inside it. */
    {
        const char *path = "jpl_test_lighttime_r11.bin";
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        const double dt = 0.0057756184;
        const double expected_tau = (1.0 + 0.01 * dt) / (c + 0.01);
        static const jpl_date receive = {2451545.0, 0.0057756184};
        jpl_state state = {0, 0, 0, 0, 0, 0};
        double tau = -1.0;
        jpl_ephemeris *e = NULL;
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451546.0, 0.01, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &receive, JPL_MARS, 0, &state, &tau) == JPL_OK);
        JPL_TEST_CHECK(fabs(tau - expected_tau) < 1e-12);
        JPL_TEST_CHECK(fabs(tau - hypot(hypot(state.x, state.y), state.z) / c) < 1e-12);
        /* Mixed-epoch velocity pinned independently of the delay oracle:
         * the model velocity is exactly the 0.01 AU/day slope. */
        JPL_TEST_CHECK(fabs(state.vx - 0.01) < 1e-12);
        JPL_TEST_CHECK(state.vy == 0.0 && state.vz == 0.0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Genuinely pre-coverage root: receive slightly below 1/c with the
     * same linear model. The equation root precedes coverage, so RANGE
      * with unchanged outputs is required (contrast with the in-coverage case
     * above, which must succeed). */
    {
        const char *path = "jpl_test_lighttime_preco.bin";
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        jpl_date receive = {2451545.0, 1.0 / c - 1e-9};
        jpl_state state = {11.0, 12.0, 13.0, 14.0, 15.0, 16.0};
        jpl_state before = {11.0, 12.0, 13.0, 14.0, 15.0, 16.0};
        double tau = -7.0;
        jpl_ephemeris *e = NULL;
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451546.0, 0.01, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &receive, JPL_MARS, 0, &state, &tau) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(memcmp(&state, &before, sizeof state) == 0);
        JPL_TEST_CHECK(tau == -7.0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Moving observer with a stationary target: observer EMB fixed at
     * reception, target Mars constant at 2 AU. Delay is 2/c days and the
     * state holds the observer at reception, not at emission. */
    {
        const char *path = "jpl_test_lighttime_movobs.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        static const jpl_date jd = {2451545.5, 0.0};
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        jpl_state app;
        double tau = -1.0;
        jpl_ephemeris *e = NULL;
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 2, 6, 2, 1);   /* EMB: slots 6..9, two coeffs */
        test_put_ptr(rec, 3, 10, 2, 1);  /* Mars: slots 10..13, two coeffs */
        f = fopen(path, "wb"); JPL_TEST_CHECK(f);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        { unsigned char lc[JPL_TEST_HEADER_BYTES] = {0};
          test_put_f64(lc, 1.0); test_put_f64(lc + 8, 2.0);
          JPL_TEST_CHECK(fwrite(lc, 1, sizeof lc, f) == sizeof lc); }
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        /* EMB x(t): 0.5 AU mid-record with slope 0.02 AU/day. */
        test_put_f64(rec + 5 * 8, 0.5 * JPL_TEST_AU_KM);
        test_put_f64(rec + 6 * 8, 0.02 * 0.5 * JPL_TEST_AU_KM);
        /* Mars x(t): constant 2 AU. */
        test_put_f64(rec + 9 * 8, 2.0 * JPL_TEST_AU_KM);
        test_put_f64(rec + 10 * 8, 0.0);
        JPL_TEST_CHECK(fwrite(rec, 1, JPL_TEST_HEADER_BYTES, f) == JPL_TEST_HEADER_BYTES);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_MARS, JPL_EARTH_MOON_BARYCENTER, &app, &tau) == JPL_OK);
        {
            jpl_state obs, tgt, expect;
            jpl_date emi_date;
            double jd_arr[2], emi_arr[2];
            size_t k;
            jpl_date_to_pair(&jd, jd_arr);
            JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_EARTH_MOON_BARYCENTER, 0, &obs) == JPL_OK);
            JPL_TEST_CHECK(jpl_date_add_days(jd_arr, -tau, emi_arr) == JPL_OK);
            jpl_pair_to_date(emi_arr, &emi_date);
            JPL_TEST_CHECK(jpl_get_state(e, &emi_date, JPL_MARS, 0, &tgt) == JPL_OK);
            expect.x = tgt.x - obs.x; expect.y = tgt.y - obs.y; expect.z = tgt.z - obs.z;
            expect.vx = tgt.vx - obs.vx; expect.vy = tgt.vy - obs.vy; expect.vz = tgt.vz - obs.vz;
            for (k = 0; k < 6; ++k) JPL_TEST_CHECK(fabs(test_state_component(&app, (int)k) - test_state_component(&expect, (int)k)) < 1e-9);
            /* 2-D here: observer off-axis would enter via hypot; assert the
             * actual independent residual instead of a 1-D shortcut. */
            JPL_TEST_CHECK(fabs(tau - hypot(hypot(app.x, app.y), app.z) / c) < 1e-12);
            /* Target stationary: velocity is -observer velocity. */
            JPL_TEST_CHECK(fabs(app.vx - (0.0 - obs.vx)) < 1e-9);
        }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* SSB target with moving center: the fixed observer leg is evaluated
     * at reception while the (zero) target leg needs no coverage. The
     * delay is range/c of the negated observer vector. Identical bodies
     * must return exact zero state with zero delay (no correction
     * lookups beyond validation). */
    {
        const char *path = "jpl_test_lighttime_nanc.bin";
        jpl_state state;
        double tau = -7.0;
        size_t k;
        static const jpl_date jd = {2451545.5, 0.0};
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        jpl_ephemeris *e = NULL;
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451546.0, 0.01, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        for (k = 0; k < 6; ++k) { (&state.x)[k] = 11.0 + (double)k; }
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_SOLAR_SYSTEM_BARYCENTER, JPL_MARS, &state, &tau) == JPL_OK);
        {
            jpl_state obs;
            JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_MARS, 0, &obs) == JPL_OK);
            for (k = 0; k < 6; ++k) JPL_TEST_CHECK(fabs(test_state_component(&state, (int)k) + test_state_component(&obs, (int)k)) < 1e-9);
            JPL_TEST_CHECK(fabs(tau - hypot(hypot(state.x, state.y), state.z) / c) < 1e-12);
        }
        for (k = 0; k < 6; ++k) { (&state.x)[k] = 11.0 + (double)k; }
        tau = -7.0;
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_MARS, JPL_MARS, &state, &tau) == JPL_OK);
        JPL_TEST_CHECK(state.x == 0.0 && state.y == 0.0 && state.z == 0.0);
        JPL_TEST_CHECK(tau == 0.0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Superluminal evaluated file velocity: slope c+1 AU/day makes the
     * file state nonphysical; the solver must report FORMAT, never a
     * delay or a uniqueness claim. */
    {
        const char *path = "jpl_test_lighttime_super.bin";
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        static const jpl_date jd = {2451545.5, 0.0};
        jpl_state state, before;
        double tau = -7.0;
        size_t k;
        jpl_ephemeris *e = NULL;
        for (k = 0; k < 6; ++k) { (&state.x)[k] = 11.0 + (double)k; }
        before = state;
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451546.0, c + 1.0, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, &jd, JPL_MARS, 0, &state, &tau) == JPL_ERROR_FORMAT);
        JPL_TEST_CHECK(memcmp(&state, &before, sizeof state) == 0);
        JPL_TEST_CHECK(tau == -7.0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Coverage endpoints: at start the delay is the geometric range/c
     * when the emission stays in coverage; at end the same bracket
     * applies. Both must satisfy the independent residual. A stationary
     * 1 AU target has delay 1/c (~0.00577 d), so interior receptions
     * near each endpoint keep the emission inside one record. */
    {
        const char *path = "jpl_test_lighttime_ends.bin";
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        jpl_ephemeris *e = NULL;
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451549.0, 0.0, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        {
            static const jpl_date near_start = {2451545.0, 0.01};
            static const jpl_date near_end = {2451548.0, 0.99};
            jpl_state st, geo;
            double tau = -1.0;
            JPL_TEST_CHECK(jpl_get_state(e, &near_start, JPL_MARS, 0, &geo) == JPL_OK);
            JPL_TEST_CHECK(jpl_get_state_lighttime(e, &near_start, JPL_MARS, 0, &st, &tau) == JPL_OK);
            JPL_TEST_CHECK(fabs(tau - 1.0 / c) < 1e-12);
            JPL_TEST_CHECK(fabs(tau - hypot(hypot(st.x, st.y), st.z) / c) < 1e-12);
            JPL_TEST_CHECK(jpl_get_state_lighttime(e, &near_end, JPL_MARS, 0, &st, &tau) == JPL_OK);
            JPL_TEST_CHECK(fabs(tau - 1.0 / c) < 1e-12);
            JPL_TEST_CHECK(fabs(tau - hypot(hypot(st.x, st.y), st.z) / c) < 1e-12);
        }
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Deterministic exhaustion: zero budgets on a non-root case must
     * return CONVERGENCE with unchanged outputs. */
    {
        const char *path = "jpl_test_lighttime_budget.bin";
        static const double jd[2] = {2451545.5, 0.0};
        double state[6], before[6], tau = -7.0;
        size_t k;
        jpl_ephemeris *e = NULL;
        for (k = 0; k < 6; ++k) { state[k] = 11.0 + (double)k; before[k] = state[k]; }
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451546.0, 0.01, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, jd, JPL_MARS, 0, state, &tau, 0, 0) == JPL_ERROR_CONVERGENCE);
        JPL_TEST_CHECK(memcmp(state, before, sizeof state) == 0);
        JPL_TEST_CHECK(tau == -7.0);
        /* Negative budgets are ARGUMENT, also transactional. */
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, jd, JPL_MARS, 0, state, &tau, -1, 0) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(state, before, sizeof state) == 0);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Budget exhaustion after a successful evaluation is still failure.
     * fast=1 cannot settle this moving-target mock (each step shrinks the
     * residual by v/c only), so a 1/0 budget must return CONVERGENCE with
     * outputs untouched rather than reporting JPL_OK with the caller's
     * state never written. */
    {
        const char *path = "jpl_test_lighttime_budget_fast.bin";
        static const double jd[2] = {2451545.5, 0.0};
        const double c = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        double state[6], before[6], tau = -7.0, range;
        size_t k;
        jpl_ephemeris *e = NULL;
        for (k = 0; k < 6; ++k) { state[k] = 21.0 + (double)k; before[k] = state[k]; }
        JPL_TEST_CHECK(write_linear_mock(path, 2451545.0, 2451546.0, 0.01, 1.0) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, jd, JPL_MARS, 0, state, &tau, 1, 0) == JPL_ERROR_CONVERGENCE);
        JPL_TEST_CHECK(memcmp(state, before, sizeof state) == 0);
        JPL_TEST_CHECK(tau == -7.0);
        /* The safeguarded bracket must still recover the root when the
         * fast budget alone is short. */
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, jd, JPL_MARS, 0, state, &tau, 1, JPL_LIGHTTIME_SAFE_MAX_ITER) == JPL_OK);
        JPL_TEST_CHECK(tau > 0.0);
        range = hypot(hypot(state[0], state[1]), state[2]);
        JPL_TEST_CHECK(fabs(tau - range / c) < 1e-12);
        /* A full fast budget settles without the bracket and publishes
         * the emission-epoch state. */
        tau = -7.0;
        JPL_TEST_CHECK(jpl_test_lighttime_with_budget(e, jd, JPL_MARS, 0, state, &tau, JPL_LIGHTTIME_FAST_MAX_ITER, 0) == JPL_OK);
        JPL_TEST_CHECK(tau > 0.0);
        range = hypot(hypot(state[0], state[1]), state[2]);
        JPL_TEST_CHECK(fabs(tau - range / c) < 1e-12);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    return 0;
}
