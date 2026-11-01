/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_test_util.h"
#include "jpl_private.h"

#include <stdio.h>

enum { HEADER_BYTES = JPL_TEST_HEADER_BYTES, RECORD_DOUBLES = 360, DATA_BYTES = RECORD_DOUBLES * 8 };

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
    /* A two-component final IPT entry gives a 357-double extent; the file
     * record is padded to the 360-double (2880-byte) uniform record size. */
    test_put_ptr(record, 11, 2, 178, 1);
    test_put_ptr(record, 12, 441, 0, 0); /* DENUM + absent libration */

    file = fopen(path, "wb"); JPL_TEST_CHECK(file != NULL);
    JPL_TEST_CHECK(fwrite(record, 1, sizeof record, file) == sizeof record);
    {
        /* Constants record: fixed 2880-byte header-sized block (zeros here
         * except the two constant values); data records follow at 357. */
        unsigned char consts[HEADER_BYTES] = { 0 };
        test_put_f64(consts, 1.0); test_put_f64(consts + 8, 2.0);
        JPL_TEST_CHECK(fwrite(consts, 1, sizeof consts, file) == sizeof consts);
    }
    memset(record, 0, HEADER_BYTES);
    test_put_f64(record + 0 * 8, 2451545.0);
    test_put_f64(record + 1 * 8, 2451546.0);
    for (i = 2; i < RECORD_DOUBLES; ++i) test_put_f64(record + i * 8, (double)i);
    JPL_TEST_CHECK(fwrite(record, 1, DATA_BYTES, file) == DATA_BYTES);
    JPL_TEST_CHECK(fclose(file) == 0);

    JPL_TEST_CHECK(jpl_open(path, &ephemeris) == JPL_OK);
    header = jpl_get_header(ephemeris);
    JPL_TEST_CHECK(header != NULL && header->constant_count == 2);
    JPL_TEST_CHECK(header->record_double_count == RECORD_DOUBLES);
    JPL_TEST_CHECK(jpl_record_count(ephemeris) == 1);
    JPL_TEST_CHECK(jpl_read_constants(ephemeris, constants, 2) == JPL_OK);
    JPL_TEST_CHECK(constants[0] == 1.0 && constants[1] == 2.0);
    JPL_TEST_CHECK(jpl_read_record(ephemeris, 0, coefficients, RECORD_DOUBLES) == JPL_OK);
    JPL_TEST_CHECK(coefficients[0] == 2451545.0 && coefficients[1] == 2451546.0 && coefficients[356] == 356.0 && coefficients[359] == 359.0);
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
        test_put_ptr_be(be, 12, 441, 0, 0); /* DENUM + absent libration */
        bf = fopen(path, "wb"); JPL_TEST_CHECK(bf != NULL);
        JPL_TEST_CHECK(fwrite(be, 1, sizeof be, bf) == sizeof be);
        {
            unsigned char bec[HEADER_BYTES] = { 0 };
            test_put_f64be(bec, 1.0); test_put_f64be(bec + 8, 2.0);
            JPL_TEST_CHECK(fwrite(bec, 1, sizeof bec, bf) == sizeof bec);
        }
        memset(be, 0, HEADER_BYTES);
        test_put_f64be(be + 0 * 8, 2451545.0);
        test_put_f64be(be + 1 * 8, 2451546.0);
        for (i = 2; i < RECORD_DOUBLES; ++i) test_put_f64be(be + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(be, 1, DATA_BYTES, bf) == DATA_BYTES);
        JPL_TEST_CHECK(fclose(bf) == 0);
        JPL_TEST_CHECK(jpl_open(path, &bee) == JPL_OK);
        beh = jpl_get_header(bee);
        JPL_TEST_CHECK(beh != NULL && beh->byte_order == JPL_BYTE_ORDER_BIG_ENDIAN);
        JPL_TEST_CHECK(jpl_read_constants(bee, c0, 2) == JPL_OK);
        JPL_TEST_CHECK(c0[0] == 1.0 && c0[1] == 2.0);
        JPL_TEST_CHECK(jpl_read_record(bee, 0, r0, RECORD_DOUBLES) == JPL_OK);
        JPL_TEST_CHECK(r0[0] == 2451545.0 && r0[1] == 2451546.0 && r0[356] == 356.0);
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
    /* TT-TDB pointer at IPT entry 15 (file offset 2868): present case. */
    {
        unsigned char rec2[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f2;
        jpl_ephemeris *e2 = NULL;
        const jpl_header *h2;
        memcpy(rec2, "TT-TDB present", 14);
        test_mock_header(rec2, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec2, 11, 2, 178, 1);
        test_put_ptr(rec2, 12, 441, 0, 0); /* DENUM + absent libration */
        test_put_u32(rec2 + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET, 300);
        test_put_u32(rec2 + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET + 4, 11);
        test_put_u32(rec2 + JPL_HDR_IPT14_OFFSET + JPL_TT_TDB_TRIPLET_OFFSET + 8, 4);
        f2 = fopen(path, "wb"); JPL_TEST_CHECK(f2 != NULL);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        {
            unsigned char c2[JPL_TEST_HEADER_BYTES] = { 0 };
            test_put_f64(c2, 1.0); test_put_f64(c2 + 8, 2.0);
            JPL_TEST_CHECK(fwrite(c2, 1, sizeof c2, f2) == sizeof c2);
        }
        memset(rec2, 0, JPL_TEST_HEADER_BYTES);
        test_put_f64(rec2 + 0 * 8, 2451545.0);
        test_put_f64(rec2 + 1 * 8, 2451546.0);
        for (i = 2; i < RECORD_DOUBLES; ++i) test_put_f64(rec2 + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(rec2, 1, DATA_BYTES, f2) == DATA_BYTES);
        JPL_TEST_CHECK(fclose(f2) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e2) == JPL_OK);
        h2 = jpl_get_header(e2);
        JPL_TEST_CHECK(h2 != NULL && h2->tt_tdb_pointer.start_index == 300);
        JPL_TEST_CHECK(h2->tt_tdb_pointer.coefficients_per_component == 11);
        JPL_TEST_CHECK(h2->tt_tdb_pointer.subintervals == 4);
        JPL_TEST_CHECK(h2->has_tt_tdb != 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e2) != 0);
        jpl_close(e2);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* TT-TDB pointer at IPT entry 15 (file offset 2868): absent case. */
    {
        unsigned char rec3[JPL_TEST_HEADER_BYTES] = {0};
        FILE *f3;
        jpl_ephemeris *e3 = NULL;
        const jpl_header *h3;
        memcpy(rec3, "TT-TDB absent", 13);
        test_mock_header(rec3, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec3, 11, 2, 178, 1);
        test_put_ptr(rec3, 12, 441, 0, 0); /* DENUM + absent libration */
        /* IPT entries 14/15 left as zeros: dataset absent. */
        f3 = fopen(path, "wb"); JPL_TEST_CHECK(f3 != NULL);
        JPL_TEST_CHECK(fwrite(rec3, 1, sizeof rec3, f3) == sizeof rec3);
        {
            unsigned char c3[JPL_TEST_HEADER_BYTES] = { 0 };
            test_put_f64(c3, 1.0); test_put_f64(c3 + 8, 2.0);
            JPL_TEST_CHECK(fwrite(c3, 1, sizeof c3, f3) == sizeof c3);
        }
        memset(rec3, 0, JPL_TEST_HEADER_BYTES);
        test_put_f64(rec3 + 0 * 8, 2451545.0);
        test_put_f64(rec3 + 1 * 8, 2451546.0);
        for (i = 2; i < RECORD_DOUBLES; ++i) test_put_f64(rec3 + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(rec3, 1, DATA_BYTES, f3) == DATA_BYTES);
        JPL_TEST_CHECK(fclose(f3) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e3) == JPL_OK);
        h3 = jpl_get_header(e3);
        JPL_TEST_CHECK(h3 != NULL && h3->has_tt_tdb == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e3) == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(NULL) == 0);
        jpl_close(e3);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("jpl tests passed");
    return 0;
}
