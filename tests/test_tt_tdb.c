#include "jpl.h"
#include "jpl_test_util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "jpl_test_tttdb.bin";
    /* Absent dataset: header with zero TT-TDB pointer must report
     * has == 0 and RANGE on get. */
    {
        unsigned char rec[2880] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        const jpl_header *h;
        double sec = 0.0, rate = 0.0;
        size_t i;
        memcpy(rec, "no TT-TDB here", 14);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 11, 2, 178, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        for (i = 0; i < 357; ++i) test_put_f64(rec + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->has_tt_tdb == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e) == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(NULL) == 0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, &sec, &rate) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(NULL, 2451545.5, &sec, &rate) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, NULL, &rate) == JPL_ERROR_ARGUMENT);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Present dataset, NULL rate allowed; out-of-coverage is RANGE. */
    {
        unsigned char rec[2880] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        double sec = 0.0, rate = -1.0;
        memcpy(rec, "TT-TDB present", 14);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 11, 2, 178, 1);
        test_put_u32(rec + 2868, 300);
        test_put_u32(rec + 2872, 2);
        test_put_u32(rec + 2876, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 299 * 8, 0.001);
        test_put_f64(rec + 300 * 8, 0.00002);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e) != 0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, &sec, NULL) == JPL_OK);
        JPL_TEST_CHECK(isfinite(sec));
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(isfinite(rate));
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.0 - 10.0, &sec, &rate) == JPL_ERROR_RANGE);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("tt-tdb tests passed");
    return 0;
}
