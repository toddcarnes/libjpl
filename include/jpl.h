/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#ifndef LIBJPL_JPL_H
#define LIBJPL_JPL_H

/**
 * @file jpl.h
 * @brief Portable reader for JPL Development Ephemeris (DE) binary files.
 * @note License: first-party code is MIT, copyright (c) 2026 Todd Carnes;
 *       see LICENSE in the project root. Frame/time routines use vendored
 *       ERFA v2.0.1 (NumFOCUS Foundation, BSD-3-Clause); see
 *       src/erfa/LICENSE. Both texts install under
 *       ${CMAKE_INSTALL_DATADIR}/licenses/libjpl/. ERFA declarations are
 *       private and never exposed through this header.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Title line length in chars (84). */
#define JPL_TITLE_LINE_LENGTH 84
/** @brief Constant name length in chars (6). */
#define JPL_CONSTANT_NAME_LENGTH 6
/* The legacy binary header reserves 400 six-character constant-name fields.
 * Modern DE files can contain more numeric constants than named fields. */
/** @brief Maximum number of named constants (400). */
#define JPL_MAX_CONSTANT_NAMES 400
/** @brief Legacy alias kept for compatibility; prefer JPL_MAX_CONSTANT_NAMES.
 *  @note The constants record itself is NOT limited to 400 entries:
 *        header.constant_count may exceed it, so size value buffers from
 *        header.constant_count, not from this macro. */
#define JPL_MAX_CONSTANTS JPL_MAX_CONSTANT_NAMES
/** @brief Number of coefficient-layout file slots (12, including nutations). */
#define JPL_TARGET_COUNT 12

/**
 * @brief Status code returned by libjpl functions.
 */
typedef enum jpl_status {
    JPL_OK = 0,          /**< @brief Success. */
    JPL_ERROR_ARGUMENT,  /**< @brief Invalid argument (NULL pointer, short buffer, bad target/center). */
    JPL_ERROR_IO,        /**< @brief File I/O error. */
    JPL_ERROR_FORMAT,    /**< @brief Invalid JPL DE format. */
    JPL_ERROR_RANGE,     /**< @brief Index or Julian date out of range. */
    JPL_ERROR_MEMORY,    /**< @brief Out of memory. */
    JPL_ERROR_UNSUPPORTED, /**< @brief Requested dataset absent, or convenience output frame not verified as ICRF-aligned. */
    JPL_ERROR_CONVERGENCE, /**< @brief A valid bounded solve failed its stopping criteria within the iteration limit. */
    JPL_ERROR_NUMERIC    /**< @brief Finite validated inputs nevertheless produced an unrepresentable numerical result. */
} jpl_status;

/**
 * @brief Byte order of the DE binary file, auto-detected by jpl_open().
 */
typedef enum jpl_byte_order {
    JPL_BYTE_ORDER_LITTLE_ENDIAN,  /**< @brief Little-endian file layout. */
    JPL_BYTE_ORDER_BIG_ENDIAN      /**< @brief Big-endian file layout. */
} jpl_byte_order;

/**
 * @brief JPL target identifiers used by jpl_get_state().
 * @note Numbering follows the JPL PLEPH/testpo convention (see the
 *       SUBROUTINE PLEPH header in JPL testeph.f): 3 is Earth and
 *       13 is the Earth-Moon barycenter.
 */
typedef enum jpl_target {
    JPL_MERCURY = 1,             /**< @brief Mercury (1). */
    JPL_VENUS,                   /**< @brief Venus (2). */
    JPL_EARTH,                   /**< @brief Earth (3), derived from barycenter and geocentric Moon. */
    JPL_MARS,                    /**< @brief Mars (4). */
    JPL_JUPITER,                 /**< @brief Jupiter (5). */
    JPL_SATURN,                  /**< @brief Saturn (6). */
    JPL_URANUS,                  /**< @brief Uranus (7). */
    JPL_NEPTUNE,                 /**< @brief Neptune (8). */
    JPL_PLUTO,                   /**< @brief Pluto (9). */
    JPL_MOON,                    /**< @brief Moon (10), derived from barycenter and geocentric Moon. */
    JPL_SUN,                     /**< @brief Sun (11). */
    JPL_SOLAR_SYSTEM_BARYCENTER, /**< @brief Solar-system barycenter (12). */
    JPL_EARTH_MOON_BARYCENTER = 13, /**< @brief Earth-Moon barycenter (13). */
    JPL_NUTATIONS,               /**< @brief Nutations (14): longitude and obliquity, radians. */
    JPL_LIBRATIONS,              /**< @brief Librations (15): three lunar libration angles, radians. */
    JPL_TT_TDB = 17,               /**< @brief TT-TDB time correction (17): seconds, from *t files. */
    JPL_SSB = 0                  /**< @brief Center alias: 0 means the solar-system barycenter. */
} jpl_target;

