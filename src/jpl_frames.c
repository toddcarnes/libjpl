#include "jpl_frames.h"
#include "jpl_private.h"
#include "erfa.h"
#include <string.h>
void jpl_frame_rotation_matrix(double jd_tt, double R[3][3]) {
    /* IAU 2006 ICRS -> ecliptic matrix, then transpose for the inverse
     * (ecliptic -> ICRS). eraEcm06 internally composes frame bias +
     * precession (eraPmat06) with mean obliquity (eraObl06). */
    double ecm[3][3];
    eraEcm06(JPL_DJM0, jd_tt-JPL_DJM0, ecm);
    eraTr(ecm, R);
}
static void rotate_state(double R[3][3], const double s[6], double o[6]) {
    double p[3]={s[0],s[1],s[2]}, v[3]={s[3],s[4],s[5]}, pr[3], vr[3];
    eraRxp(R, p, pr); eraRxp(R, v, vr);
    memcpy(o, pr, 3*sizeof(double)); memcpy(o+3, vr, 3*sizeof(double));
}
void jpl_ecliptic_to_icrs(const double s[6], double jd_tt, double o[6]) {
    double R[3][3]; jpl_frame_rotation_matrix(jd_tt, R);
    rotate_state(R, s, o);
}
void jpl_icrs_to_ecliptic(const double s[6], double jd_tt, double o[6]) {
    double ecm[3][3];
    eraEcm06(JPL_DJM0, jd_tt-JPL_DJM0, ecm);
    rotate_state(ecm, s, o);
}
static double dtdb_days(double jd) { return eraDtdb(JPL_DJM0, jd-JPL_DJM0, 0,0,0,0)/JPL_DAYSEC; }
double jpl_tt_to_tdb(double jd_tt) { return jd_tt + dtdb_days(jd_tt); }
double jpl_tdb_to_tt(double jd_tdb) { return jd_tdb - dtdb_days(jd_tdb); }
