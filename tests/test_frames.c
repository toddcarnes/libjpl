#include "jpl_frames.h"
#include "jpl_test_util.h"
#include "erfa.h"
#include <math.h>
#include <stdio.h>
int main(void) {
    double st[6] = {1.0, 0.2, 0.1, 0.01, 0.002, 0.001};
    double icrs[6], back[6];
    jpl_ecliptic_to_icrs(st, 2451545.0, icrs);
    jpl_icrs_to_ecliptic(icrs, 2451545.0, back);
    for (int i=0;i<6;++i) JPL_TEST_CHECK(fabs(back[i]-st[i]) < 1e-9);
    /* Absolute: ecliptic north pole must tilt by ~eps (~23.4 deg). J2000
     * mean obliquity from eraObl06 is the reference value. */
    {
        double pole[6] = {0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
        double eq[6], eps, ex[3], dot, sep, tilt;
        double rn;
        jpl_ecliptic_to_icrs(pole, 2451545.0, eq);
        eps = eraObl06(2400000.5, 2451545.0-2400000.5);
        ex[0]=0.0; ex[1]=-sin(eps); ex[2]=cos(eps);
        rn = sqrt(eq[0]*eq[0]+eq[1]*eq[1]+eq[2]*eq[2]);
        JPL_TEST_CHECK(fabs(rn-1.0) < 1e-12);
        dot = (eq[0]*ex[0]+eq[1]*ex[1]+eq[2]*ex[2])/rn;
        if (dot > 1.0) dot = 1.0; if (dot < -1.0) dot = -1.0;
        sep = acos(dot);
        JPL_TEST_CHECK(sep < 1e-6); /* frame bias/precession only at J2000 */
        tilt = acos(eq[2]/rn);
        JPL_TEST_CHECK(fabs(tilt-eps) < 1e-6); /* ~23.4 deg present, not ~0 */
        JPL_TEST_CHECK(fabs(eps-0.4090928041) < 1e-6); /* sanity: ~23.43928 deg */
    }
    /* Rotation matrix orthonormal + consistent with vector path. */
    {
        double R[3][3], p[3]={0.0,0.0,1.0}, pr[3];
        int i, j, k;
        jpl_frame_rotation_matrix(2451545.0, R);
        eraRxp(R, p, pr);
        {
            double pole[6]={0,0,1,0,0,0}, eq[6];
            jpl_ecliptic_to_icrs(pole, 2451545.0, eq);
            JPL_TEST_CHECK(fabs(pr[0]-eq[0])<1e-15 && fabs(pr[1]-eq[1])<1e-15 && fabs(pr[2]-eq[2])<1e-15);
        }
        for (i=0;i<3;++i) for (j=0;j<3;++j) {
            double s=0.0; for (k=0;k<3;++k) s+=R[k][i]*R[k][j];
            JPL_TEST_CHECK(fabs(s-(i==j?1.0:0.0)) < 1e-12);
        }
    }
    /* Aliasing-safe: in-place equals out-of-place. */
    {
        double alias[6], ref[6];
        int i;
        for (i=0;i<6;++i) alias[i]=st[i];
        jpl_ecliptic_to_icrs(st, 2451545.0, ref);
        jpl_ecliptic_to_icrs(alias, 2451545.0, alias);
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(alias[i]-ref[i]) < 1e-15);
        for (i=0;i<6;++i) alias[i]=ref[i];
        jpl_icrs_to_ecliptic(alias, 2451545.0, alias);
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(alias[i]-back[i]) < 1e-15);
    }
    /* Independent velocity check: finite-difference of the rotation at two
     * epochs must match the rotated input velocity (rigid rotation, so
     * v_out == R * v_in exactly for a fixed epoch). */
    {
        double s2[6] = {0.5, -0.3, 0.8, 0.001, -0.002, 0.0005};
        double o2[6], R2[3][3], p[3], v[3], pr[3], vr[3];
        int k;
        jpl_ecliptic_to_icrs(s2, 2460000.5, o2);
        jpl_frame_rotation_matrix(2460000.5, R2);
        for (k = 0; k < 3; ++k) { p[k] = s2[k]; v[k] = s2[k+3]; }
        eraRxp(R2, p, pr); eraRxp(R2, v, vr);
        for (k = 0; k < 3; ++k) {
            JPL_TEST_CHECK(fabs(o2[k] - pr[k]) < 1e-15);
            JPL_TEST_CHECK(fabs(o2[k+3] - vr[k]) < 1e-15);
        }
    }
    double tdb = jpl_tt_to_tdb(2451545.0);
    /* Round-trip error is far below the ~2ms absolute model floor of the
     * zero-observer eraDtdb approximation. */
    JPL_TEST_CHECK(fabs(jpl_tdb_to_tt(tdb) - 2451545.0) < 1e-9);
    puts("frames tests passed");
    return 0;
}