/**
 * @brief Orientation family of the DE coefficient frame.
 * @note Identifies orientation families only, not a transformation engine
 *       and not a claim of identical ICRF realizations. Unknown values
 *       remain readable by core native-frame APIs; ICRS convenience
 *       functions require JPL_FRAME_ICRF and return JPL_ERROR_UNSUPPORTED
 *       otherwise. Classification follows JPL's export information
 *       (https://ssd.jpl.nasa.gov/planets/eph_export.html): DE102 is
 *       referred to the dynamical equator and equinox of 1950, DE200/DE202
 *       to the dynamical equator and equinox of 2000, and the documented
 *       modern releases below are ICRF-aligned.
 */
typedef enum jpl_reference_frame {
    JPL_FRAME_UNKNOWN = 0,      /**< @brief Unverified or unlisted ephemeris number. */
    JPL_FRAME_DYNAMICAL_1950,   /**< @brief Dynamical equator/equinox of 1950 (DE102). */
    JPL_FRAME_DYNAMICAL_2000,   /**< @brief Dynamical equator/equinox of 2000 (DE200/DE202). */
    JPL_FRAME_ICRF              /**< @brief ICRF-aligned modern release (explicit allowlist). */
} jpl_reference_frame;

/**
 * @brief A 1-based coefficient index and its Chebyshev-series dimensions.
 */
typedef struct jpl_pointer {
    int start_index; /**< @brief 1-based index of the first coefficient in a data record (<= 0 means absent). */
    int coefficients_per_component; /**< @brief Chebyshev coefficients per component per subinterval. */
    int subintervals; /**< @brief Subintervals per record for this target. */
} jpl_pointer;

/**
 * @brief Parsed DE header: coverage, constants metadata, and coefficient layout.
 */
typedef struct jpl_header {
    char titles[3][JPL_TITLE_LINE_LENGTH + 1]; /**< @brief Three 84-character title lines (NUL-terminated). */
    char constant_names[JPL_MAX_CONSTANT_NAMES][JPL_CONSTANT_NAME_LENGTH + 1]; /**< @brief Up to 400 six-character constant names (NUL-terminated). */
    double start_jd[2]; /**< @brief Start of ephemeris coverage as a two-part Julian date (sum of both parts). */
    double end_jd[2]; /**< @brief End of ephemeris coverage as a two-part Julian date (sum of both parts). */
    double step_days; /**< @brief Span in days covered by one data record. */
    int constant_count; /**< @brief Number of numeric constants in the constants record. */
    double astronomical_unit_km; /**< @brief Original header AU in km (metadata only; public body states use the modern AU 149597870.7 km, not this field). */
    double earth_moon_mass_ratio; /**< @brief Earth-Moon mass ratio (used to derive Earth and Moon states). */
    jpl_pointer pointers[JPL_TARGET_COUNT]; /**< @brief File slots Mercury through Sun (including EMB in slot 3) followed by nutations in slot 12; these are not the public body-code indices. */
    int ephemeris_number; /**< @brief DE ephemeris number (e.g. 441 for DE441). */
    jpl_reference_frame reference_frame; /**< @brief Orientation family derived from ephemeris_number via an explicit cited allowlist (ICRF, dynamical 1950/2000, or unknown). Core readers accept every frame natively; ICRS convenience requires ICRF. */
    jpl_pointer libration_pointer; /**< @brief Coefficient layout for librations (target 15). */
    jpl_pointer tt_tdb_pointer; /**< @brief Coefficient layout for TT-TDB (target 17, *t files only). */
    int has_tt_tdb; /**< @brief Nonzero iff the file carries the TT-TDB dataset. */
    size_t record_double_count; /**< @brief Doubles per data record, including the two Julian-date bounds. */
    jpl_byte_order byte_order; /**< @brief Detected byte order of the open file. */
} jpl_header;

