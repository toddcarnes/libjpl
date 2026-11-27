/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Domain-level DE fixture builder for behavior tests.
 *
 * Tests declare coverage, bodies, coefficient series, and optional
 * TT-TDB/libration data; the builder owns the on-disk record layout, slot
 * assignment, constants record, padding, and byte order. Parser-negative
 * tests keep using the raw byte helpers in jpl_test_util.h; this builder is
 * for fixtures whose subject is behavior, not layout. */
#ifndef LIBJPL_FIXTURE_H
#define LIBJPL_FIXTURE_H

#include "jpl_test_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One file slot's coefficient series. slot is the 0-based IPT file slot:
 * 0..9 Mercury..Pluto, 10 Sun, 11 nutations. Coefficients are record-major:
 * record r, subinterval s, component c, coefficient k lives at
 * ((r * subintervals + s) * components + c) * coefficients_per_component + k.
 * Values are raw file units (km, km/day for bodies; radians for angles). */
typedef struct jpl_fixture_series {
    int slot;
    int components;
    int coefficients_per_component;
    int subintervals;
    const double *coefficients;
} jpl_fixture_series;

typedef struct jpl_fixture_spec {
    double start[2];
    double end[2];
    double step_days;
    int denum;
    double astronomical_unit_km;
    double earth_moon_mass_ratio;
    size_t record_doubles;      /* >= JPL_TEST_RECORD_BYTES / 8 */
    size_t records;             /* advertised data records */
    const double *constants;    /* constant_count values, or NULL for zeros */
    size_t constant_count;
    const jpl_fixture_series *series;
    size_t series_count;
    int tt_tdb_present;
    int tt_tdb_coeffs;                 /* coefficients per component */
    int tt_tdb_subintervals;
    const double *tt_tdb_coefficients; /* records * coeffs per record */
    int libration_present;
    int libration_coeffs;              /* coefficients per component */
    int libration_subintervals;
    const double *libration_coefficients; /* records * 3 comps * sub * coeffs */
    int big_endian;
    int trailing_record;
    const char *title;
} jpl_fixture_spec;

/* Write a valid DE file for spec. Returns 0 on success, 1 on failure.
 * Slot start indices are assigned sequentially from slot 3 (after the two
 * date doubles), with libration and TT-TDB after the declared series. */
int jpl_fixture_write(const char *path, const jpl_fixture_spec *spec);

/* Convenience defaults for behavior mocks. */
#define JPL_FIXTURE_DOUBLES 360
#define JPL_FIXTURE_AU_KM JPL_TEST_AU_KM
#define JPL_FIXTURE_EMRAT JPL_TEST_MASS_RATIO

#ifdef __cplusplus
}
#endif

#endif /* LIBJPL_FIXTURE_H */
