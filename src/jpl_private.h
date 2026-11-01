/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Internal shared constants and helpers, not installed. Hook-dependent
 * tests include this header; mock builders live in tests/jpl_test_util.h. */
#ifndef LIBJPL_PRIVATE_H
#define LIBJPL_PRIVATE_H

#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "jpl.h"

/* libjpl requires 64-bit size_t to address multi-GiB ephemerides (DE431/DE441).
 * 32-bit builds are explicitly unsupported. */
#if UINTPTR_MAX != UINT64_MAX
#error "libjpl requires 64-bit size_t (multi-GiB ephemerides)"
#endif

/* IAU / JPL physical constants (exact values used across the library).
 * The modern AU and SI light speed are the exact inputs; light speed in
 * AU/day is derived from them, never a rounded literal. */
#define JPL_AU_KM 149597870.7
#define JPL_DAYSEC 86400.0
#define JPL_C_KM_PER_SECOND 299792.458
#define JPL_C_AU_PER_DAY (JPL_C_KM_PER_SECOND * JPL_DAYSEC / JPL_AU_KM)
/* MJD zero-point; never use it to split an already-rounded Julian date. */
#define JPL_DJM0 2400000.5

/* File-slot index (0-based) of the EMB coefficients in header.pointers.
 * File slots follow coefficient order (Mercury..Sun, EMB third), while body
 * codes follow PLEPH (Earth 3, EMB 13): the two coincide for every body
 * except Earth/EMB, so the EMB slot is named explicitly. */
#define JPL_FILE_SLOT_EMB 2

/* Schwarzschild radius of the Sun in AU (ERFA ERFA_SRS = 1.97412574336e-8). */
#define JPL_SRS_AU 1.97412574336e-8
/* Nominal solar radius from Horizons' Sun target radii, for limiting
 * deflection of a ray occulted by the solar disk. */
#define JPL_SOLAR_RADIUS_KM 695700.0

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
 * The IPT coefficient-pointer table starts at JPL_HDR_IPT_OFFSET: twelve
 * 12-byte (3x u32) entries, then the DENUM word and the libration triple.
 * parse_header walks these fields with a cursor (p += 8/4); the cursor
 * position is asserted against these constants (see parse_header), so the
 * two representations must agree when the layout changes. */
#define JPL_HDR_TITLES_LEN 252
#define JPL_HDR_TEXT_LEN 2400
#define JPL_HDR_IPT_OFFSET 2696
#define JPL_HDR_IPT_ENTRY_BYTES 12
#define JPL_HDR_TITLES_LEN 252
#define JPL_HDR_TEXT_LEN 2400
#define JPL_HDR_START_JD_OFFSET 2652
#define JPL_HDR_END_JD_OFFSET 2660
#define JPL_HDR_STEP_DAYS_OFFSET 2668
#define JPL_HDR_NCONST_OFFSET 2676
#define JPL_HDR_AU_KM_OFFSET 2680
#define JPL_HDR_EMRAT_OFFSET 2688
#define JPL_HDR_DENUM_OFFSET 2840
#define JPL_HDR_LIBRATION_OFFSET 2844
#define JPL_HDR_IPT14_OFFSET 2856

/* Base file offset of the IPT 14/15 extension triplets when the file
 * carries no overflow constant names (constant_count <= 400). Files with
 * more constants store the extra 6-character names first, so the triplets
 * start at JPL_TT_TDB_TAIL_BASE + 6 * (constant_count - 400). */
#define JPL_TT_TDB_TAIL_BASE 2856
#define JPL_TT_TDB_TRIPLET_OFFSET 12

/* Overflow-name tail base for `extra` constant names past the classic 400:
 * JPL_TT_TDB_TAIL_BASE + 6 * extra, computed with overflow checked.
 * Shared by the layout validator, the overflow-name reader, and the test
 * mock builders (single source; was re-derived in three places). Callers
 * bounds-check the result against their own record width (plus any trailing
 * bytes they consume) and keep their own error codes. */
static inline jpl_status jpl_tail_offset(size_t extra_names, size_t *out) {
    if (!out) return JPL_ERROR_ARGUMENT;
    if (extra_names > (SIZE_MAX - (size_t)JPL_TT_TDB_TAIL_BASE) /
            (size_t)JPL_CONSTANT_NAME_LENGTH)
        return JPL_ERROR_FORMAT;
    *out = (size_t)JPL_TT_TDB_TAIL_BASE +
        extra_names * (size_t)JPL_CONSTANT_NAME_LENGTH;
    return JPL_OK;
}

