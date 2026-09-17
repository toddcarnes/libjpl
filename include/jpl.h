#ifndef LIBJPL_JPL_H
#define LIBJPL_JPL_H

/**
 * @file jpl.h
 * @brief Portable reader for JPL Development Ephemeris (DE) binary files.
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
/** @brief Number of coefficient-layout targets (12). */
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
    JPL_ERROR_MEMORY     /**< @brief Out of memory. */
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
 */
typedef enum jpl_target {
    JPL_MERCURY = 1,             /**< @brief Mercury (1). */
    JPL_VENUS,                   /**< @brief Venus (2). */
    JPL_EARTH_MOON_BARYCENTER,   /**< @brief Earth-Moon barycenter (3). */
    JPL_MARS,                    /**< @brief Mars (4). */
    JPL_JUPITER,                 /**< @brief Jupiter (5). */
    JPL_SATURN,                  /**< @brief Saturn (6). */
    JPL_URANUS,                  /**< @brief Uranus (7). */
    JPL_NEPTUNE,                 /**< @brief Neptune (8). */
    JPL_PLUTO,                   /**< @brief Pluto (9). */
    JPL_MOON,                    /**< @brief Moon (10), derived from barycenter and geocentric Moon. */
    JPL_SUN,                     /**< @brief Sun (11). */
    JPL_SOLAR_SYSTEM_BARYCENTER, /**< @brief Solar-system barycenter (12). */
    JPL_EARTH,                   /**< @brief Earth (13), derived from barycenter and geocentric Moon. */
    JPL_NUTATIONS,               /**< @brief Nutations (14): longitude and obliquity, radians. */
    JPL_LIBRATIONS,              /**< @brief Librations (15): three lunar libration angles, radians. */
    JPL_SSB = 0                  /**< @brief Center alias: 0 means the solar-system barycenter. */
} jpl_target;

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
    double start_jd; /**< @brief Start of ephemeris coverage (Julian date). */
    double end_jd; /**< @brief End of ephemeris coverage (Julian date). */
    double step_days; /**< @brief Span in days covered by one data record. */
    int constant_count; /**< @brief Number of numeric constants in the constants record. */
    double astronomical_unit_km; /**< @brief Astronomical unit in km (used to scale jpl_get_state output to AU). */
    double earth_moon_mass_ratio; /**< @brief Earth-Moon mass ratio (used to derive Earth and Moon states). */
    jpl_pointer pointers[JPL_TARGET_COUNT]; /**< @brief Coefficient layout for targets 1-12 (nutations reuse the last entry). */
    int ephemeris_number; /**< @brief DE ephemeris number (e.g. 441 for DE441). */
    jpl_pointer libration_pointer; /**< @brief Coefficient layout for librations (target 15). */
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
 * @param out_ephemeris Receives allocated handle on success, NULL on failure.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if path/out is NULL,
 *         JPL_ERROR_IO on file errors, JPL_ERROR_FORMAT on bad header.
 * @note Byte order is auto-detected. Close with jpl_close().
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
 * @brief Interpolates a JPL target at a TDB Julian date.
 * @param ephemeris Open handle.
 * @param jd_tdb Julian date (TDB) within header coverage.
 * @param target Target body as a jpl_target value (1-15).
 * @param center Center body as a jpl_target value; may also be 0 for the
 *        solar-system barycenter.
 * @param state Output array of 6 slots receiving the relative state.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if ephemeris/state is NULL
 *         or the target/center combination is invalid, JPL_ERROR_RANGE if
 *         jd_tdb is outside coverage, JPL_ERROR_FORMAT on corrupt records,
 *         JPL_ERROR_IO on file errors, JPL_ERROR_MEMORY on allocation failure.
 * @note For targets 1-13, state contains x,y,z (AU) then vx,vy,vz (AU/day).
 *       Nutations (14) return dpsi, deps in state[0], state[1] and their
 *       rates in state[3], state[4]; state[2] and state[5] are zero.
 *       Librations (15) return three angles and three rates. Both require
 *       center 0.
 *       A single handle must not be shared across threads on the FILE
 *       path: reads update a per-handle record cache, so sharing is a
 *       data race. The mmap path performs no per-handle mutation and is
 *       safe for concurrent reads on one handle; either way,
 *       jpl_ephemeris_clone() per thread is always safe.
 */
jpl_status jpl_get_state(const jpl_ephemeris *ephemeris, double jd_tdb,
                         int target, int center, double state[6]);