/** @brief Opaque ephemeris handle. Obtain via jpl_open()/jpl_open_mmap(), release with jpl_close(). */
typedef struct jpl_ephemeris jpl_ephemeris;

/**
 * @brief Returns a human-readable description of a status code.
 * @param status Status code from any libjpl function.
 * @return Pointer to a static string; "unknown error" if status is out of range. Never NULL.
 */
const char *jpl_status_string(jpl_status status);

/**
 * @brief Opens a standard JPL DE binary file.
 * @param path Filesystem path to DE binary (e.g. "DE441.eph").
 * @param out_ephemeris Receives allocated handle on success; set to NULL
 *        on failure after non-NULL arguments are validated. If path is
 *        NULL, an existing value is left unchanged.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if path/out is NULL,
 *         JPL_ERROR_IO on file errors, JPL_ERROR_FORMAT on bad header,
 *         JPL_ERROR_MEMORY on allocation failure.
 * @note Byte order is auto-detected. Close with jpl_close().
 * @note The open path is resolved to an absolute reopen path when possible,
 *       so changing the working directory cannot redirect a clone. If
 *       resolution fails, a clone still checks the reopened file's identity
 *       and size against the original stream (POSIX device+inode, Windows
 *       volume/file index), rejecting a different file with JPL_ERROR_IO.
 *       Contents must stay immutable while readers are active: identity
 *       checks do not snapshot-isolate in-place edits.
 */
jpl_status jpl_open(const char *path, jpl_ephemeris **out_ephemeris);

/**
 * @brief Closes an ephemeris handle and releases its resources.
 * @param ephemeris Handle from jpl_open(), jpl_open_mmap(), or
 *        jpl_ephemeris_clone(); NULL is a no-op.
 * @note Each handle must be closed exactly once; closing twice is
 *       use-after-free. A single handle must not be shared across threads
 *       on the FILE path (see jpl_get_state).
 */
void jpl_close(jpl_ephemeris *ephemeris);

/**
 * @brief Returns the parsed header of an open ephemeris.
 * @param ephemeris Open handle, or NULL.
 * @return Pointer to the header, valid while the handle is open; NULL if ephemeris is NULL.
 * @note Each clone has its own header copy; do not use after closing that handle.
 */
const jpl_header *jpl_get_header(const jpl_ephemeris *ephemeris);

/**
 * @brief Reads the constants record of an open ephemeris.
 * @param ephemeris Open handle.
 * @param values Destination array with at least header.constant_count slots.
 * @param value_count Number of slots available in values.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if ephemeris/values is NULL
 *         or value_count is smaller than header.constant_count,
 *         JPL_ERROR_IO on file errors, JPL_ERROR_MEMORY on allocation failure.
 * @note Raw I/O: the destination buffer may be partially written after an
 *       I/O failure (unlike computed-state outputs, which are
 *       transactional).
 */
jpl_status jpl_read_constants(const jpl_ephemeris *ephemeris, double *values, size_t value_count);

/**
 * @brief Reads one complete coefficient record, including its two Julian-date bounds.
 * @param ephemeris Open handle.
 * @param record_index Zero-based data record index (less than jpl_record_count()).
 * @param values Destination array with at least header.record_double_count slots.
 * @param value_count Number of slots available in values.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if ephemeris/values is NULL
 *         or value_count is too small, JPL_ERROR_RANGE if record_index is out
 *         of range, JPL_ERROR_IO on file errors, JPL_ERROR_MEMORY on allocation failure.
 * @note Raw I/O: the destination buffer carries the encoded on-disk scalar
 *       epoch fields verbatim and may be partially written after an I/O
 *       failure (unlike computed-state outputs, which are transactional).
 */
jpl_status jpl_read_record(const jpl_ephemeris *ephemeris, size_t record_index,
                           double *values, size_t value_count);

/**
 * @brief Returns the number of data records in an open ephemeris.
 * @param ephemeris Open handle, or NULL.
 * @return Number of data records; 0 if ephemeris is NULL.
 */
size_t jpl_record_count(const jpl_ephemeris *ephemeris);

