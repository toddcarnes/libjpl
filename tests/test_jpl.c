#include "jpl.h"
#include "jpl_test_util.h"

#include <assert.h>
#include <stdio.h>

enum { HEADER_BYTES = JPL_TEST_RECORD_BYTES, RECORD_DOUBLES = 357 };

int main(void) {
    const char *path = "jpl_test_de.bin";
    unsigned char record[HEADER_BYTES] = { 0 };
    FILE *file;
    jpl_ephemeris *ephemeris;
    const jpl_header *header;
    double constants[2], coefficients[RECORD_DOUBLES];
    size_t i;

    memcpy(record, "Test JPL DE", 11);
    memcpy(record + 252, "AU    ", 6);
    test_mock_header(record, 2451545.0, 2451546.0, 1.0, 2);
    /* A two-component final IPT entry gives a 357-double mock record. */
    test_put_ptr(record, 11, 2, 178, 1);

    file = fopen(path, "wb"); assert(file != NULL);
    assert(fwrite(record, 1, sizeof record, file) == sizeof record);
    memset(record, 0, sizeof record);
    test_put_f64(record, 1.0); test_put_f64(record + 8, 2.0);
    assert(fwrite(record, 1, sizeof record, file) == sizeof record);
    for (i = 0; i < RECORD_DOUBLES; ++i) test_put_f64(record + i * 8, (double)i);
    assert(fwrite(record, 1, sizeof record, file) == sizeof record);
    assert(fclose(file) == 0);

    assert(jpl_open(path, &ephemeris) == JPL_OK);
    header = jpl_get_header(ephemeris);
    assert(header != NULL && header->constant_count == 2);
    assert(header->record_double_count == RECORD_DOUBLES);
    assert(jpl_record_count(ephemeris) == 1);
    assert(jpl_read_constants(ephemeris, constants, 2) == JPL_OK);
    assert(constants[0] == 1.0 && constants[1] == 2.0);
    assert(jpl_read_record(ephemeris, 0, coefficients, RECORD_DOUBLES) == JPL_OK);
    assert(coefficients[0] == 0.0 && coefficients[356] == 356.0);
    assert(jpl_read_record(ephemeris, 1, coefficients, RECORD_DOUBLES) == JPL_ERROR_RANGE);
    jpl_close(ephemeris);
    assert(remove(path) == 0);
    assert(jpl_open(NULL, NULL) == JPL_ERROR_ARGUMENT);
    /* Big-endian mock: same values, byte-swapped, must decode identically. */
    {
        unsigned char be[HEADER_BYTES] = {0};
        jpl_ephemeris *bee = NULL;
        const jpl_header *beh;
        double c0[2], r0[RECORD_DOUBLES];
        FILE *bf;
        memcpy(be, "Test JPL DE BE", 14);
        test_put_f64be(be + 2652, 2451545.0);
        test_put_f64be(be + 2660, 2451546.0);
        test_put_f64be(be + 2668, 1.0);
        test_put_u32be(be + 2676, 2);
        test_put_f64be(be + 2680, 149597870.7);
        test_put_f64be(be + 2688, 81.30056);
        test_put_u32be(be + 2696 + 11 * 12, 2);
        test_put_u32be(be + 2696 + 11 * 12 + 4, 178);
        test_put_u32be(be + 2696 + 11 * 12 + 8, 1);
        bf = fopen(path, "wb"); assert(bf != NULL);
        assert(fwrite(be, 1, sizeof be, bf) == sizeof be);
        memset(be, 0, sizeof be);
        test_put_f64be(be, 1.0); test_put_f64be(be + 8, 2.0);
        assert(fwrite(be, 1, sizeof be, bf) == sizeof be);
        for (i = 0; i < RECORD_DOUBLES; ++i) test_put_f64be(be + i * 8, (double)i);
        assert(fwrite(be, 1, sizeof be, bf) == sizeof be);
        assert(fclose(bf) == 0);
        assert(jpl_open(path, &bee) == JPL_OK);
        beh = jpl_get_header(bee);
        assert(beh != NULL && beh->byte_order == JPL_BYTE_ORDER_BIG_ENDIAN);
        assert(jpl_read_constants(bee, c0, 2) == JPL_OK);
        assert(c0[0] == 1.0 && c0[1] == 2.0);
        assert(jpl_read_record(bee, 0, r0, RECORD_DOUBLES) == JPL_OK);
        assert(r0[0] == 0.0 && r0[356] == 356.0);
        jpl_close(bee);
        assert(remove(path) == 0);
    }
    /* Truncated file is I/O error, not a crash. */
    {
        jpl_ephemeris *te = NULL;
        FILE *tf = fopen(path, "wb"); assert(tf != NULL);
        assert(fwrite("short", 1, 5, tf) == 5);
        assert(fclose(tf) == 0);
        assert(jpl_open(path, &te) == JPL_ERROR_IO);
        assert(remove(path) == 0);
    }
    puts("jpl tests passed");
    return 0;
}