/**
 * @brief Interpolates apparent geometric state corrected for light-time.
 * @param ephemeris Open handle from jpl_open().
 * @param jd_tdb TDB Julian date of observation.
 * @param target Target jpl_target (1-13).
 * @param center Center jpl_target, or 0 for SSB.
 * @param state Output array of 6 slots receiving the relative state.
 * @param out_light_time Optional output light-time in days, may be NULL.
 * @return JPL_OK; JPL_ERROR_ARGUMENT on NULL handle/state or target/center
 *         outside 0-13; JPL_ERROR_RANGE on out-of-coverage jd or
 *         non-convergence after 10 iters; JPL_ERROR_FORMAT/IO/MEMORY from
 *         the underlying state reads.
 * @note Iterates jd - light_time until delta < 1e-12 d or 10 iters.
 *       Only bodies 0-13 are accepted; nutations (14) and librations (15)
 *       have no light-time meaning and return JPL_ERROR_ARGUMENT.
 */
jpl_status jpl_get_state_lighttime(const jpl_ephemeris *ephemeris, double jd_tdb,
                                   int target, int center, double state[6], double *out_light_time);

/**
 * @brief Applies special-relativistic stellar aberration.
 * @param state Input geometric state, pos AU + vel AU/day. NULL is a no-op.
 * @param observer_vel Observer barycentric velocity AU/day. NULL is a no-op.
 * @param out_state Output aberrated state, may alias state. NULL is a no-op.
 * @note Special-relativistic direction transform (cf. ERFA eraAb) with
 *       distance preservation; solar gravitational term (<=0.4uas) omitted;
 *       velocity passed through unchanged. In-place safe.
 *       Known limitation, deferred by design: the effect is ~10x below
 *       the ~5uas velocity-knowledge floor, threading Sun distance through
 *       would churn the API and go singular when the observer is at the
 *       Sun (center==Sun).
 */
void jpl_apply_aberration(const double state[6], const double observer_vel[3], double out_state[6]);

/**
 * @brief Applies full special-relativistic stellar aberration including the
 *        solar gravitational potential term.
 * @param state Input geometric state, pos AU + vel AU/day. NULL is a no-op.
 * @param observer_vel Observer barycentric velocity AU/day. NULL is a no-op.
 * @param sun_distance_au Distance from the Sun to the observer in AU; must
 *        be positive and finite (guarded: non-positive input falls back to
 *        jpl_apply_aberration behavior).
 * @param out_state Output aberrated state, may alias state. NULL is a no-op.
 * @note Matches ERFA eraAb (Explanatory Supplement Expr. 7.40 + Klioner
 *       2003 solar term) to ~1e-12 on unit vectors; velocity passed through
 *       unchanged. In-place safe. See also jpl_apply_aberration, the
 *       signature-stable wrapper that omits the solar term.
 * @note Prefer this over jpl_apply_aberration when the Sun distance is
 *       known; the wrapper omits the <=0.4uas solar term (singular for
 *       Sun-centered observers).
 */
void jpl_apply_aberration_full(const double state[6], const double observer_vel[3],
                               double sun_distance_au, double out_state[6]);

/**
 * @brief Opens a DE file via read-only memory mapping.
 * @param path Filesystem path.
 * @param out_ephemeris Receives handle.
 * @return JPL_OK or JPL_ERROR_ARGUMENT/IO/FORMAT/MEMORY.
 * @note The file must not change while mapped (size is re-checked after
 *       mapping, but concurrent truncation may still fault on access).
 *       The mmap path performs no per-handle mutation, so a single mmap
 *       handle is shareable for concurrent reads; clones (via
 *       jpl_ephemeris_clone()) are still safe, and each handle must be
 *       closed. Concurrent reads on distinct handles are safe; external
 *       sync required for clone-vs-close races.
 */
jpl_status jpl_open_mmap(const char *path, jpl_ephemeris **out_ephemeris);
/**
 * @brief Clones a handle for concurrent read access.
 * @param ephem Source handle.
 * @param out_clone Receives new handle sharing mapped data.
 * @return JPL_OK or JPL_ERROR_ARGUMENT (NULL args, corrupt handle),
 *         JPL_ERROR_MEMORY on allocation failure, or JPL_ERROR_IO if the
 *         FILE-handle clone cannot re-open the path.
 * @note Each clone must be closed with jpl_close(); underlying mapping unmaps at zero refcount. Concurrent reads on distinct handles are safe; external sync required for clone-vs-close races.
 */
jpl_status jpl_ephemeris_clone(const jpl_ephemeris *ephem, jpl_ephemeris **out_clone);

#ifdef __cplusplus
}
#endif

#endif
