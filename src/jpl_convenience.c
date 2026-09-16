#include "jpl_convenience.h"
#include "jpl_private.h"
#include "jpl_frames.h"
#include <string.h>
/* DE coefficient records are Cartesian in the ICRS equatorial frame, so
 * jpl_get_state output is already ICRS: no rotation is applied here. The
 * frames module stays for user-side ecliptic conversion. */
void jpl_au_per_day_to_km_per_s(const double v[3], double o[3]) {
    const double K = JPL_AU_KM/JPL_DAYSEC;
    o[0]=v[0]*K; o[1]=v[1]*K; o[2]=v[2]*K;
}
void jpl_km_to_au(double km, double *au) { if(au) *au = km/JPL_AU_KM; }
jpl_status jpl_planet_position(const jpl_ephemeris *e, double jd_tt, int planet, int center, double st[6]) {
    double tdb, lt, tmp[6], vobs[3], cst[6];
    jpl_status s;
    if(!e||!st) return JPL_ERROR_ARGUMENT;
    /* Planets 1-9, Moon 10, Sun 11, Earth 13. Reject SSB 12 and the
     * nutation (14) / libration (15) sensitivities, 0, and out-of-range. */
    if(!((planet>=1&&planet<=11)||planet==13)) return JPL_ERROR_ARGUMENT;
    /* Centers: 0 (SSB) or bodies 1-13. Reject negatives and anything above 13. */
    if(center<0||center>13) return JPL_ERROR_ARGUMENT;
    tdb = jpl_tt_to_tdb(jd_tt);
    s = jpl_get_state_lighttime(e, tdb, planet, center, tmp, &lt);
    if(s!=JPL_OK) return s;
    /* Aberration needs the observer (center-body) velocity at the retarded
     * emission epoch, which the light-time call above does not return, so
     * fetch it with one extra state evaluation at tdb-lt. */
    if(center!=0){ s=jpl_get_state(e, tdb-lt, center, 0, cst); if(s!=JPL_OK) return s;
        vobs[0]=cst[3];vobs[1]=cst[4];vobs[2]=cst[5];
    } else { vobs[0]=vobs[1]=vobs[2]=0.0; }
    jpl_apply_aberration(tmp, vobs, tmp);
    memcpy(st, tmp, sizeof tmp);
    return JPL_OK;
}
jpl_status jpl_moon_position(const jpl_ephemeris *e, double jd_tt, double st[6]) {
    return jpl_planet_position(e, jd_tt, 10, 13, st);
}
