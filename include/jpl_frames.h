/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#ifndef LIBJPL_FRAMES_H
#define LIBJPL_FRAMES_H
#include "jpl.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @file jpl_frames.h
 * @brief Date-dependent ecliptic/ICRS frame helpers and TT/TDB conversions.
 * @note License: first-party code is MIT, copyright (c) 2026 Todd Carnes;
 *       see LICENSE in the project root. ERFA is used privately by the
 *       implementation; no ERFA declaration is exposed here.
 */
/**
 * @brief Rotate an ecliptic-of-date state to ICRS.
 * @param state Input state, position (AU) + velocity (AU/day) in ecliptic of date. Must be non-NULL.
 * @param jd_tt TT Julian date as a two-part array used to evaluate bias/precession/obliquity.
 * @param out_state Output state in ICRS, may alias state. Must be non-NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers or nonfinite
 *         date parts or nonfinite state slots, JPL_ERROR_NUMERIC when finite
 *         inputs produce an unrepresentable result.
 * @note Date-dependent (equinox of date, not fixed J2000): the matrix is the
 *       transpose of the IAU 2006 ICRS-to-ecliptic matrix (eraEcm06) at jd_tt,
 *       combining frame bias + precession (eraPmat06) with mean obliquity
 *       (eraObl06).
 * @note Aliasing-safe: position/velocity are rotated via locals before store.
 * @note Velocity is rotated as a direction at fixed jd_tt (R.v);
 *       frame-rate dR/dt terms from precession are not included
 *       (~50"/yr x 1 AU ~= 1 m/s).
 */
jpl_status jpl_ecliptic_to_icrs(const double state[6], const double jd_tt[2], double out_state[6]);
/**
 * @brief Rotate an ICRS state to ecliptic of date.
 * @param state Input state, position (AU) + velocity (AU/day) in ICRS. Must be non-NULL.
 * @param jd_tt TT Julian date as a two-part array used to evaluate bias/precession/obliquity.
 * @param out_state Output state in ecliptic of date, may alias state. Must be non-NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers or nonfinite
 *         date parts or nonfinite state slots, JPL_ERROR_NUMERIC when finite
 *         inputs produce an unrepresentable result.
 * @note Inverse of jpl_ecliptic_to_icrs; same equinox-of-date convention.
 * @note Aliasing-safe: position/velocity are rotated via locals before store.
 * @note Velocity is rotated as a direction at fixed jd_tt (R.v);
 *       frame-rate dR/dt terms from precession are not included
 *       (~50"/yr x 1 AU ~= 1 m/s).
 */
jpl_status jpl_icrs_to_ecliptic(const double state[6], const double jd_tt[2], double out_state[6]);
/**
 * @brief Ecliptic-of-date-to-ICRS rotation matrix R[3][3].
 * @param jd_tt TT Julian date as a two-part array used to evaluate bias/precession/obliquity.
 * @param R Output 3x3 rotation matrix mapping ecliptic vectors to ICRS. Must be non-NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers or nonfinite
 *         date parts, JPL_ERROR_NUMERIC when finite inputs produce an
 *         unrepresentable result.
 * @note Equinox-of-date convention: transpose of eraEcm06 at jd_tt
 *       (bias + precession + mean obliquity, IAU 2006).
 */
jpl_status jpl_frame_rotation_matrix(const double jd_tt[2], double R[3][3]);
/**
 * @brief TT Julian date to TDB (approximate, via ERFA).
 * @param jd_tt TT Julian date as a two-part array.
 * @param jd_tdb Output TDB Julian date as a two-part array. May alias jd_tt.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers or nonfinite
 *         date parts, JPL_ERROR_NUMERIC on unrepresentable arithmetic.
 * @note Geocentric Fairhead & Bretagnon (1990) model via ERFA eraDtdb with
 *       zero observer location/velocity (topocentric terms nullified).
 *       The TT-TDB correction amplitude is roughly 1.7 ms (main annual
 *       term about 1.66 ms plus planetary/lunar/diurnal terms to tens of
 *       microseconds); ERFA cites better than +/-3 ns model accuracy over
 *       1950-2050 relative to DE405 numerical integrations, omitting
 *       topocentric terms. This is model accuracy, not arithmetic
 *       precision: normalized two-part dates and compensated duration
 *       helpers keep rounding far below the model envelope inside the
 *       validated interval. Outside 1950-2050 no high-accuracy promise
 *       is made. Ephemeris-interpolated TT-TDB from *t files
 *       (jpl_get_tt_minus_tdb) is the definitive source when present.
 *       For interpolated dataset TT-TDB from *t files see
 *       jpl_get_tt_minus_tdb().
 */
jpl_status jpl_tt_to_tdb(const double jd_tt[2], double jd_tdb[2]);
/**
 * @brief TDB Julian date to TT (approximate).
 * @param jd_tdb TDB Julian date as a two-part array.
 * @param jd_tt Output TT Julian date as a two-part array. May alias jd_tdb.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL pointers or nonfinite
 *         date parts, JPL_ERROR_CONVERGENCE when the residual does not reach
 *         1e-15 day within 10 iterations, JPL_ERROR_NUMERIC on
 *         unrepresentable arithmetic.
 * @note Bounded iteration on two-part epochs; residual target 1e-15 day,
 *       at most 10 iterations. The 1e-15-day residual target is an
 *       arithmetic stopping criterion, far below the millisecond-scale
 *       ERFA model envelope; it is not a physical accuracy claim.
 */
jpl_status jpl_tdb_to_tt(const double jd_tdb[2], double jd_tt[2]);
#ifdef __cplusplus
}
#endif
#endif
