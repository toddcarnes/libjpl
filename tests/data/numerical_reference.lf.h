/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Pinned independent numerical reference (test-only, generated).
 * Produced by tools/generate_numerical_reference.c calling upstream
 * ERFA primitives directly (eraDtdb/eraEcm06/eraAb), never libjpl.
 * ERFA v2.0.1 (SOFA 20231011); anchor eraDtdb(2448939.5, 0.123,
 * 0.76543, 5.0123, 5525.242, 3190.0) = -0.1280368005936998991e-2
 * (ERFA t_erfa_c.c, t_dtdb, tolerance 1e-15), verified at generation.
 * Review tools/generate_numerical_reference.c and
 * tests/data/numerical_reference.md before trusting these values.
 * Apparent-oracle mock: barycentric target P={1,0.4,-0.2},
 * V={0.005,-0.003,0.001}; observer O={0.1,-0.2,0.03},
 * W={0.002,0.004,-0.001} AU/day modern AU, Sun fixed at origin;
 * case 1 linear, case 2 adds A={0.0001,-0.00005,0.00002}*d^2 to
 * the observer; T0=2451545.5, coverage [2451545.0, 2451546.0],
 * TT inputs {2451545.0,0.5} and {2451545.0,0.6}; velocity V-W.
 */
#ifndef LIBJPL_NUMERICAL_REFERENCE_H
#define LIBJPL_NUMERICAL_REFERENCE_H

#include <stddef.h>

typedef struct jpl_time_reference {
    double tt[2], tdb_minus_tt_seconds, icrs_to_ecliptic[3][3];
} jpl_time_reference;
typedef struct jpl_apparent_reference {
    double tt[2], state[6], delay_days;
} jpl_apparent_reference;

static const jpl_time_reference jpl_time_references[] = {
    { { 0x1.2b42c8p+21, 0x0p+0 }, -0x1.a0864afd79ecp-14, { { 0x1.fffffffffffcbp-1, -0x1.30037d6267512p-24, 0x1.5a03026ad0b89p-24 }, { 0x1.1a9546e9bfe13p-25, 0x1.d5c037bd4c5ffp-1, 0x1.9752da8eda501p-2 }, { -0x1.b663a4c38f2b3p-24, -0x1.9752da8eda4a9p-2, 0x1.d5c037bd4c5e6p-1 } } },
    { { 0x1.2b4524p+21, 0x0p+0 }, 0x1.99f51961ff3c5p-10, { { 0x1.fffffff513dc3p-1, -0x1.84522543b6548p-15, -0x1.4f9e302dd08dep-16 }, { 0x1.a7072bf4637c8p-15, 0x1.d5c03df62c2d4p-1, 0x1.9752bda3ed142p-2 }, { -0x1.01a28a3293fp-24, -0x1.9752bdac9158p-2, 0x1.d5c03e0034398p-1 } } },
    { { 0x1.2b4ac4p+21, 0x0p+0 }, -0x1.8e13072187174p-10, { { 0x1.ffffff830785bp-1, -0x1.482d241361ec2p-13, -0x1.1cbf37d04354ap-14 }, { 0x1.65ba9373eebb5p-13, 0x1.d5c04c7b07ac6p-1, 0x1.9752786c687e3p-2 }, { 0x1.5ae22e0eecp-25, -0x1.975278cfefef9p-2, 0x1.d5c04cedaa311p-1 } } },
    { { -0x1.7a6b04p+21, 0x1p-40 }, 0x1.9e0799e2629b5p-11, { { -0x1.c42f8a7e32318p-1, -0x1.b720b2c66730ep-2, -0x1.8520d017cdc6ep-3 }, { 0x1.e00bef2b61816p-2, -0x1.96771a9464dcep-1, -0x1.8c9455e426236p-2 }, { 0x1.f36f90d0bb25p-7, -0x1.b975bbfb2b608p-2, 0x1.cde91c53ef11cp-1 } } },
    { { 0x1.e84802p+22, 0x1p-40 }, 0x1.ff2e405e49037p-13, { { -0x1.a393417adfceep-1, 0x1.0cc265182cf63p-1, 0x1.d7115167eb2a8p-3 }, { -0x1.2533a5ced4b8fp-1, -0x1.8816f4ee8586cp-1, -0x1.2b9dab6704a82p-2 }, { 0x1.718e717439ec8p-6, -0x1.7c692ac96a5c8p-2, 0x1.db37e96e26ce3p-1 } } }
};
static const size_t jpl_time_reference_count = 5;

static const jpl_apparent_reference jpl_apparent_references[] = {
    { { 0x1.2b42c8p+21, 0x1p-1 }, { 0x1.ccc78f32fe0e2p-1, 0x1.33373d7ff1aa2p-1, -0x1.d70e259c0e5e7p-3, 0x1.89374bc6a7efap-9, -0x1.cac083126e979p-8, 0x1.0624dd2f1a9fcp-9 }, 0x1.a2901c814d785p-8 },
    { { 0x1.2b42c8p+21, 0x1.3333333333333p-1 }, { 0x1.cceec2a59c09fp-1, 0x1.32db8b58a8fa3p-1, -0x1.d6a5604f33da7p-3, 0x1.86983515e9e1bp-9, -0x1.ca18bd663f141p-8, 0x1.059ea57227d02p-9 }, 0x1.a27ee9935cc97p-8 }
};
static const size_t jpl_apparent_reference_count = 2;

#endif
