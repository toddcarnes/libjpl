#ifndef LIBJPL_FRAMES_H
#define LIBJPL_FRAMES_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Rotate an ecliptic-of-date state to ICRS.
 * @param state Input state, position (AU) + velocity (AU/day) in ecliptic of date. Must be non-NULL.
 * @param jd_tt TT Julian date used to evaluate bias/precession/obliquity.
 * @param out_state Output state in ICRS, may alias state. Must be non-NULL.
 * @note Date-dependent (equinox of date, not fixed J2000): the matrix is the
 *       transpose of the IAU 2006 ICRS-to-ecliptic matrix (eraEcm06) at jd_tt,
 *       combining frame bias + precession (eraPmat06) with mean obliquity
 *       (eraObl06).
 * @note Aliasing-safe: position/velocity are rotated via locals before store.
 */
void jpl_ecliptic_to_icrs(const double state[6], double jd_tt, double out_state[6]);
/**
 * @brief Rotate an ICRS state to ecliptic of date.
 * @param state Input state, position (AU) + velocity (AU/day) in ICRS. Must be non-NULL.
 * @param jd_tt TT Julian date used to evaluate bias/precession/obliquity.
 * @param out_state Output state in ecliptic of date, may alias state. Must be non-NULL.
 * @note Inverse of jpl_ecliptic_to_icrs; same equinox-of-date convention.
 * @note Aliasing-safe: position/velocity are rotated via locals before store.
 */
void jpl_icrs_to_ecliptic(const double state[6], double jd_tt, double out_state[6]);
/**
 * @brief Ecliptic-of-date-to-ICRS rotation matrix R[3][3].
 * @param jd_tt TT Julian date used to evaluate bias/precession/obliquity.
 * @param R Output 3x3 rotation matrix mapping ecliptic vectors to ICRS. Must be non-NULL.
 * @note Equinox-of-date convention: transpose of eraEcm06 at jd_tt
 *       (bias + precession + mean obliquity, IAU 2006).
 */
void jpl_frame_rotation_matrix(double jd_tt, double R[3][3]);
/**
 * @brief TT Julian date to TDB (approximate, via ERFA).
 * @param jd_tt TT Julian date.
 * @return Approximate TDB Julian date.
 * @note Calls eraDtdb with zero observer location/velocity (geocenter
 *       approximation per spec); good to ~2 ms, not for precision timing.
 */
double jpl_tt_to_tdb(double jd_tt);
/**
 * @brief TDB Julian date to TT (approximate).
 * @param jd_tdb TDB Julian date.
 * @return Approximate TT Julian date.
 * @note Inverse of jpl_tt_to_tdb with the same eraDtdb zero-args approximation.
 */
double jpl_tdb_to_tt(double jd_tdb);
#ifdef __cplusplus
}
#endif
#endif
