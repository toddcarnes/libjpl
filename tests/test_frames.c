#include "jpl_frames.h"
#include "erfa.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    double st[6] = {1.0, 0.2, 0.1, 0.01, 0.002, 0.001};
    double icrs[6], back[6];
    jpl_ecliptic_to_icrs(st, 2451545.0, icrs);
    jpl_icrs_to_ecliptic(icrs, 2451545.0, back);
    for (int i=0;i<6;++i) assert(fabs(back[i]-st[i]) < 1e-9);
    /* Absolute: ecliptic north pole must tilt by ~eps (~23.4 deg). */
    {
        double pole[6] = {0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
        double eq[6], eps, ex[3], dot, sep, tilt;
        double rn;
        jpl_ecliptic_to_icrs(pole, 2451545.0, eq);
        eps = eraObl06(2400000.5, 2451545.0-2400000.5);
        ex[0]=0.0; ex[1]=-sin(eps); ex[2]=cos(eps);
        rn = sqrt(eq[0]*eq[0]+eq[1]*eq[1]+eq[2]*eq[2]);
        assert(fabs(rn-1.0) < 1e-12);
        dot = (eq[0]*ex[0]+eq[1]*ex[1]+eq[2]*ex[2])/rn;
        if (dot > 1.0) dot = 1.0; if (dot < -1.0) dot = -1.0;
        sep = acos(dot);
        assert(sep < 1e-6); /* frame bias/precession only at J2000 */
        tilt = acos(eq[2]/rn);
        assert(fabs(tilt-eps) < 1e-6); /* ~23.4 deg present, not ~0 */
        assert(fabs(eps-0.4090928041) < 1e-6); /* sanity: ~23.43928 deg */
    }
    /* Rotation matrix orthonormal + consistent with vector path. */
    {
        double R[3][3], RtR[3][3], p[3]={0.0,0.0,1.0}, pr[3];
        int i, j, k;
        jpl_frame_rotation_matrix(2451545.0, R);
        eraRxp(R, p, pr);
        {
            double pole[6]={0,0,1,0,0,0}, eq[6];
            jpl_ecliptic_to_icrs(pole, 2451545.0, eq);
            assert(fabs(pr[0]-eq[0])<1e-15 && fabs(pr[1]-eq[1])<1e-15 && fabs(pr[2]-eq[2])<1e-15);
        }
        for (i=0;i<3;++i) for (j=0;j<3;++j) {
            double s=0.0; for (k=0;k<3;++k) s+=R[k][i]*R[k][j];
            assert(fabs(s-(i==j?1.0:0.0)) < 1e-12);
        }
        (void)RtR;
    }
    /* Aliasing-safe: in-place equals out-of-place. */
    {
        double alias[6], ref[6];
        int i;
        for (i=0;i<6;++i) alias[i]=st[i];
        jpl_ecliptic_to_icrs(st, 2451545.0, ref);
        jpl_ecliptic_to_icrs(alias, 2451545.0, alias);
        for (i=0;i<6;++i) assert(fabs(alias[i]-ref[i]) < 1e-15);
        for (i=0;i<6;++i) alias[i]=ref[i];
        jpl_icrs_to_ecliptic(alias, 2451545.0, alias);
        for (i=0;i<6;++i) assert(fabs(alias[i]-back[i]) < 1e-15);
    }
    double tdb = jpl_tt_to_tdb(2451545.0);
    assert(fabs(jpl_tdb_to_tt(tdb) - 2451545.0) < 1e-6);
    puts("frames tests passed");
    return 0;
}
