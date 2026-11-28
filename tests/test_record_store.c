/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_fixture.h"
#include "jpl_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_store_fixture(const char *path, int big_endian) {
    const double constants[2] = {1.0, 2.0};
    const double mars[2][3] = {{1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}};
    jpl_fixture_series series;
    jpl_fixture_spec spec;
    memset(&series, 0, sizeof series);
    series.slot = 3;
    series.components = 3;
    series.coefficients_per_component = 1;
    series.subintervals = 1;
    series.coefficients = &mars[0][0];
    memset(&spec, 0, sizeof spec);
    spec.start[0] = 2451545.0;
    spec.end[0] = 2451547.0;
    spec.step_days = 1.0;
    spec.denum = 441;
    spec.astronomical_unit_km = JPL_TEST_AU_KM;
    spec.earth_moon_mass_ratio = JPL_TEST_MASS_RATIO;
    spec.record_doubles = JPL_FIXTURE_DOUBLES;
    spec.records = 2;
    spec.constants = constants;
    spec.constant_count = 2;
    spec.series = &series;
    spec.series_count = 1;
    spec.big_endian = big_endian;
    spec.title = "record store self-test";
    return jpl_fixture_write(path, &spec);
}

static int check_store(const char *path, jpl_byte_order order) {
    FILE *f;
    long size;
    jpl_record_store store;
    jpl_record_view first, second;
    unsigned char raw[16];

    f = fopen(path, "rb");
    if (!f) return 1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 1; }
    size = ftell(f);
    if (size < 0) { fclose(f); return 1; }
    if (jpl_store_init_file(&store, f, (size_t)size, order) != JPL_OK) { fclose(f); return 1; }

    JPL_TEST_CHECK(jpl_store_read_raw(&store, 0, raw, sizeof raw) == JPL_OK);
    JPL_TEST_CHECK(jpl_store_record(&store, JPL_FIXTURE_DOUBLES, 0, &first) == JPL_OK);
    JPL_TEST_CHECK(first.values != NULL);
    JPL_TEST_CHECK(first.values[0] == 2451545.0);
    JPL_TEST_CHECK(first.values[1] == 2451546.0);
    JPL_TEST_CHECK(first.values[2] == 1.0);
    JPL_TEST_CHECK(jpl_store_record(&store, JPL_FIXTURE_DOUBLES, 0, &second) == JPL_OK);
    JPL_TEST_CHECK(second.values == first.values); /* FILE cache hit */
    JPL_TEST_CHECK(jpl_store_record(&store, JPL_FIXTURE_DOUBLES, 1, &second) == JPL_OK);
    JPL_TEST_CHECK(second.values[2] == 2.0);
    JPL_TEST_CHECK(jpl_store_record(&store, JPL_FIXTURE_DOUBLES, 2, &second) == JPL_ERROR_IO);
    jpl_record_view_release(&first);
    jpl_record_view_release(&second);

    free(store.cache.record);
    fclose(f);
    return 0;
}

int main(void) {
    const char *le_path = "jpl_test_record_store_le.bin";
    const char *be_path = "jpl_test_record_store_be.bin";
    JPL_TEST_CHECK(write_store_fixture(le_path, 0) == 0);
    JPL_TEST_CHECK(check_store(le_path, JPL_BYTE_ORDER_LITTLE_ENDIAN) == 0);
    JPL_TEST_CHECK(write_store_fixture(be_path, 1) == 0);
    JPL_TEST_CHECK(check_store(be_path, JPL_BYTE_ORDER_BIG_ENDIAN) == 0);
    JPL_TEST_CHECK(remove(le_path) == 0);
    JPL_TEST_CHECK(remove(be_path) == 0);
    return 0;
}