/**
 * @brief Interpolates TT minus TDB at a TDB Julian date (*t files).
 * @param ephemeris Open handle.
 * @param jd_tdb Julian date (TDB) as a two-part array within header coverage.
 * @param seconds Output TT-TDB in seconds. Must be non-NULL.
 * @param rate_per_day Optional output rate of change in seconds/day, may be NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if ephemeris/seconds/jd_tdb
 *         is NULL or either date part is nonfinite,
 *         JPL_ERROR_RANGE if jd_tdb is outside coverage (coverage bounds
 *         query validity, not model accuracy),
 *         JPL_ERROR_UNSUPPORTED if the file carries no TT-TDB dataset
 *         (check jpl_has_tt_tdb()), JPL_ERROR_FORMAT for malformed record
 *         metadata or nonfinite file coefficients, JPL_ERROR_NUMERIC when
 *         finite inputs produce an unrepresentable result, JPL_ERROR_IO
 *         or JPL_ERROR_MEMORY from the record read.
 * @note Computed outputs are transactional: on failure *seconds and
 *       *rate_per_day (when non-NULL) are left unchanged.
 * @note Coefficients are stored in seconds and the record span is in days,
 *       so no unit conversion is applied. For files without the dataset use
 *       jpl_tt_to_tdb() (geocentric Fairhead & Bretagnon model, roughly
 *       1.7 ms amplitude, +/-3 ns model accuracy over 1950-2050 per ERFA)
 *       instead.
 */
jpl_status jpl_get_tt_minus_tdb(const jpl_ephemeris *ephemeris, const double jd_tdb[2],
                                 double *seconds, double *rate_per_day);

/**
 * @brief Reports whether the open file carries the TT-TDB dataset.
 * @param ephemeris Open handle, or NULL.
 * @return Nonzero iff the TT-TDB coefficients are present (*t files);
 *         0 if ephemeris is NULL or the dataset is absent.
 * @note Content-derived, never filename-derived: DENUM carries no t suffix.
 */
int jpl_has_tt_tdb(const jpl_ephemeris *ephemeris);

/**
 * @brief Interpolates a JPL target at a TDB Julian date.
 * @param ephemeris Open handle.
 * @param jd_tdb Julian date (TDB) as a two-part array within header coverage.
 * @param target Target body as a jpl_target value (1-15; TT-TDB 17 is served
 *        by jpl_get_tt_minus_tdb()).
 * @param center Center 0 (SSB alias) or a body code 1-13 for ordinary
 *        targets; nutations (14) and librations (15) require center 0.
 * @param state Output array of 6 slots receiving the relative state.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if ephemeris/state/jd_tdb
 *         is NULL, either date part is nonfinite, or the target/center
 *         combination is invalid (target 0 and center 14/15 are invalid;
 *         target 17 must use jpl_get_tt_minus_tdb());
 *         JPL_ERROR_RANGE if jd_tdb is outside coverage (the file span is
 *         not a model-accuracy guarantee); JPL_ERROR_UNSUPPORTED if the
 *         requested dataset is absent for a valid in-coverage query;
 *         JPL_ERROR_FORMAT for malformed record metadata or nonfinite file
 *         coefficients;
 *         JPL_ERROR_NUMERIC when finite validated inputs produce an
 *         unrepresentable result (including a finite two-part date whose
 *         mathematical sum is unrepresentable); JPL_ERROR_IO on file
 *         errors, JPL_ERROR_MEMORY on allocation failure.
 * @note Computed outputs are transactional: on failure the caller's state
 *       array is left unchanged, and no partial component is published.
 *       Failed interpolations never poison the FILE-path record cache.
 *       Raw record/constant reads are the exception: jpl_read_record() and
 *       jpl_read_constants() destination buffers may be partially written
 *       after an I/O failure.
 * @note For targets 1-13, state contains x,y,z (AU) then vx,vy,vz (AU/day)
 *       in the modern AU (149597870.7 km), independent of the header AU.
 *       Nutations (14) return dpsi, deps in state[0], state[1] and their
 *       rates (rad/day) in state[3], state[4]; state[2] and state[5] are
 *       zero. Librations (15) return three angles (rad) and three rates
 *       (rad/day). Both require center 0. For TT-TDB (17) use
 *       jpl_get_tt_minus_tdb().
 * @note A single handle must not be shared across threads on the FILE
 *       path: reads update a per-handle record cache, so sharing is a
 *       data race. The mmap path performs no per-handle mutation and is
 *       safe for concurrent reads on one handle; clones can be used on
 *       separate threads if the file stays immutable. Synchronize cloning
 *       against closing the source (see jpl_ephemeris_clone()).
 */
