/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#ifndef LIBJPL_CONVENIENCE_H
#define LIBJPL_CONVENIENCE_H
#include "jpl.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @file jpl_convenience.h
 * @brief ICRS apparent-place planet/Moon helpers and modern-AU unit conversions.
 * @note License: first-party code is MIT, copyright (c) 2026 Todd Carnes;
 *       see LICENSE in the project root. No ERFA declaration is exposed here.
 */
/**
 * @brief Apparent planet position (light-time + stellar aberration, ICRS).
 * @param ephem Open handle.
 * @param jd_tt TT Julian date as a two-part array.
 * @param planet Target body: 1-11 or 13 (planets incl. Earth 3, Moon 10,
 *        Sun 11, EMB 13; rejects target 0, SSB 12, nutations 14,
 *        librations 15, and out-of-range values).
 * @param center Center body: 0 (SSB) or 1-13 (rejects negatives and
 *        anything above 13). Centers 0 and 12 are equivalent; both denote
 *        a zero-velocity SSB observer with no aberration/Sun lookup.
 * @param state Output array of 6 slots receiving position (AU) + velocity
 *        (AU/day) in ICRS. Accepted ICRF-aligned DE records are already
 *        equatorial; no frame rotation is applied.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT for invalid planet/center,
 *         NULL handle/state/jd_tt, or nonfinite date parts;
 *         JPL_ERROR_RANGE for out-of-coverage TDB (before frame gating);
 *         JPL_ERROR_NUMERIC for unrepresentable finite input or computation;
 *         JPL_ERROR_UNSUPPORTED for a non-ICRF frame (even for identical
 *         bodies) or missing data; JPL_ERROR_FORMAT, JPL_ERROR_IO,
 *         JPL_ERROR_MEMORY, or JPL_ERROR_CONVERGENCE from underlying calls.
 * @note Requires header.reference_frame == JPL_FRAME_ICRF. Core
 *       jpl_get_state() stays available for native-frame reads of legacy
 *       or unknown frames (DENUM 102 dynamical-1950, 200/202
 *       dynamical-2000, unlisted DENUM unknown); no historical-frame
 *       transformation is applied here. File coverage bounds query
 *       validity only: frame/time-model accuracy intervals are narrower
 *       and documented per model (see jpl_tt_to_tdb() and validation
 *       notes), never implied by coverage alone.
 * @note If planet == center the result is the zero vector (JPL_OK) after
 *       successful required-body validation, without unrelated Sun work.
 *       Planet SSB (12) is rejected here; use jpl_get_state() for SSB geometry.
 * @note Apparent means light-time (mixed-epoch velocity
 *       v_target(emission) - v_center(reception), not an apparent tracking
 *       rate) plus special-relativistic stellar aberration with the
 *       <=0.4uas solar-potential term (via jpl_apply_aberration_full)
 *       whenever the observer is neither the SSB nor the Sun itself; the
 *       term is not gravitational light bending. SSB-observer calls apply
 *       no aberration. state[3..5] is the retarded-epoch velocity, passed
 *       through aberration unchanged.
 * @note The body-center observer is the center body's barycenter at the
 *       reception epoch (centers 0/12: the SSB with zero velocity). Only
 *       the light-time plus aberration corrections above are applied:
 *       no deflection, Earth-orientation, refraction, or derivative terms.
 * @note state is unchanged on failure.
 */
jpl_status jpl_planet_position(const jpl_ephemeris *ephem, const double jd_tt[2], int planet, int center, double state[6]);
/**
 * @brief Apparent Moon position, Earth-centered ICRS.
 * @param ephem Open handle.
 * @param jd_tt TT Julian date as a two-part array.
 * @param state Output array of 6 slots receiving position (AU) + velocity
 *        (AU/day) in ICRS, Earth-centered.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT for NULL handle/state/jd_tt
 *         or nonfinite date parts; JPL_ERROR_RANGE for out-of-coverage TDB
 *         (before frame gating); JPL_ERROR_UNSUPPORTED for a non-ICRF
 *         frame or absent data; JPL_ERROR_NUMERIC for unrepresentable
 *         finite input or computation; JPL_ERROR_FORMAT, JPL_ERROR_IO,
 *         JPL_ERROR_MEMORY, or JPL_ERROR_CONVERGENCE from underlying calls.
 * @note Requires header.reference_frame == JPL_FRAME_ICRF via
 *       jpl_planet_position(). Unknown, dynamical-1950, and
 *       dynamical-2000 frames are rejected with JPL_ERROR_UNSUPPORTED;
 *       no historical-frame transformation is applied.
 * @note This is one Earth-observer-to-Moon light-time leg with one
 *       aberration correction, using jpl_planet_position(). The velocity
 *       state[3..5] is retarded-epoch and unaberrated.
 * @note If a caller adds this result to a separate apparent leg to
 *       construct a third-body direction, the independently corrected
 *       legs have different emission epochs; that sum is only an
 *       approximation. For consistent geometry, start with geometric
 *       jpl_get_state() legs and apply the desired corrections explicitly.
 * @note state is unchanged on failure.
 */
jpl_status jpl_moon_position(const jpl_ephemeris *ephem, const double jd_tt[2], double state[6]);
/** @brief Convert velocity from AU/day to km/s.
 *  @param vel_au_day Input triple, must be non-NULL.
 *  @param vel_km_s Output triple, must be non-NULL.
 *  @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL input/output or
 *          nonfinite input, JPL_ERROR_NUMERIC on unrepresentable output.
 *  @note Output is unchanged on failure. Uses the modern AU (149597870.7 km).
 *        Supports exact input/output aliasing. */
jpl_status jpl_au_per_day_to_km_per_s(const double vel_au_day[3], double vel_km_s[3]);
/**
 * @brief Convert distance from km to AU.
 * @param km Distance in km.
 * @param au Output distance in AU (km divided by 149597870.7); left
 *        unchanged when the call fails, including on NULL output.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT on NULL output or nonfinite
 *         input.
 * @note Output is unchanged on failure. Uses the modern AU definition. */
jpl_status jpl_km_to_au(double km, double *au);
#ifdef __cplusplus
}
#endif
#endif
