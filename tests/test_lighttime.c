#include "jpl.h"
#include "jpl_test_util.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    double st[6] = {0};
    assert(jpl_get_state_lighttime(NULL, 2451545.0, JPL_EARTH, JPL_SUN, st, NULL) == JPL_ERROR_ARGUMENT);
    /* Nutations/librations are angles: no light-time meaning, must reject. */
    assert(jpl_get_state_lighttime(NULL, 2451545.0, JPL_NUTATIONS, 0, st, NULL) == JPL_ERROR_ARGUMENT);
    /* Mock ephemeris: EMB=(1 AU,0,0), Sun=origin, zero velocities.
     * Light-time must converge to range/c with velocity from Chebyshev. */
    {
        const char *path = "jpl_test_lighttime.bin";
        unsigned char rec[JPL_TEST_RECORD_BYTES] = {0};
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
        f = fopen(path, "wb"); assert(f);
        assert(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        assert(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 5 * 8, 1.0); /* EMB x = 1 AU at slot 6, rest zero */
        assert(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        assert(fclose(f) == 0);
        assert(jpl_open(path, &e) == JPL_OK);
        assert(jpl_get_state(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, geo) == JPL_OK);
        assert(jpl_get_state_lighttime(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, app, &lt) == JPL_OK);
        assert(lt > 0.0);
        range = test_norm3(geo);
        expect_lt = range / 173.144632684;
        /* Converged light-time matches range/c of the geometric vector.
         * Zero test velocities make the retarded epoch coincide with the
         * geometric one, so apparent must equal geometric exactly here. */
        assert(fabs(lt - expect_lt) < 1e-12);
        for (i = 0; i < 6; ++i) assert(app[i] == geo[i]);
        for (i = 0; i < 6; ++i) assert(isfinite(app[i]));
        jpl_close(e);
        assert(remove(path) == 0);
    }
    puts("lighttime tests passed");
    return 0;
}