jpl_status jpl_get_state(const jpl_ephemeris *ephemeris, const double jd_tdb[2],
                          int target, int center, double state[6]);

/**
 * @brief Interpolates a light-time-corrected relative state without aberration.
 * @param ephemeris Open handle from jpl_open(), jpl_open_mmap(), or a clone.
 * @param jd_tdb TDB Julian date of observation as a two-part array.
 * @param target Target jpl_target (1-13).
 * @param center Center jpl_target (1-13), or 0 for SSB.
 * @param state Output array of 6 slots receiving the relative state.
 * @param out_light_time Optional output light-time in days, may be NULL.
 * @return JPL_OK; JPL_ERROR_ARGUMENT on NULL handle/state/jd_tdb, nonfinite
 *         jd_tdb parts, or target outside 1-13 or center outside 0-13;
 *         JPL_ERROR_RANGE on out-of-coverage jd or when no in-coverage
 *         emission epoch solves the delay equation (genuinely pre-coverage
 *         root under the subluminal-motion contract);
 *         JPL_ERROR_CONVERGENCE when a bounded in-coverage solve exhausts
 *         its fast/bracketed iteration budget without an accepted epoch;
 *         JPL_ERROR_FORMAT for nonfinite file coefficients or nonphysical
 *         (superluminal) evaluated file motion; JPL_ERROR_NUMERIC when
 *         finite inputs produce an unrepresentable result;
 *         JPL_ERROR_UNSUPPORTED, JPL_ERROR_IO, or JPL_ERROR_MEMORY from
 *         the underlying state reads.
 * @note Holds the observer barycentric state fixed at reception and solves
 *       tau = range(target(receive - tau), observer(receive))/c with the
 *       emission epoch as a two-part pair. Up to 10 fast fixed-point
 *       iterations run first; on coverage crossing or stagnation a
 *       sign-checked bisection over emission epochs in
 *       [coverage_start, receive] (at most 128 steps) recovers valid
 *       near-start solutions. State and delay come from one accepted
 *       emission evaluation with residual |tau - range/c| <= 1e-12 d.
 *       The 1e-12-day residual is a solver tolerance, not an
 *       astronomical accuracy claim: Chebyshev interpolation and model
 *       error dominate real ephemeris uncertainty.
 *       Only bodies 1-13 are accepted as targets and 0-13 as centers;
 *       target 0 is always invalid; nutations (14) and librations (15)
 *       have no light-time meaning and return JPL_ERROR_ARGUMENT.
 *       State velocity is the mixed-epoch difference
 *       v_target(emission) - v_center(reception); it is not the derivative
 *       of apparent position with respect to reception time. Aberration is
 *       not applied (use jpl_apply_aberration_full with the observer
 *       velocity at reception).
 * @note Computed state and optional light-time output are unchanged on
 *       failure.
 */
jpl_status jpl_get_state_lighttime(const jpl_ephemeris *ephemeris, const double jd_tdb[2],
                                    int target, int center, double state[6], double *out_light_time);

/**
 * @brief Applies special-relativistic stellar aberration.
 * @param state Input geometric state, pos AU + vel AU/day. Must be non-NULL.
 * @param observer_vel Observer barycentric velocity AU/day. Must be non-NULL.
 * @param out_state Output aberrated state, may alias state. Must be non-NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers, nonfinite
 *         state/velocity slots, or observer speed at/above light speed,
 *         JPL_ERROR_NUMERIC when finite inputs produce an unrepresentable result.
 * @note Special-relativistic direction transform (cf. ERFA eraAb) with
 *       distance preservation; solar gravitational term (<=0.4uas) omitted;
 *       velocity passed through unchanged. In-place safe.
 *       For the <=0.4uas solar term use jpl_apply_aberration_full when
 *       the Sun distance is known.
 * @note Transactional: out_state is unchanged on failure. Exact
 *       state/out_state aliasing is supported.
 */
