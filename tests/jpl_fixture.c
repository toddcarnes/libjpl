/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_fixture.h"

#include <stdlib.h>
#include <string.h>

static void put_ptr(unsigned char *header, int slot, unsigned int start,
                    unsigned int coeffs, unsigned int subintervals, int big_endian) {
    if (big_endian) test_put_ptr_be(header, slot, start, coeffs, subintervals);
    else test_put_ptr(header, (int)slot, start, coeffs, subintervals);
}

int jpl_fixture_write(const char *path, const jpl_fixture_spec *spec) {
    unsigned char *header = NULL;
    double *constants = NULL;
    double *records = NULL;
    size_t *starts = NULL;
    jpl_test_mock mock;
    size_t record_bytes, i, r, s, c, k, next_start = 3;
    size_t data_records;
    size_t libration_start = 0, tt_tdb_start = 0;
    double start_scalar;
    int ok = 0;

    if (!path || !spec) return 1;
    if (spec->record_doubles < (size_t)JPL_TEST_RECORD_BYTES / 8) return 1;
    if (spec->records == 0) return 1;
    data_records = spec->records + (spec->trailing_record ? (size_t)1 : (size_t)0);
    if (data_records < spec->records) return 1;
    if (data_records > SIZE_MAX / spec->record_doubles) return 1;
    if (spec->series_count > 0 && !spec->series) return 1;
    if (spec->constant_count > 0 && !spec->constants) return 1;
    /* test_write_mock() serializes the whole record-sized constants block,
     * reading every slot, so a partial block must still be backed by a
     * full-size, zero-filled allocation. */
    if (spec->constant_count > spec->record_doubles) return 1;
    if (spec->tt_tdb_present &&
        (spec->tt_tdb_coeffs <= 0 || spec->tt_tdb_subintervals <= 0 ||
         !spec->tt_tdb_coefficients)) return 1;
    if (spec->libration_present &&
        (spec->libration_coeffs <= 0 || spec->libration_subintervals <= 0 ||
         !spec->libration_coefficients)) return 1;

    record_bytes = spec->record_doubles * 8;
    header = (unsigned char *)calloc(record_bytes, 1);
    records = (double *)calloc(data_records * spec->record_doubles, sizeof(double));
    if (!header || !records) goto done;
    if (spec->constant_count > 0) {
        constants = (double *)calloc(spec->record_doubles, sizeof(double));
        if (!constants) goto done;
        memcpy(constants, spec->constants, spec->constant_count * sizeof(double));
    }
    if (spec->series_count > 0) {
        starts = (size_t *)calloc(spec->series_count, sizeof(size_t));
        if (!starts) goto done;
    }

    if (spec->big_endian) test_mock_header_be(header, spec->start[0], spec->start[1],
                                              spec->step_days, (unsigned int)spec->constant_count);
    else test_mock_header(header, spec->start[0], spec->start[1],
                          spec->step_days, (unsigned int)spec->constant_count);
    if (spec->title) {
        size_t title_len = strlen(spec->title);
        if (title_len > (size_t)JPL_HDR_TITLES_LEN) title_len = (size_t)JPL_HDR_TITLES_LEN;
        memcpy(header, spec->title, title_len);
    }

    for (i = 0; i < spec->series_count; ++i) {
        const jpl_fixture_series *series = &spec->series[i];
        size_t span;
        if (series->slot < 0 || series->slot > 11) goto done;
        if (series->components <= 0 || series->coefficients_per_component <= 0 ||
            series->subintervals <= 0 || !series->coefficients) goto done;
        span = (size_t)series->components * (size_t)series->subintervals *
               (size_t)series->coefficients_per_component;
        if (span > SIZE_MAX - next_start) goto done;
        starts[i] = next_start;
        put_ptr(header, series->slot, (unsigned int)next_start,
                (unsigned int)series->coefficients_per_component,
                (unsigned int)series->subintervals, spec->big_endian);
        next_start += span;
    }

    if (spec->libration_present) {
        libration_start = next_start;
        next_start += 3u * (size_t)spec->libration_subintervals *
                      (size_t)spec->libration_coeffs;
    }
    put_ptr(header, 12, (unsigned int)spec->denum, (unsigned int)libration_start,
            (unsigned int)(spec->libration_present ? spec->libration_coeffs : 0),
            spec->big_endian);
    if (spec->big_endian)
        test_put_u32be(header + JPL_HDR_LIBRATION_OFFSET + 8,
                       (unsigned int)(spec->libration_present ? spec->libration_subintervals : 0));
    else
        test_put_u32(header + JPL_HDR_LIBRATION_OFFSET + 8,
                     (unsigned int)(spec->libration_present ? spec->libration_subintervals : 0));

    if (spec->tt_tdb_present) {
        tt_tdb_start = next_start;
        next_start += (size_t)spec->tt_tdb_subintervals *
                      (size_t)spec->tt_tdb_coeffs;
        put_ptr(header, 14, (unsigned int)tt_tdb_start,
                (unsigned int)spec->tt_tdb_coeffs,
                (unsigned int)spec->tt_tdb_subintervals, spec->big_endian);
    }

    if (next_start - 1 > spec->record_doubles) goto done;

    start_scalar = spec->start[0] + spec->start[1];
    for (r = 0; r < spec->records; ++r) {
        double *rec = records + r * spec->record_doubles;
        rec[0] = start_scalar + (double)r * spec->step_days;
        rec[1] = start_scalar + (double)(r + 1) * spec->step_days;
        for (i = 0; i < spec->series_count; ++i) {
            const jpl_fixture_series *series = &spec->series[i];
            size_t base = starts[i] - 1;
            size_t stride = (size_t)series->components * (size_t)series->subintervals *
                            (size_t)series->coefficients_per_component;
            for (s = 0; s < (size_t)series->subintervals; ++s)
                for (c = 0; c < (size_t)series->components; ++c)
                    for (k = 0; k < (size_t)series->coefficients_per_component; ++k)
                        rec[base + ((s * (size_t)series->components + c) *
                                    (size_t)series->coefficients_per_component) + k] =
                            series->coefficients[r * stride +
                                ((s * (size_t)series->components + c) *
                                 (size_t)series->coefficients_per_component) + k];
        }
        if (spec->libration_present) {
            size_t stride = 3u * (size_t)spec->libration_subintervals *
                            (size_t)spec->libration_coeffs;
            for (s = 0; s < (size_t)spec->libration_subintervals; ++s)
                for (c = 0; c < 3u; ++c)
                    for (k = 0; k < (size_t)spec->libration_coeffs; ++k)
                        rec[libration_start - 1 +
                            ((s * 3u + c) * (size_t)spec->libration_coeffs) + k] =
                            spec->libration_coefficients[r * stride +
                                ((s * 3u + c) * (size_t)spec->libration_coeffs) + k];
        }
        if (spec->tt_tdb_present) {
            size_t stride = (size_t)spec->tt_tdb_subintervals *
                            (size_t)spec->tt_tdb_coeffs;
            for (s = 0; s < (size_t)spec->tt_tdb_subintervals; ++s)
                for (k = 0; k < (size_t)spec->tt_tdb_coeffs; ++k)
                    rec[tt_tdb_start - 1 +
                        (s * (size_t)spec->tt_tdb_coeffs) + k] =
                        spec->tt_tdb_coefficients[r * stride +
                            (s * (size_t)spec->tt_tdb_coeffs) + k];
        }
    }

    memset(&mock, 0, sizeof mock);
    mock.record_doubles = spec->record_doubles;
    mock.advertised_records = spec->records;
    mock.trailing_record = spec->trailing_record;
    mock.big_endian = spec->big_endian;
    mock.denum = spec->denum;
    mock.constant_count = (unsigned int)spec->constant_count;
    mock.start[0] = spec->start[0];
    mock.start[1] = spec->start[1];
    mock.end[0] = spec->end[0];
    mock.end[1] = spec->end[1];
    mock.step_days = spec->step_days;
    mock.au_km = spec->astronomical_unit_km;
    mock.emrat = spec->earth_moon_mass_ratio;
    mock.header = header;
    mock.constants = constants;
    mock.records = records;
    ok = test_write_mock(path, &mock) == 0;

done:
    free(starts);
    free(records);
    free(constants);
    free(header);
    return ok ? 0 : 1;
}
