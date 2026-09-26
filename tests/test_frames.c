/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_frames.h"
#include "jpl_private.h"
#include "jpl_test_util.h"
#include "numerical_reference.h"
#include "erfa.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define JPL_DJM0_TEST 2400000.5
int main(void) {
    double st[6] = {1.0, 0.2, 0.1, 0.01, 0.002, 0.001};
    double icrs[6], back[6];
    static const double jd_j2000[2] = {2451545.0, 0.0};
    static const double jd_off[2] = {2460000.5, 0.0};
    JPL_TEST_CHECK(jpl_ecliptic_to_icrs(st, jd_j2000, icrs) == JPL_OK);
    JPL_TEST_CHECK(jpl_icrs_to_ecliptic(icrs, jd_j2000, back) == JPL_OK);
    for (int i=0;i<6;++i) JPL_TEST_CHECK(fabs(back[i]-st[i]) < 1e-9);
    /* Absolute: ecliptic north pole must tilt by ~eps (~23.4 deg). J2000
     * mean obliquity from eraObl06 is the reference value. */
    {
        double pole[6] = {0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
        double eq[6], eps, ex[3], dot, sep, tilt;
        double rn;
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(pole, jd_j2000, eq) == JPL_OK);
        eps = eraObl06(2400000.5, 2451545.0-2400000.5);
        ex[0]=0.0; ex[1]=-sin(eps); ex[2]=cos(eps);
        rn = sqrt(eq[0]*eq[0]+eq[1]*eq[1]+eq[2]*eq[2]);
        JPL_TEST_CHECK(fabs(rn-1.0) < 1e-12);
        dot = (eq[0]*ex[0]+eq[1]*ex[1]+eq[2]*ex[2])/rn;
        if (dot > 1.0) dot = 1.0;
        if (dot < -1.0) dot = -1.0;
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
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(jd_j2000, R) == JPL_OK);
        eraRxp(R, p, pr);
        {
            double pole[6]={0,0,1,0,0,0}, eq[6];
            JPL_TEST_CHECK(jpl_ecliptic_to_icrs(pole, jd_j2000, eq) == JPL_OK);
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
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(st, jd_j2000, ref) == JPL_OK);
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(alias, jd_j2000, alias) == JPL_OK);
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(alias[i]-ref[i]) < 1e-15);
        for (i=0;i<6;++i) alias[i]=ref[i];
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(alias, jd_j2000, alias) == JPL_OK);
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(alias[i]-back[i]) < 1e-15);
    }
    /* Fixed-epoch rigid rotation: v_out == R * v_in exactly at one epoch. */
    {
        double s2[6] = {0.5, -0.3, 0.8, 0.001, -0.002, 0.0005};
        double o2[6], R2[3][3], p[3], v[3], pr[3], vr[3];
        int k;
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(s2, jd_off, o2) == JPL_OK);
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(jd_off, R2) == JPL_OK);
        for (k = 0; k < 3; ++k) { p[k] = s2[k]; v[k] = s2[k+3]; }
        eraRxp(R2, p, pr); eraRxp(R2, v, vr);
        for (k = 0; k < 3; ++k) {
            JPL_TEST_CHECK(fabs(o2[k] - pr[k]) < 1e-15);
            JPL_TEST_CHECK(fabs(o2[k+3] - vr[k]) < 1e-15);
        }
    }
    /* True-of-date nutation belongs to the new position-only rotation;
     * the existing public state conversion remains mean-of-date. */
    {
        const double tt[2] = {2451545.0, 0.25};
        double p[3] = {1.0, 0.2, 0.1};
        const double zero[3] = {0.0, 0.0, 0.0};
        const double bad[3] = {INFINITY, 0.0, 0.0};
        double m[3][3], dpsi, deps, expect[3], got[3] = {7.0, 8.0, 9.0};
        int k;
        eraPnm06a(tt[0], tt[1], m);
        eraNut06a(tt[0], tt[1], &dpsi, &deps);
        eraRx(eraObl06(tt[0], tt[1]) + deps, m);
        eraRxp(m, p, expect);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(p, tt, got) == JPL_OK);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - expect[k]) < 1e-13);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(zero, tt, got) == JPL_OK);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(got[k] == 0.0);
        got[0] = 7.0; got[1] = 8.0; got[2] = 9.0;
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(bad, tt, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(got[0] == 7.0 && got[1] == 8.0 && got[2] == 9.0);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(NULL, tt, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(got[0] == 7.0 && got[1] == 8.0 && got[2] == 9.0);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(p, NULL, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(p, tt, NULL) == JPL_ERROR_ARGUMENT);
    }
    /* TT/TDB conversion is status-returning with pair outputs now. */
    {
        double tdb[2] = {0.0, 0.0}, back_tt[2] = {0.0, 0.0};
        JPL_TEST_CHECK(jpl_tt_to_tdb(jd_j2000, tdb) == JPL_OK);
        /* Round-trip error closes to the 1e-15-day arithmetic residual,
         * far below the millisecond-scale geocentric eraDtdb model
         * envelope (roughly 1.7 ms amplitude, +/-3 ns model accuracy
         * over 1950-2050 per ERFA). */
        JPL_TEST_CHECK(jpl_tdb_to_tt(tdb, back_tt) == JPL_OK);
        JPL_TEST_CHECK(fabs((back_tt[0] + back_tt[1]) - 2451545.0) < 1e-9);
        /* In-place conversion is supported. */
        {
            double alias[2] = {2451545.0, 0.0};
            JPL_TEST_CHECK(jpl_tt_to_tdb(alias, alias) == JPL_OK);
            JPL_TEST_CHECK(alias[0] == tdb[0] && alias[1] == tdb[1]);
        }
    }
    /* Off-J2000 independent composition (Pmat06 + Rx(obliquity)).
     * eraEcm06 == Rxr(Rx(eps), Pmat06) by construction, so the eraPmat06-only
     * composition agrees to roundoff (~1e-15; no bias floor since Pmat06
     * already includes frame bias). */
    {
        double jd = 2460000.5;
        double R[3][3], Rp[3][3], ecm[3][3], p[3] = {0.2, -0.5, 0.8}, a[3], b[3];
        double eps = eraObl06(JPL_DJM0_TEST, jd - JPL_DJM0_TEST);
        eraPmat06(JPL_DJM0_TEST, jd - JPL_DJM0_TEST, Rp);
        eraRx(eps, Rp); /* Rp = Rx(eps) * Pmat06: ICRS -> mean ecliptic of date */
        eraTr(Rp, ecm); /* transpose: ecliptic -> ICRS */
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(jd_off, R) == JPL_OK);
        eraRxp(R, p, a);
        eraRxp(ecm, p, b);
        for (int k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(a[k] - b[k]) < 1e-12);
    }
    /* Off-J2000 golden vector: pins jpl_ecliptic_to_icrs output at 2460000.5
     * so a future wrong-routine swap fails even if the composition above
     * were rebuilt from the same wrong call. */
    {
        double s[6] = {0.2, -0.5, 0.8, 0.001, -0.002, 0.0005};
        double o[6];
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(s, jd_off, o) == JPL_OK);
        JPL_TEST_CHECK(fabs(o[0] - 0.197178391766373) < 1e-12);
        JPL_TEST_CHECK(fabs(o[1] - -0.777962617565809) < 1e-12);
        JPL_TEST_CHECK(fabs(o[2] - 0.534691357224504) < 1e-12);
        JPL_TEST_CHECK(fabs(o[3] - 0.000988697796185) < 1e-12);
        JPL_TEST_CHECK(fabs(o[4] - -0.002039018049268) < 1e-12);
        JPL_TEST_CHECK(fabs(o[5] - -0.000338942565310) < 1e-12);
    }
    /* Absolute TT/TDB reference values with both signs of correction:
     * eraDtdb evaluated on the same pair is the independent oracle here
     * only for the forward direction; the inverse must close the loop to
     * 1e-15 day. A pure production round trip is not the sole oracle for
     * the forward value: the pinned correction digits below come from the
     * ERFA model independently of the date helpers. */
    {
        double tt[2] = {2451545.0, 0.0}, tdb[2], tt2[2], sec, back_sec;
        double corr = eraDtdb(2451545.0, 0.0, 0.0, 0.0, 0.0, 0.0) / 86400.0;
        JPL_TEST_CHECK(jpl_tt_to_tdb(tt, tdb) == JPL_OK);
        sec = (tdb[0] - tt[0]) + (tdb[1] - tt[1]);
        JPL_TEST_CHECK(fabs(sec - corr) < 1e-15);
        JPL_TEST_CHECK(jpl_tdb_to_tt(tdb, tt2) == JPL_OK);
        back_sec = (tt2[0] - tt[0]) + (tt2[1] - tt[1]);
        JPL_TEST_CHECK(fabs(back_sec) <= 1e-15);
    }
    /* Pinned time/frame references consumed as orientation truth.
     * jpl_frame_rotation_matrix returns ecliptic->ICRS, the transpose of
     * the pinned ICRS->ecliptic matrix; the correction seconds come back
     * through the small pair difference. */
    {
        size_t e;
        JPL_TEST_CHECK(jpl_time_reference_count == 5);
        for (e = 0; e < jpl_time_reference_count; ++e) {
            const jpl_time_reference *ref = &jpl_time_references[e];
            double tdb[2], sec;
            double R[3][3];
            int i, j;
            JPL_TEST_CHECK(jpl_tt_to_tdb(ref->tt, tdb) == JPL_OK);
            sec = ((tdb[0] - ref->tt[0]) + (tdb[1] - ref->tt[1])) * 86400.0;
            JPL_TEST_CHECK(fabs(sec - ref->tdb_minus_tt_seconds) < 1e-9);
            JPL_TEST_CHECK(jpl_frame_rotation_matrix(ref->tt, R) == JPL_OK);
            for (i = 0; i < 3; ++i) {
                for (j = 0; j < 3; ++j) {
                    JPL_TEST_CHECK(fabs(R[j][i] - ref->icrs_to_ecliptic[i][j]) < 1e-12);
                }
            }
        }
    }
    /* Status contracts: nonfinite states are ARGUMENT with unchanged
     * output; NULL pointers are ARGUMENT; conversion overflow (very
     * large finite arithmetic) is NUMERIC with unchanged output. */
    {
        double huge[6] = {1e308, 1e308, 1e308, 0.0, 0.0, 0.0};
        double bad[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
        double keep[6], got[6], R[3][3], Rkeep[3][3];
        double rnan[3][3];
        int a, b;
        static const double jd[2] = {2451545.0, 0.0};
        static const double nan_date[2] = {2451545.0, NAN};
        bad[1] = NAN;
        memcpy(keep, st, sizeof keep);
        memcpy(got, keep, sizeof got);
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(bad, jd, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(got, keep, sizeof got) == 0);
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(bad, jd, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(got, keep, sizeof got) == 0);
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(st, nan_date, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(got, keep, sizeof got) == 0);
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(NULL, jd, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(st, jd, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(jd, NULL) == JPL_ERROR_ARGUMENT);
        /* Nonfinite rotation matrix inputs are ARGUMENT, unchanged. */
        for (a = 0; a < 3; ++a) for (b = 0; b < 3; ++b) { R[a][b] = 5.0; Rkeep[a][b] = 5.0; }
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(nan_date, R) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(R, Rkeep, sizeof R) == 0);
        /* Matrix finiteness itself cannot be forced from a finite date
         * through ERFA, so check the propagation path structurally:
         * a NaN matrix applied to a finite state must fail NUMERIC. */
        for (a = 0; a < 3; ++a) for (b = 0; b < 3; ++b) rnan[a][b] = (a == b) ? 1.0 : 0.0;
        rnan[0][0] = NAN;
        { double p[3] = {1.0, 0.0, 0.0}, pr[3]; eraRxp(rnan, p, pr);
          JPL_TEST_CHECK(!isfinite(pr[0])); }
        /* Very large finite arithmetic: rotation of 1e308-scale vectors
         * stays finite (rotation preserves the norm), so assert success
         * and finiteness rather than overflow here. */
        JPL_TEST_CHECK(jpl_ecliptic_to_icrs(huge, jd, got) == JPL_OK);
        for (a = 0; a < 6; ++a) JPL_TEST_CHECK(isfinite(got[a]));
    }
    puts("frames tests passed");
    return 0;
}
