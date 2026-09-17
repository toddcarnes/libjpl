#include "jpl.h"
#include "jpl_test_util.h"
#include <math.h>
#include <stdio.h>

int main(void) {
    double st[6] = {0};
    JPL_TEST_CHECK(jpl_get_state_lighttime(NULL, 2451545.0, JPL_EARTH, JPL_SUN, st, NULL) == JPL_ERROR_ARGUMENT);
    /* Nutations/librations are angles: no light-time meaning, must reject. */
    JPL_TEST_CHECK(jpl_get_state_lighttime(NULL, 2451545.0, JPL_NUTATIONS, 0, st, NULL) == JPL_ERROR_ARGUMENT);
    /* Mock ephemeris: EMB=(1 AU,0,0), Sun=origin, zero velocities.
     * Light-time must converge to range/c with velocity from Chebyshev. */
    {
        const char *path = "jpl_test_lighttime.bin";
        unsigned char rec[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        const double jd = 2451545.5;
        double geo[6], app[6], lt = -1.0;
        double range, expect_lt;
        size_t i;
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
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, jd, JPL_NUTATIONS, 0, app, &lt) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, jd, JPL_LIBRATIONS, 0, app, &lt) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, geo) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, app, &lt) == JPL_OK);
        JPL_TEST_CHECK(lt > 0.0);
        range = test_norm3(geo);
        expect_lt = range / JPL_TEST_C_AU_PER_DAY;
        /* Converged light-time matches range/c of the geometric vector.
         * Zero test velocities make the retarded epoch coincide with the
         * geometric one, so apparent must equal geometric exactly here. */
        JPL_TEST_CHECK(fabs(lt - expect_lt) < 1e-12);
        for (i = 0; i < 6; ++i) JPL_TEST_CHECK(app[i] == geo[i]);
        for (i = 0; i < 6; ++i) JPL_TEST_CHECK(isfinite(app[i]));
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
        const double jd = 2451545.5;
        double app[6], lt = -1.0;
        double tgt_lt[6], ctr_now[6], ctr_lt[6], expect[6], wrong[6];
        size_t i;
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
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, app, &lt) == JPL_OK);
        JPL_TEST_CHECK(lt > 0.0);
        /* jpl_get_state already returns AU, so no further scaling here. */
        JPL_TEST_CHECK(jpl_get_state(e, jd - lt, JPL_EARTH_MOON_BARYCENTER, 0, tgt_lt) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, jd, JPL_SUN, 0, ctr_now) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, jd - lt, JPL_SUN, 0, ctr_lt) == JPL_OK);
        for (i = 0; i < 6; ++i) expect[i] = tgt_lt[i] - ctr_now[i];
        for (i = 0; i < 6; ++i) wrong[i] = tgt_lt[i] - ctr_lt[i];
        for (i = 0; i < 6; ++i) JPL_TEST_CHECK(fabs(app[i] - expect[i]) < 1e-9);
        JPL_TEST_CHECK(fabs(app[0] - wrong[0]) > 1e-9);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("lighttime tests passed");
    return 0;
}