jpl_status jpl_apply_aberration(const double state[6], const double observer_vel[3], double out_state[6]);

/**
 * @brief Applies full special-relativistic stellar aberration including the
 *        solar gravitational potential term.
 * @param state Input geometric state, pos AU + vel AU/day. Must be non-NULL.
 * @param observer_vel Observer barycentric velocity AU/day. Must be non-NULL.
 * @param sun_distance_au Distance from the Sun to the observer in AU; finite
 *        nonpositive input omits the potential term (wrapper behavior),
 *        nonfinite input is invalid.
 * @param out_state Output aberrated state, may alias state. Must be non-NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers, nonfinite
 *         state/velocity slots, observer speed at/above light speed, or
 *         nonfinite Sun distance, JPL_ERROR_NUMERIC when finite inputs
 *         produce an unrepresentable result.
 * @note Matches ERFA eraAb (Explanatory Supplement Expr. 7.40 + Klioner
 *       2003 solar term) to ~1e-12 on unit vectors; velocity passed through
 *       unchanged. In-place safe. See also jpl_apply_aberration, the
 *       signature-stable wrapper that omits the solar term.
 * @note Prefer this over jpl_apply_aberration when the Sun distance is
 *       known; the wrapper omits the <=0.4uas solar term (singular for
 *       Sun-centered observers).
 * @note Transactional: out_state is unchanged on failure. Exact
 *       state/out_state aliasing is supported.
 */
jpl_status jpl_apply_aberration_full(const double state[6], const double observer_vel[3],
                               double sun_distance_au, double out_state[6]);

/**
 * @brief Opens a DE file via read-only memory mapping.
 * @param path Filesystem path.
 * @param out_ephemeris Receives handle on success; set to NULL on failure
 *        after non-NULL arguments are validated. If path is NULL, an
 *        existing value is left unchanged.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT for NULL path/output,
 *         JPL_ERROR_IO for file or mapping errors, JPL_ERROR_FORMAT for
 *         invalid file layout, JPL_ERROR_MEMORY on allocation failure.
 * @note The file must not change while mapped (size is re-checked after
 *       mapping, but concurrent truncation may still fault on access).
 *       The mmap path performs no per-handle mutation, so a single mmap
 *       handle is shareable for concurrent reads; clones (via
 *       jpl_ephemeris_clone()) are still safe, and each handle must be
 *       closed. Concurrent reads on distinct handles are safe; external
 *       sync required for clone-vs-close races. An mmap clone shares
 *       the existing mapping without reopening the path. Contents must
 *       stay immutable while any reader is active; in-place edits are
 *       not snapshot-isolated.
 */
jpl_status jpl_open_mmap(const char *path, jpl_ephemeris **out_ephemeris);
/**
 * @brief Clones a handle for concurrent read access.
 * @param ephem Source handle.
 * @param out_clone Receives a new handle on success; NULL on failure.
 *        Mmap clones share the mapping, while FILE clones reopen an
 *        independent stream.
 * @return JPL_OK or JPL_ERROR_ARGUMENT (NULL args, corrupt handle),
 *         JPL_ERROR_MEMORY on allocation failure, or JPL_ERROR_IO if the
 *         FILE-handle clone cannot re-open the path (including after the
 *         original path was renamed or deleted: clones are not promised
 *         to survive path disappearance).
 * @note Each clone must be closed with jpl_close(); underlying mapping unmaps at zero refcount. Concurrent reads on distinct handles are safe; external sync required for clone-vs-close races.
 * @note FILE clones open an independent stream at the stored absolute
 *       reopen path and verify its identity (device/inode on POSIX,
 *       volume/file index on Windows) plus size against the still-open
 *       source before the clone is published. A replacement file or a
 *       missing path reports JPL_ERROR_IO with NULL output, never
 *       replacement data. In-place content mutation is unsupported. The
 *       source handle must stay open (externally synchronized) for the
 *       duration of the clone call; no handle may be closed while a
 *       reader is using it.
 */
jpl_status jpl_ephemeris_clone(const jpl_ephemeris *ephem, jpl_ephemeris **out_clone);

#ifdef __cplusplus
}
#endif

#endif
