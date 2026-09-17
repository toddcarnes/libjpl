#ifndef LIBJPL_CONVENIENCE_H
#define LIBJPL_CONVENIENCE_H
#include "jpl.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Apparent planet position (light-time + stellar aberration, ICRS).
 * @param ephem Open handle.
 * @param jd_tt TT Julian date.
 * @param planet Target body: 1-11 or 13 (planets 1-9, EMB 3, Moon 10,
 *        Sun 11, Earth 13; rejects SSB 12, nutations 14, librations 15,
 *        and out-of-range values).
 * @param center Center body: 0 (SSB) or 1-13 (rejects negatives and
 *        anything above 13).
 * @param state Output array of 6 slots receiving position (AU) + velocity
 *        (AU/day) in ICRS. DE records are already ICRS-equatorial, so no
 *        frame rotation is applied.
  * @return JPL_OK on success, JPL_ERROR_ARGUMENT for invalid planet/center
  *         or NULL handle/state, otherwise the underlying light-time/state
  *         error (RANGE/FORMAT/IO/MEMORY).
 * @note If planet == center the result is the zero vector (JPL_OK).
 *       Planet SSB (12) is rejected here; use jpl_get_state() for SSB geometry.
 * @note state[3..5] is the retarded-epoch velocity, passed through
 *       aberration unchanged.
  */
jpl_status jpl_planet_position(const jpl_ephemeris *ephem, double jd_tt, int planet, int center, double state[6]);
/**
 * @brief Apparent Moon position, Earth-centered ICRS.
 * @param ephem Open handle.
 * @param jd_tt TT Julian date.
 * @param state Output array of 6 slots receiving position (AU) + velocity
 *        (AU/day) in ICRS, Earth-centered.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT for NULL handle/state.
 * @note Moon+Earth vector addition mixes legs with different emission
 *       epochs and aberration corrections; it is approximate (km-level for
 *       moving centers). For consistent apparent Moon-Sun use geometric
 *       jpl_get_state() legs at a common epoch plus manual light-time/aberration,
 *       and note state[3..5] below are retarded-epoch, unaberrated velocities.
 */
jpl_status jpl_moon_position(const jpl_ephemeris *ephem, double jd_tt, double state[6]);
/** @brief Convert velocity from AU/day to km/s.
 *  @param vel_au_day Input triple, must be non-NULL.
 *  @param vel_km_s Output triple, must be non-NULL. */
void jpl_au_per_day_to_km_per_s(const double vel_au_day[3], double vel_km_s[3]);
/**
 * @brief Convert distance from km to AU.
 * @param km Distance in km.
 * @param au Output distance in AU (km divided by 149597870.7); ignored if NULL.
 */
void jpl_km_to_au(double km, double *au);
#ifdef __cplusplus
}
#endif
#endif
