#include "jpl.h"
#include "jpl_test_util.h"

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

    file = fopen(path, "wb"); JPL_TEST_CHECK(file != NULL);
    JPL_TEST_CHECK(fwrite(record, 1, sizeof record, file) == sizeof record);
    memset(record, 0, sizeof record);
    test_put_f64(record, 1.0); test_put_f64(record + 8, 2.0);
    JPL_TEST_CHECK(fwrite(record, 1, sizeof record, file) == sizeof record);
    for (i = 0; i < RECORD_DOUBLES; ++i) test_put_f64(record + i * 8, (double)i);
    JPL_TEST_CHECK(fwrite(record, 1, sizeof record, file) == sizeof record);
    JPL_TEST_CHECK(fclose(file) == 0);

    JPL_TEST_CHECK(jpl_open(path, &ephemeris) == JPL_OK);
    header = jpl_get_header(ephemeris);
    JPL_TEST_CHECK(header != NULL && header->constant_count == 2);
    JPL_TEST_CHECK(header->record_double_count == RECORD_DOUBLES);
    JPL_TEST_CHECK(jpl_record_count(ephemeris) == 1);
    JPL_TEST_CHECK(jpl_read_constants(ephemeris, constants, 2) == JPL_OK);
    JPL_TEST_CHECK(constants[0] == 1.0 && constants[1] == 2.0);
    JPL_TEST_CHECK(jpl_read_record(ephemeris, 0, coefficients, RECORD_DOUBLES) == JPL_OK);
    JPL_TEST_CHECK(coefficients[0] == 0.0 && coefficients[356] == 356.0);
    JPL_TEST_CHECK(jpl_read_record(ephemeris, 1, coefficients, RECORD_DOUBLES) == JPL_ERROR_RANGE);
    jpl_close(ephemeris);
    JPL_TEST_CHECK(remove(path) == 0);
    JPL_TEST_CHECK(jpl_open(NULL, NULL) == JPL_ERROR_ARGUMENT);
    /* Big-endian mock: same values, byte-swapped, must decode identically. */
    {
        unsigned char be[HEADER_BYTES] = {0};
        jpl_ephemeris *bee = NULL;
        const jpl_header *beh;
        double c0[2], r0[RECORD_DOUBLES];
        FILE *bf;
        memcpy(be, "Test JPL DE BE", 14);
        test_mock_header_be(be, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr_be(be, 11, 2, 178, 1);
        bf = fopen(path, "wb"); JPL_TEST_CHECK(bf != NULL);
        JPL_TEST_CHECK(fwrite(be, 1, sizeof be, bf) == sizeof be);
        memset(be, 0, sizeof be);
        test_put_f64be(be, 1.0); test_put_f64be(be + 8, 2.0);
        JPL_TEST_CHECK(fwrite(be, 1, sizeof be, bf) == sizeof be);
        for (i = 0; i < RECORD_DOUBLES; ++i) test_put_f64be(be + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(be, 1, sizeof be, bf) == sizeof be);
        JPL_TEST_CHECK(fclose(bf) == 0);
        JPL_TEST_CHECK(jpl_open(path, &bee) == JPL_OK);
        beh = jpl_get_header(bee);
        JPL_TEST_CHECK(beh != NULL && beh->byte_order == JPL_BYTE_ORDER_BIG_ENDIAN);
        JPL_TEST_CHECK(jpl_read_constants(bee, c0, 2) == JPL_OK);
        JPL_TEST_CHECK(c0[0] == 1.0 && c0[1] == 2.0);
        JPL_TEST_CHECK(jpl_read_record(bee, 0, r0, RECORD_DOUBLES) == JPL_OK);
        JPL_TEST_CHECK(r0[0] == 0.0 && r0[356] == 356.0);
        jpl_close(bee);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Truncated file is I/O error, not a crash. */
    {
        jpl_ephemeris *te = NULL;
        FILE *tf = fopen(path, "wb"); JPL_TEST_CHECK(tf != NULL);
        JPL_TEST_CHECK(fwrite("short", 1, 5, tf) == 5);
        JPL_TEST_CHECK(fclose(tf) == 0);
        JPL_TEST_CHECK(jpl_open(path, &te) == JPL_ERROR_IO);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("jpl tests passed");
    return 0;
}
