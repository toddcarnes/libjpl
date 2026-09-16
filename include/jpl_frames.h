#ifndef LIBJPL_FRAMES_H
#define LIBJPL_FRAMES_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Rotate an ecliptic J2000 state to ICRS.
 * @param state Input state, position (AU) + velocity (AU/day) in ecliptic J2000.
 * @param jd_tt TT Julian date used to evaluate bias/precession/obliquity.
 * @param out_state Output state in ICRS, may alias state.
 * @note J2000-mean-equator convention: the matrix is the transpose of the
 *       IAU 2006 ICRS-to-ecliptic matrix (eraEcm06) at jd_tt, combining frame
 *       bias + precession (eraPmat06) with mean obliquity (eraObl06).
 * @note Aliasing-safe: position/velocity are rotated via locals before store.
 */
void jpl_ecliptic_to_icrs(const double state[6], double jd_tt, double out_state[6]);
/**
 * @brief Rotate an ICRS state to ecliptic J2000.
 * @param state Input state, position (AU) + velocity (AU/day) in ICRS.
 * @param jd_tt TT Julian date used to evaluate bias/precession/obliquity.
 * @param out_state Output state in ecliptic J2000, may alias state.
 * @note Inverse of jpl_ecliptic_to_icrs; same J2000-mean-equator convention.
 * @note Aliasing-safe: position/velocity are rotated via locals before store.
 */
void jpl_icrs_to_ecliptic(const double state[6], double jd_tt, double out_state[6]);
/**
 * @brief Ecliptic-to-ICRS rotation matrix R[3][3].
 * @param jd_tt TT Julian date used to evaluate bias/precession/obliquity.
 * @param R Output 3x3 rotation matrix mapping ecliptic vectors to ICRS.
 * @note J2000-mean-equator convention: transpose of eraEcm06 at jd_tt
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
