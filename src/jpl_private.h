/* Internal shared constants and helpers for libjpl sources.
 * NOT installed; tests use tests/jpl_test_util.h instead. */
#ifndef LIBJPL_PRIVATE_H
#define LIBJPL_PRIVATE_H

#include <math.h>

/* IAU / JPL physical constants (exact values used across the library). */
#define JPL_AU_KM 149597870.7
#define JPL_DAYSEC 86400.0
#define JPL_C_AU_PER_DAY 173.144632684
/* Two-part Julian-date split used by ERFA-style calls. */
#define JPL_DJM0 2400000.5

/* File-slot index (0-based) of the EMB coefficients in header.pointers.
 * File slots follow coefficient order (Mercury..Sun, EMB third), while body
 * codes follow PLEPH (Earth 3, EMB 13): the two coincide for every body
 * except Earth/EMB, so the EMB slot is named explicitly. */
#define JPL_FILE_SLOT_EMB 2

/* Schwarzschild radius of the Sun in AU (ERFA ERFA_SRS = 1.97412574336e-8). */
#define JPL_SRS_AU 1.97412574336e-8

/* Light-time solver: tolerance-based convergence per design spec. */
#define JPL_LIGHTTIME_TOL_DAYS 1e-12
#define JPL_LIGHTTIME_MAX_ITER 10

/* Julian-date coverage plausibility window (DE431/DE441 span millennia). */
#define JPL_PLAUSIBLE_JD_MIN -100000000.0
#define JPL_PLAUSIBLE_JD_MAX 100000000.0

/* Header/record layout field offsets (bytes) in the 2856-byte header.
 * Kept next to the parser for documentation; parse_header still advances
 * a cursor (p += 8/4) so the two must agree when the layout changes. */
#define JPL_HDR_TITLES_LEN 252
#define JPL_HDR_TEXT_LEN 2400

/* Base file offset of the IPT 14/15 extension triplets when the file
 * carries no overflow constant names (constant_count <= 400). Files with
 * more constants store the extra 6-character names first, so the triplets
 * start at JPL_TT_TDB_TAIL_BASE + 6 * (constant_count - 400). */
#define JPL_TT_TDB_TAIL_BASE 2856

/* Upper bound on a plausible TT-TDB dataset span end (1-based slot count).
 * Genuine *t datasets end near slot ~1000; a 1M-slot cap accepts every
 * real file while rejecting garbage u32 triplets in pre-extension headers
 * (lenient-absent semantics: garbage reads as absent, never FORMAT). */
#define JPL_TT_TDB_SPAN_LIMIT 1048576

static inline double jpl_vec_norm3(const double v[3]) {
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

#endif
