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
#include <stdio.h>

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
/* File-slot index (0-based) of the EMB coefficients in header.pointers.
 * File slots follow coefficient order (Mercury..Sun, EMB third), while body
 * codes follow PLEPH (Earth 3, EMB 13): the two coincide for every body
 * except Earth/EMB, so the EMB slot is named explicitly. */
#define JPL_FILE_SLOT_EMB 2

/* Private header layout. Callers outside the library use the jpl_header_*()
 * accessor set; the binding adapters no longer mirror this struct. */
struct jpl_header {
    char titles[3][JPL_TITLE_LINE_LENGTH + 1];
    char constant_names[JPL_MAX_CONSTANT_NAMES][JPL_CONSTANT_NAME_LENGTH + 1];
    double start_jd[2];
    double end_jd[2];
    double step_days;
    int constant_count;
    double astronomical_unit_km;
    double earth_moon_mass_ratio;
    jpl_pointer pointers[JPL_TARGET_COUNT];
    int ephemeris_number;
    jpl_reference_frame reference_frame;
    jpl_pointer libration_pointer;
    jpl_pointer tt_tdb_pointer;
    int has_tt_tdb;
    size_t record_double_count;
    jpl_byte_order byte_order;
};

/* Private header view for white-box tests; not installed or exported. */
const jpl_header *jpl_private_get_header(const jpl_ephemeris *e);

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

/* Atomic light-time geometry for one target/center/epoch. Everything a
 * convenience caller needs, computed once in coverage-then-read order:
 * the retarded relative state, the solved delay, the observer's
 * reception-epoch velocity, the observer-to-Sun vector, and the emission
 * epoch (tdb minus delay via compensated subtraction). Mixing reception
 * and emission inputs is a type-level impossibility for callers: they
 * read named fields instead of threading three epochs by hand.
 * sun_vec is all zeros when the center is the Sun (no Sun leg needed). */
typedef struct jpl_geometry {
    jpl_state rel_state;
    double light_time_days;
    double observer_vel[3];
    double sun_vec[3];
    jpl_date emission;
} jpl_geometry;

/* Apparent-place pipeline stage types (src/jpl_convenience.c). Values flow
 * geometric state -> Direction -> deflected -> aberrated -> true ecliptic;
 * past construction no bare double[3] crosses a stage seam. */
/* A finite unit vector. The only supported way to obtain one is
 * jpl_direction_from_raw, which normalizes finite nonzero input exactly
 * once; NULL/nonfinite/zero input fails instead of flowing silently into
 * eraLd (regression guard for the 2026-10-31 Sun-to-observer e-vector
 * fix). Outputs are transactional: out is untouched unless JPL_OK. */
typedef struct jpl_direction { double v[3]; } jpl_direction;
static inline jpl_status jpl_direction_from_raw(const double in[3], jpl_direction *out) {
    double norm;
    size_t i;
    if (!in || !out) return JPL_ERROR_ARGUMENT;
    for (i = 0; i < 3; ++i) {
        if (!isfinite(in[i])) return JPL_ERROR_ARGUMENT;
    }
    norm = jpl_vec_norm3(in);
    if (!isfinite(norm) || !(norm > 0.0)) return JPL_ERROR_NUMERIC;
    out->v[0] = in[0] / norm;
    out->v[1] = in[1] / norm;
    out->v[2] = in[2] / norm;
    return JPL_OK;
}
/* Exact negation of a constructed direction (Sun->observer from
 * observer->Sun for the eraLd e argument). Both sides are already
 * directions, so there is nothing to validate: in and out must be
 * non-NULL and may alias. */
static inline void jpl_direction_negate(const jpl_direction *in, jpl_direction *out) {
    out->v[0] = -in->v[0];
    out->v[1] = -in->v[1];
    out->v[2] = -in->v[2];
}
/* Longitude-rate estimator policy: central, one-sided, and adaptive-h
 * finite differences over a caller-supplied sampler. Extracted from
 * jpl_apparent_ecliptic_position so the one-sided/adaptive-h branches are
 * unit-testable without ephemeris data.
 * sample(ctx, tt, days, position) reports the apparent ecliptic
 * position[3] (longitude deg, latitude deg, range AU) at tt+days, or
 * JPL_ERROR_RANGE past coverage (any other error aborts the estimate).
 * center_longitude_deg is the already-sampled rate-epoch longitude.
 * Returns the longitude rate in deg/day, or RANGE when every shrinking-h
 * sample stays out of coverage. rate_out is untouched on failure. */
typedef jpl_status (*jpl_ecliptic_sampler)(void *ctx, const jpl_date *tt,
                                           double days, double position[3]);