static inline double jpl_vec_norm3(const double v[3]) {
    /* Scale-safe Euclidean norm: nested hypot preserves tiny (1e-200)
     * directions and avoids overflow when the norm stays representable. */
    return hypot(hypot(v[0], v[1]), v[2]);
}

/* Conversion helpers between public struct types and internal double[2]/double[6] */
static inline void jpl_date_to_pair(const jpl_date *d, double out[2]) {
    out[0] = d->hi; out[1] = d->lo;
}
static inline void jpl_pair_to_date(const double in[2], jpl_date *d) {
    d->hi = in[0]; d->lo = in[1];
}
static inline void jpl_state_to_array(const jpl_state *s, double out[6]) {
    out[0] = s->x; out[1] = s->y; out[2] = s->z;
    out[3] = s->vx; out[4] = s->vy; out[5] = s->vz;
}
static inline void jpl_array_to_state(const double in[6], jpl_state *s) {
    s->x = in[0]; s->y = in[1]; s->z = in[2];
    s->vx = in[3]; s->vy = in[4]; s->vz = in[5];
}

/* Compensated two-part Julian-date arithmetic (src/jpl_date.c).
 * The represented epoch is the mathematical sum of two finite binary64
 * values. Status-returning helpers validate finiteness, leave outputs
 * untouched on failure, and support exact input/output aliasing.
 *
 * Normalization discipline (single source of truth for the date protocol):
 * every public entry point normalizes user-supplied dates exactly once via
 * jpl_date_normalize (the trust-boundary funnel: it accepts any finite
 * pair and publishes a canonical one). Past that funnel, internal producers
 * emit canonical pairs and internal pure arithmetic (add_core) asserts it
 * in debug builds via JPL_ASSERT_NORMALIZED. jpl_date_compare is
 * lexicographic over all finite pairs (pinned by tests/test_dates.c) --
 * callers pass it normalized pairs, where lexicographic order coincides
 * with chronological order. Re-normalizing an already-normal pair is
 * idempotent but pointless; do not add defensive re-normalizations inside
 * the library (each one must name the trust boundary it guards, or be
 * removed). Raw on-disk scalar epochs stay scalar; convert at the
 * serialization boundary only. */
jpl_status jpl_date_normalize(const double in[2], double out[2]);
jpl_status jpl_date_add(const double a[2], const double b[2], double out[2]);
jpl_status jpl_date_subtract(const double a[2], const double b[2], double out[2]);
jpl_status jpl_date_add_days(const double date[2], double days, double out[2]);
jpl_status jpl_date_add_scaled(const double date[2], double step,
                               size_t index, double out[2]);
jpl_status jpl_date_partition(const double begin[2], const double end[2],
                              size_t count, size_t index, double out[2]);
int jpl_date_compare(const double a[2], const double b[2]);

/* Normalized-pair predicate for debug assertions (see the discipline note
 * above). A finite pair is normalized when it is a fixed point of
 * jpl_date_normalize -- exactly what two_sum plus zero-canonicalization
 * produces, so every internal producer's output qualifies by construction.
 * Fixed-point comparison (rather than an epsilon/ULP bound) cannot
 * false-fire on legitimate pairs, including huge or tiny magnitudes.
 * Debug-only: JPL_ASSERT_NORMALIZED vanishes under NDEBUG like assert(). */
static inline int jpl_date_is_normalized(const double p[2]) {
    double n[2];
    if (!isfinite(p[0]) || !isfinite(p[1])) return 0;
    if (jpl_date_normalize(p, n) != JPL_OK) return 0;
    return n[0] == p[0] && n[1] == p[1];
}
#define JPL_ASSERT_NORMALIZED(p) assert(jpl_date_is_normalized(p))

/* ICRS position to IAU 2006/2000A true ecliptic and equinox of TT date.
 * A pure rotation; the public mean-of-date state helpers are unchanged. */
jpl_status jpl_icrs_to_true_ecliptic_position(const double in[3],
                                               const jpl_date *jd_tt, double out[3]);

#endif
