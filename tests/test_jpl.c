#include "jpl.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { HEADER_BYTES = 2856, RECORD_DOUBLES = 357 };

static void put_u32(unsigned char *at, uint32_t value) {
    at[0] = (unsigned char)value;
    at[1] = (unsigned char)(value >> 8);
    at[2] = (unsigned char)(value >> 16);
    at[3] = (unsigned char)(value >> 24);
}

static void put_f64(unsigned char *at, double value) { memcpy(at, &value, sizeof value); }

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
    put_f64(record + 2652, 2451545.0);
    put_f64(record + 2660, 2451546.0);
    put_f64(record + 2668, 1.0);
    put_u32(record + 2676, 2);
    put_f64(record + 2680, 149597870.7);
    put_f64(record + 2688, 81.30056);
    /* A two-component final IPT entry gives a 357-double mock record. */
    put_u32(record + 2696 + 11 * 12, 2);
    put_u32(record + 2696 + 11 * 12 + 4, 178);
    put_u32(record + 2696 + 11 * 12 + 8, 1);

    file = fopen(path, "wb"); assert(file != NULL);
    assert(fwrite(record, 1, sizeof record, file) == sizeof record);
    memset(record, 0, sizeof record);
    put_f64(record, 1.0); put_f64(record + 8, 2.0);
    assert(fwrite(record, 1, sizeof record, file) == sizeof record);
    for (i = 0; i < RECORD_DOUBLES; ++i) put_f64(record + i * 8, (double)i);
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
    puts("jpl tests passed");
    return 0;
}