jpl_status jpl_estimate_longitude_rate(jpl_ecliptic_sampler sample, void *ctx,
                                       const jpl_date *tt,
                                       double center_longitude_deg,
                                       double *rate_out);

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
 * serialization boundary only.
 *
 * Trust-boundary audit (2026-11-24, followup S3): every
 * jpl_date_normalize call site names the boundary it guards, so future
 * reviews re-check this list instead of re-deriving it.
 * - Entry funnels (public API + jpl_entry_prepare): user-supplied pairs
 *   (possibly unnormalized); the single per-query funnel.
 * - Utility funnels (jpl_date_add/subtract/add_days/add_scaled/
 *   jpl_date_partition): directly callable, including by tests, with raw
 *   pairs (test_dates feeds {1.5, 0.25} and {-0.0, -0.0}). Their
 *   normalization IS the funnel; internal call sites additionally pass
 *   normalized pairs, pinned by add_core's asserts on every Debug run.
 * - select_record: reached through the gate AND directly via its test
 *   hook, so it keeps its own normalize + coverage (the hook's
 *   far-beyond-coverage case is load-bearing: RANGE with outputs
 *   untouched).
 * - Scalar construction (add_days step, add_scaled/partition offsets,
 *   subtract's negated pair): computed pairs normalized once at
 *   construction; sign-flip can introduce -0.0, so negation
 *   re-normalizes.
 * - Serialization boundary (header/record words in parse, open,
 *   select_record's header reads, verify_record_bounds,
 *   scheduled_endpoint): untrusted file content normalized on read;
 *   never re-normalized downstream.
 * - jpl_date_compare stays lexicographic over ALL finite pairs (bare
 *   finiteness assert): it is chronological only on normalized inputs,
 *   which is exactly why every consumer above funnels first. Do not
 *   "unify" its assert with JPL_ASSERT_NORMALIZED; the two spellings
 *   name different contracts. */
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

/* Entry gate: validate → normalize → coverage, owned once.
 * Every query entry point funnels its input epoch through
 * jpl_entry_prepare (NULL/finiteness checks, input + header-bound
 * normalization; header failure is FORMAT) and checks coverage with
 * jpl_entry_coverage (RANGE) before any dataset or frame gating
 * (UNSUPPORTED). Domain conversions between the two calls (e.g. TT→TDB
 * in convenience_epoch) keep their current position: coverage is always
 * checked on the epoch the data reads use, so RANGE still precedes
 * UNSUPPORTED everywhere. Callers keep their own code/budget/output
 * checks (all ARGUMENT). ctx contents are undefined unless JPL_OK. */
typedef struct jpl_entry_ctx {
    double norm[2];
    double cov_start[2];
    double cov_end[2];
} jpl_entry_ctx;

/* The gate's verified data epoch: normalized, inside the header coverage,
 * and safe to hand to every internal leg. A query prepares its caller epoch
 * once, applies any domain conversion (TT -> TDB), and then verifies the
 * exact epoch the data reads use. Internal legs consume this token instead
 * of re-normalizing or re-checking coverage. */
typedef struct jpl_verified_epoch {
    double epoch[2];
    double cov_start[2];
    double cov_end[2];
} jpl_verified_epoch;

jpl_status jpl_entry_prepare(const jpl_ephemeris *e, const double jd[2],
                             jpl_entry_ctx *out);
jpl_status jpl_entry_coverage(const jpl_entry_ctx *ctx,
                              const double data_epoch[2],
                              jpl_verified_epoch *out_verified);

/* Internal verified-epoch entry points used by the convenience pipeline
 * after its TT -> TDB conversion. They validate body codes but perform no
 * date preparation or coverage re-check. */
jpl_status jpl_get_state_verified(const jpl_ephemeris *e,
                                  const jpl_verified_epoch *epoch,
                                  int target, int center, jpl_state *state);
jpl_status jpl_get_lighttime_solution_verified(const jpl_ephemeris *e,
                                               const jpl_verified_epoch *epoch,
                                               int target, int center,
                                               jpl_lighttime_solution *out);

/* Byte-source seam: FILE and mmap submit raw bytes through one interface
 * so every open path and every record read shares one validation and
 * transport policy. Every read checks `offset <= size` and
 * `count <= size - offset` with subtraction guarded by the first check;
 * mmap memcpy and FILE seek/read share error classification (IO vs
 * FORMAT stays with the candidate evaluator). Adapters are defined in
 * src/jpl.c; the seam is declared here so tests can drive sources
 * directly. Parser reads go through parser_read_bytes (bounded-read
 * hook); record-endpoint probes are a constant 16 bytes and share the
 * same bound. */
typedef jpl_status (*jpl_read_bytes_fn)(void *context, size_t offset,
                                        unsigned char *out, size_t count);
typedef struct jpl_byte_source {
    void *context;
    size_t size;
    jpl_read_bytes_fn read;
} jpl_byte_source;
typedef struct jpl_mmap_source { const unsigned char *base; size_t size; } jpl_mmap_source;
typedef struct jpl_file_source { FILE *file; size_t size; } jpl_file_source;

/* Raw byte-source adapters shared by the parser and RecordStore. */
jpl_status jpl_file_read_bytes(void *context, size_t offset,
                               unsigned char *out, size_t count);
jpl_status jpl_mmap_read_bytes(void *context, size_t offset,
                               unsigned char *out, size_t count);

/* A decoded record view. values is borrowed until the next store call on the
 * same handle (or until release for the swapped-mmap case); owned is non-NULL
 * only when the caller must release a per-call decode buffer. */
typedef struct jpl_record_view {
    const double *values;
    double *owned;
} jpl_record_view;

/* RecordStore: source selection, decoded record access, and the per-handle
 * single-record cache. The store is embedded in the handle; FILE reads use
 * the cache, native-order mmap reads point into the mapping, and swapped
 * files decode through a per-call buffer (or the FILE cache). */
typedef struct jpl_record_store {
    jpl_byte_source source;
    jpl_file_source file;
    jpl_mmap_source mmap;
    int is_mmap;
    jpl_byte_order byte_order;
    struct {
        double *record;
        size_t index;
        int valid;
    } cache;
} jpl_record_store;

jpl_status jpl_store_init_file(jpl_record_store *store, FILE *file,
                               size_t size, jpl_byte_order byte_order);
jpl_status jpl_store_init_mmap(jpl_record_store *store,
                               const unsigned char *base, size_t size,
                               jpl_byte_order byte_order);
jpl_status jpl_store_read_raw(const jpl_record_store *store, size_t offset,
                              void *out, size_t count);
jpl_status jpl_store_read_doubles(const jpl_record_store *store, size_t offset,
                                  double *values, size_t count);
jpl_status jpl_store_record(jpl_record_store *store, size_t record_doubles,
                            size_t index, jpl_record_view *out);
void jpl_record_view_release(jpl_record_view *view);

/* RecordSchedule: scheduled endpoints and the epoch -> record ->
 * subinterval mapping. */
typedef struct jpl_record_span {
    size_t index;
    double start[2];
    double end[2];
} jpl_record_span;

typedef struct jpl_subinterval_span {
    size_t index;
    double origin[2];
    double substep;
    double argument;
} jpl_subinterval_span;

jpl_status jpl_schedule_endpoint(const double start[2], double step,
                                 size_t index, double out[2]);
jpl_status jpl_schedule_record(const jpl_header *header, size_t covered_count,
                               const jpl_verified_epoch *epoch,
                               jpl_record_span *out);
jpl_status jpl_schedule_verify_record(size_t index, const double record_dates[2],
                                      const jpl_verified_epoch *epoch,
                                      jpl_record_span *out);
jpl_status jpl_schedule_subinterval(const jpl_record_span *span, size_t nsub,
                                    const jpl_verified_epoch *epoch,
                                    jpl_subinterval_span *out);

/* BodyCatalog: one validity/execution table per query mode. */
typedef enum jpl_query_mode {
    JPL_QUERY_STATE,
    JPL_QUERY_LIGHTTIME,
    JPL_QUERY_PLANET,
    JPL_QUERY_APPARENT
} jpl_query_mode;

typedef enum jpl_body_kind {
    JPL_BODY_DIRECT,      /* one file slot */
    JPL_BODY_EMB_MOON,    /* EMB plus geocentric Moon derivation */
    JPL_BODY_ANGULAR,     /* nutations or librations */
    JPL_BODY_ZERO         /* SSB / zero vector */
} jpl_body_kind;

typedef struct jpl_body_plan {
    jpl_body_kind kind;
    const jpl_pointer *primary;
    const jpl_pointer *secondary;
    size_t components;
    int target;
} jpl_body_plan;

jpl_status jpl_body_lookup(const jpl_header *header, int body,
                           jpl_body_plan *out);
jpl_status jpl_body_plan_for(jpl_query_mode mode, const jpl_header *header,
                             int target, int center, jpl_body_plan *out);

/* Validated TT-TDB availability: present means the coefficient span fits the
 * record. Built once at open; the public flag, query, and refinement policy
 * consume this single fact. */
typedef struct jpl_tt_tdb_table {
    int present;
    jpl_pointer pointer;
    size_t span_end;
} jpl_tt_tdb_table;

jpl_status jpl_tt_tdb_table_build(const jpl_header *header,
                                  jpl_tt_tdb_table *out);
int jpl_tt_tdb_span_end(const jpl_pointer *pointer, size_t *out);

/* TT→TDB policy: file data when present and in coverage, else the model.
 * The model converts first (seed and guaranteed baseline); when the file
 * carries TT-TDB and the seed is in coverage, fixed-point refinements
 * TDB = TT - f(TDB)/86400 converge the millisecond-scale correction to
 * arithmetic noise. Any file-path failure (absent/span-invalid/corrupt
 * data, out-of-coverage seed) keeps the model result: the file is an
 * enhancement, never a new failure mode. Deterministic: fixed iteration
 * count, no convergence error. Callers keep their own NULL checks. */
jpl_status jpl_tt_to_tdb_policy(const jpl_ephemeris *e, const jpl_date *tt,
                                jpl_date *tdb);

/* ICRS position to IAU 2006/2000A true ecliptic and equinox of TT date.
 * A pure rotation; the public mean-of-date state helpers are unchanged. */
jpl_status jpl_icrs_to_true_ecliptic_position(const double in[3],
                                               const jpl_date *jd_tt, double out[3]);

#endif
