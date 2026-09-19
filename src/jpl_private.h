/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Internal shared constants and helpers for libjpl sources.
 * NOT installed; tests use tests/jpl_test_util.h instead. */
#ifndef LIBJPL_PRIVATE_H
#define LIBJPL_PRIVATE_H

#include <math.h>

#include "jpl.h"

/* IAU / JPL physical constants (exact values used across the library).
 * The modern AU and SI light speed are the exact inputs; light speed in
 * AU/day is derived from them, never a rounded literal. */
#define JPL_AU_KM 149597870.7
#define JPL_DAYSEC 86400.0
#define JPL_C_KM_PER_SECOND 299792.458
#define JPL_C_AU_PER_DAY (JPL_C_KM_PER_SECOND * JPL_DAYSEC / JPL_AU_KM)
/* Two-part Julian-date split used by ERFA-style calls. */
#define JPL_DJM0 2400000.5

/* File-slot index (0-based) of the EMB coefficients in header.pointers.
 * File slots follow coefficient order (Mercury..Sun, EMB third), while body
 * codes follow PLEPH (Earth 3, EMB 13): the two coincide for every body
 * except Earth/EMB, so the EMB slot is named explicitly. */
#define JPL_FILE_SLOT_EMB 2

/* Schwarzschild radius of the Sun in AU (ERFA ERFA_SRS = 1.97412574336e-8). */
#define JPL_SRS_AU 1.97412574336e-8

/* Light-time solver: tolerance-based convergence per design spec. The
 * public wrapper supplies JPL_LIGHTTIME_FAST_MAX_ITER fixed-point steps
 * plus JPL_LIGHTTIME_SAFE_MAX_ITER bracketed steps. */
#define JPL_LIGHTTIME_TOL_DAYS 1e-12
#define JPL_LIGHTTIME_FAST_MAX_ITER 10
#define JPL_LIGHTTIME_SAFE_MAX_ITER 128

/* Julian-date coverage plausibility window (DE431/DE441 span millennia). */
#define JPL_PLAUSIBLE_JD_MIN -100000000.0
#define JPL_PLAUSIBLE_JD_MAX 100000000.0

/* Header/record layout field offsets (bytes) in the 2880-byte base header.
 * The IPT 14/15 tail lives past it at JPL_TT_TDB_TAIL_BASE plus 6 bytes per
 * overflow constant name (constant_count - 400 when positive).
 * Kept next to the parser for documentation; parse_header still advances
 * a cursor (p += 8/4) so the two must agree when the layout changes. */
#define JPL_HDR_TITLES_LEN 252
#define JPL_HDR_TEXT_LEN 2400

/* Base file offset of the IPT 14/15 extension triplets when the file
 * carries no overflow constant names (constant_count <= 400). Files with
 * more constants store the extra 6-character names first, so the triplets
 * start at JPL_TT_TDB_TAIL_BASE + 6 * (constant_count - 400). */
#define JPL_TT_TDB_TAIL_BASE 2856

static inline double jpl_vec_norm3(const double v[3]) {
    /* Scale-safe Euclidean norm: nested hypot preserves tiny (1e-200)
     * directions and avoids overflow when the norm stays representable. */
    return hypot(hypot(v[0], v[1]), v[2]);
}

/* Compensated two-part Julian-date arithmetic (src/jpl_date.c).
 * The represented epoch is the mathematical sum of two finite binary64
 * values. Status-returning helpers validate finiteness, leave outputs
 * untouched on failure, and support exact input/output aliasing;
 * jpl_date_compare consumes normalized finite pairs only. */
jpl_status jpl_date_normalize(const double in[2], double out[2]);
jpl_status jpl_date_add(const double a[2], const double b[2], double out[2]);
jpl_status jpl_date_subtract(const double a[2], const double b[2], double out[2]);
jpl_status jpl_date_add_days(const double date[2], double days, double out[2]);
jpl_status jpl_date_add_scaled(const double date[2], double step,
                               size_t index, double out[2]);
jpl_status jpl_date_partition(const double begin[2], const double end[2],
                              size_t count, size_t index, double out[2]);
int jpl_date_compare(const double a[2], const double b[2]);

/* Private deterministic light-time iteration-budget hook for tests (T10).
 * Declared/defined only when LIBJPL_TESTING is set by the build for the
 * library plus its light-time tests; never declared in an installed
 * header and never exported from tests-OFF builds. The public solver
 * calls the same internal implementation with JPL_LIGHTTIME_FAST_MAX_ITER
 * and JPL_LIGHTTIME_SAFE_MAX_ITER. Budgets count that iteration family
 * only (zero fast + zero safeguarded forces CONVERGENCE on non-root
 * cases). Negative budgets are rejected as JPL_ERROR_ARGUMENT with no
 * output written. */
#ifdef LIBJPL_TESTING
jpl_status jpl_test_lighttime_with_budget(const jpl_ephemeris *ephemeris,
    const double jd[2], int target, int center, double state[6], double *delay,
    int fast_iterations, int safeguarded_iterations);
#endif

#endif
