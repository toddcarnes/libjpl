/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_fixture.h"
#include "jpl_private.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int write_fixture(const char *path, int big_endian) {
    const double constants[2] = {1.0, 2.0};
    const double mars[3] = {JPL_TEST_AU_KM, 0.0, 0.0};
    jpl_fixture_series series;
    jpl_fixture_spec spec;

    memset(&series, 0, sizeof series);
    series.slot = 3; /* Mars */
    series.components = 3;
    series.coefficients_per_component = 1;
    series.subintervals = 1;
    series.coefficients = mars;

    memset(&spec, 0, sizeof spec);
    spec.start[0] = 2451545.0;
    spec.end[0] = 2451546.0;
    spec.step_days = 1.0;
    spec.denum = 441;
    spec.astronomical_unit_km = JPL_TEST_AU_KM;
    spec.earth_moon_mass_ratio = JPL_TEST_MASS_RATIO;
    spec.record_doubles = JPL_FIXTURE_DOUBLES;
    spec.records = 1;
    spec.constants = constants;
    spec.constant_count = 2;
    spec.series = &series;
    spec.series_count = 1;
    spec.big_endian = big_endian;
    spec.title = "fixture builder self-test";
    return jpl_fixture_write(path, &spec);
}

static int check_fixture(const char *path) {
    jpl_ephemeris *e = NULL;
    const jpl_header *header;
    jpl_state state;
    double raw[JPL_FIXTURE_DOUBLES];
    static const jpl_date jd = {2451545.5, 0.0};

    JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
    header = jpl_get_header(e);
    JPL_TEST_CHECK(header != NULL);
    JPL_TEST_CHECK(header->ephemeris_number == 441);
    JPL_TEST_CHECK(header->reference_frame == JPL_FRAME_ICRF);
    JPL_TEST_CHECK(header->pointers[3].start_index == 3);
    JPL_TEST_CHECK(header->pointers[3].coefficients_per_component == 1);
    JPL_TEST_CHECK(header->pointers[3].subintervals == 1);
    JPL_TEST_CHECK(jpl_get_state(e, &jd, JPL_MARS, JPL_SSB, &state) == JPL_OK);
    JPL_TEST_CHECK(fabs(state.x - 1.0) < 1e-15);
    JPL_TEST_CHECK(state.y == 0.0 && state.z == 0.0);
    JPL_TEST_CHECK(jpl_read_record(e, 0, raw, JPL_FIXTURE_DOUBLES) == JPL_OK);
    JPL_TEST_CHECK(raw[2] == JPL_TEST_AU_KM);
    jpl_close(e);
    return 0;
}

int main(void) {
    const char *le_path = "jpl_test_fixture_builder_le.bin";
    const char *be_path = "jpl_test_fixture_builder_be.bin";

    JPL_TEST_CHECK(write_fixture(le_path, 0) == 0);
    JPL_TEST_CHECK(check_fixture(le_path) == 0);
    JPL_TEST_CHECK(write_fixture(be_path, 1) == 0);
    JPL_TEST_CHECK(check_fixture(be_path) == 0);
    JPL_TEST_CHECK(remove(le_path) == 0);
    JPL_TEST_CHECK(remove(be_path) == 0);
    return 0;
}
