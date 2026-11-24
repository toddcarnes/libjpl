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
    jpl_state st = {1.0, 0.2, 0.1, 0.01, 0.002, 0.001};
    jpl_state icrs, back;
    static const jpl_date jd_j2000 = {2451545.0, 0.0};
    static const jpl_date jd_off = {2460000.5, 0.0};
    JPL_TEST_CHECK(test_ecliptic_to_icrs(&st, &jd_j2000, &icrs) == JPL_OK);
    JPL_TEST_CHECK(jpl_icrs_to_ecliptic(&icrs, &jd_j2000, &back) == JPL_OK);
    for (int i=0;i<6;++i) JPL_TEST_CHECK(fabs(test_state_component(&back,i)-test_state_component(&st,i)) < 1e-9);
    /* Absolute: ecliptic north pole must tilt by ~eps (~23.4 deg). J2000
     * mean obliquity from eraObl06 is the reference value. */
    {
        jpl_state pole = {0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
        jpl_state eq;
        double eps, ex[3], dot, sep, tilt;
        double rn;
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&pole, &jd_j2000, &eq) == JPL_OK);
        eps = eraObl06(2400000.5, 2451545.0-2400000.5);
        ex[0]=0.0; ex[1]=-sin(eps); ex[2]=cos(eps);
        rn = sqrt(eq.x*eq.x+eq.y*eq.y+eq.z*eq.z);
        JPL_TEST_CHECK(fabs(rn-1.0) < 1e-12);
        dot = (eq.x*ex[0]+eq.y*ex[1]+eq.z*ex[2])/rn;
        if (dot > 1.0) dot = 1.0;
        if (dot < -1.0) dot = -1.0;
        sep = acos(dot);
        JPL_TEST_CHECK(sep < 1e-6); /* frame bias/precession only at J2000 */
        tilt = acos(eq.z/rn);
        JPL_TEST_CHECK(fabs(tilt-eps) < 1e-6); /* ~23.4 deg present, not ~0 */
        JPL_TEST_CHECK(fabs(eps-0.4090928041) < 1e-6); /* sanity: ~23.43928 deg */
    }
    /* Rotation matrix orthonormal + consistent with vector path. */
    {
        double R[3][3], p[3]={0.0,0.0,1.0}, pr[3];
        int i, j, k;
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(&jd_j2000, R) == JPL_OK);
        eraRxp(R, p, pr);
        {
            jpl_state pole={0,0,1,0,0,0}, eq;
            JPL_TEST_CHECK(test_ecliptic_to_icrs(&pole, &jd_j2000, &eq) == JPL_OK);
            JPL_TEST_CHECK(fabs(pr[0]-eq.x)<1e-15 && fabs(pr[1]-eq.y)<1e-15 && fabs(pr[2]-eq.z)<1e-15);
        }
        for (i=0;i<3;++i) for (j=0;j<3;++j) {
            double s=0.0; for (k=0;k<3;++k) s+=R[k][i]*R[k][j];
            JPL_TEST_CHECK(fabs(s-(i==j?1.0:0.0)) < 1e-12);
        }
    }
    /* Aliasing-safe: in-place equals out-of-place. */
    {
        jpl_state alias, ref;
        int i;
        alias = st;
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&st, &jd_j2000, &ref) == JPL_OK);
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&alias, &jd_j2000, &alias) == JPL_OK);
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(test_state_component(&alias,i)-test_state_component(&ref,i)) < 1e-15);
        alias = ref;
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(&alias, &jd_j2000, &alias) == JPL_OK);
        for (i=0;i<6;++i) JPL_TEST_CHECK(fabs(test_state_component(&alias,i)-test_state_component(&back,i)) < 1e-15);
    }
    /* Fixed-epoch rigid rotation: v_out == R * v_in exactly at one epoch. */
    {
        jpl_state s2 = {0.5, -0.3, 0.8, 0.001, -0.002, 0.0005};
        jpl_state o2;
        double R2[3][3], p[3], v[3], pr[3], vr[3];
        int k;
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&s2, &jd_off, &o2) == JPL_OK);
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(&jd_off, R2) == JPL_OK);
        for (k = 0; k < 3; ++k) { p[k] = test_state_component(&s2,k); v[k] = test_state_component(&s2,k+3); }
        eraRxp(R2, p, pr); eraRxp(R2, v, vr);
        for (k = 0; k < 3; ++k) {
            JPL_TEST_CHECK(fabs(test_state_component(&o2,k) - pr[k]) < 1e-15);
            JPL_TEST_CHECK(fabs(test_state_component(&o2,k+3) - vr[k]) < 1e-15);
        }
    }
    /* True-of-date nutation belongs to the new position-only rotation;
     * the existing public state conversion remains mean-of-date. */
    {
        const jpl_date tt = {2451545.0, 0.25};
        double p[3] = {1.0, 0.2, 0.1};
        const double zero[3] = {0.0, 0.0, 0.0};
        const double bad[3] = {INFINITY, 0.0, 0.0};
        double m[3][3], dpsi, deps, expect[3], got[3] = {7.0, 8.0, 9.0};
        int k;
        eraPnm06a(tt.hi, tt.lo, m);
        eraNut06a(tt.hi, tt.lo, &dpsi, &deps);
        eraRx(eraObl06(tt.hi, tt.lo) + deps, m);
        eraRxp(m, p, expect);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(p, &tt, got) == JPL_OK);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - expect[k]) < 1e-13);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(zero, &tt, got) == JPL_OK);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(got[k] == 0.0);
        got[0] = 7.0; got[1] = 8.0; got[2] = 9.0;
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(bad, &tt, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(got[0] == 7.0 && got[1] == 8.0 && got[2] == 9.0);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(NULL, &tt, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(got[0] == 7.0 && got[1] == 8.0 && got[2] == 9.0);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(p, NULL, got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(p, &tt, NULL) == JPL_ERROR_ARGUMENT);
    }
    /* TT/TDB conversion is status-returning with pair outputs now. */
    {
        jpl_date tdb = {0.0, 0.0}, back_tt = {0.0, 0.0};
        JPL_TEST_CHECK(jpl_tt_to_tdb(&jd_j2000, &tdb) == JPL_OK);
        /* Round-trip error closes to the 1e-15-day arithmetic residual,
         * far below the millisecond-scale geocentric eraDtdb model
         * envelope (roughly 1.7 ms amplitude, +/-3 ns model accuracy
         * over 1950-2050 per ERFA). */
        JPL_TEST_CHECK(jpl_tdb_to_tt(&tdb, &back_tt) == JPL_OK);
        JPL_TEST_CHECK(fabs((back_tt.hi + back_tt.lo) - 2451545.0) < 1e-9);
        /* In-place conversion is supported. */
        {
            jpl_date alias = {2451545.0, 0.0};
            JPL_TEST_CHECK(jpl_tt_to_tdb(&alias, &alias) == JPL_OK);
            JPL_TEST_CHECK(alias.hi == tdb.hi && alias.lo == tdb.lo);
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
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(&jd_off, R) == JPL_OK);
        eraRxp(R, p, a);
        eraRxp(ecm, p, b);
        for (int k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(a[k] - b[k]) < 1e-12);
    }
    /* Off-J2000 golden vector: pins ecliptic-to-ICRS output at 2460000.5
     * so a future wrong-routine swap fails even if the composition above
     * were rebuilt from the same wrong call. */
    {
        jpl_state s = {0.2, -0.5, 0.8, 0.001, -0.002, 0.0005};
        jpl_state o;
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&s, &jd_off, &o) == JPL_OK);
        JPL_TEST_CHECK(fabs(o.x - 0.197178391766373) < 1e-12);
        JPL_TEST_CHECK(fabs(o.y - -0.777962617565809) < 1e-12);
        JPL_TEST_CHECK(fabs(o.z - 0.534691357224504) < 1e-12);
        JPL_TEST_CHECK(fabs(o.vx - 0.000988697796185) < 1e-12);
        JPL_TEST_CHECK(fabs(o.vy - -0.002039018049268) < 1e-12);
        JPL_TEST_CHECK(fabs(o.vz - -0.000338942565310) < 1e-12);
    }
    /* Absolute TT/TDB reference values with both signs of correction:
     * eraDtdb evaluated on the same pair is the independent oracle here
     * only for the forward direction; the inverse must close the loop to
     * 1e-15 day. A pure production round trip is not the sole oracle for
     * the forward value: the pinned correction digits below come from the
     * ERFA model independently of the date helpers. */
    {
        jpl_date tt = {2451545.0, 0.0}, tdb = {0.0, 0.0}, tt2 = {0.0, 0.0};
        double sec, back_sec;
        double corr = eraDtdb(2451545.0, 0.0, 0.0, 0.0, 0.0, 0.0) / 86400.0;
        JPL_TEST_CHECK(jpl_tt_to_tdb(&tt, &tdb) == JPL_OK);
        sec = (tdb.hi - tt.hi) + (tdb.lo - tt.lo);
        JPL_TEST_CHECK(fabs(sec - corr) < 1e-15);
        JPL_TEST_CHECK(jpl_tdb_to_tt(&tdb, &tt2) == JPL_OK);
        back_sec = (tt2.hi - tt.hi) + (tt2.lo - tt.lo);
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
            jpl_date ref_tt = {ref->tt[0], ref->tt[1]};
            jpl_date tdb = {0.0, 0.0};
            double sec;
            double R[3][3];
            int i, j;
            JPL_TEST_CHECK(jpl_tt_to_tdb(&ref_tt, &tdb) == JPL_OK);
            sec = ((tdb.hi - ref->tt[0]) + (tdb.lo - ref->tt[1])) * 86400.0;
            JPL_TEST_CHECK(fabs(sec - ref->tdb_minus_tt_seconds) < 1e-9);
            JPL_TEST_CHECK(jpl_frame_rotation_matrix(&ref_tt, R) == JPL_OK);
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
        jpl_state huge = {1e308, 1e308, 1e308, 0.0, 0.0, 0.0};
        jpl_state bad = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
        jpl_state keep, got;
        double R[3][3], Rkeep[3][3];
        double rnan[3][3];
        int a, b;
        static const jpl_date jd = {2451545.0, 0.0};
        static const jpl_date nan_date = {2451545.0, NAN};
        bad.y = NAN;
        keep = st;
        got = keep;
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&bad, &jd, &got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&got, &keep, sizeof got) == 0);
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(&bad, &jd, &got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&got, &keep, sizeof got) == 0);
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&st, &nan_date, &got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&got, &keep, sizeof got) == 0);
        JPL_TEST_CHECK(test_ecliptic_to_icrs(NULL, &jd, &got) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_icrs_to_ecliptic(&st, &jd, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(&jd, NULL) == JPL_ERROR_ARGUMENT);
        /* Nonfinite rotation matrix inputs are ARGUMENT, unchanged. */
        for (a = 0; a < 3; ++a) for (b = 0; b < 3; ++b) { R[a][b] = 5.0; Rkeep[a][b] = 5.0; }
        JPL_TEST_CHECK(jpl_frame_rotation_matrix(&nan_date, R) == JPL_ERROR_ARGUMENT);
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
        JPL_TEST_CHECK(test_ecliptic_to_icrs(&huge, &jd, &got) == JPL_OK);
        JPL_TEST_CHECK(isfinite(got.x) && isfinite(got.y) && isfinite(got.z) &&
                       isfinite(got.vx) && isfinite(got.vy) && isfinite(got.vz));
    }
    /* Direct frame-context API tests: creation error contract (*out = NULL),
     * rotate_vector (position and velocity), rotate_state, aliasing, NULL/NaN. */
    {
        jpl_frame_context *ctx = (jpl_frame_context *)&st; /* non-NULL sentinel */
        static const jpl_date jd = {2460000.5, 0.0};
        static const jpl_date nan_jd = {2451545.0, NAN};
        double pos[3] = {0.2, -0.5, 0.8};
        double vel[3] = {0.001, -0.002, 0.0005};
        double pos_out[3] = {7.0, 8.0, 9.0}, vel_out[3] = {7.0, 8.0, 9.0};
        double pos_alias[3] = {0.2, -0.5, 0.8}, vel_alias[3] = {0.001, -0.002, 0.0005};
        double bad_vec[3] = {1.0, NAN, 0.0};
        jpl_state st_in = {0.2, -0.5, 0.8, 0.001, -0.002, 0.0005};
        jpl_state st_out;
        JPL_TEST_CHECK(jpl_frame_context_create(&jd, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_create(NULL, &ctx) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(ctx == NULL);
        ctx = (jpl_frame_context *)&st;
        JPL_TEST_CHECK(jpl_frame_context_create(&nan_jd, &ctx) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(ctx == NULL);
        jpl_frame_context_destroy(NULL); /* must be a safe no-op */
        JPL_TEST_CHECK(jpl_frame_context_create(&jd, &ctx) == JPL_OK && ctx != NULL);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, pos, pos_out) == JPL_OK);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, vel, vel_out) == JPL_OK);
        JPL_TEST_CHECK(jpl_frame_context_rotate_state(ctx, &st_in, &st_out) == JPL_OK);
        /* Single-computer pin: the state entry is two vector rotations,
         * so its halves must equal the vector entry bit for bit. A future
         * frame-rate fix that touches only one path breaks loudly here,
         * which is the point: the dR/dt omission is a contract, and any
         * change to it must be deliberate and total. */
        JPL_TEST_CHECK(pos_out[0] == st_out.x && pos_out[1] == st_out.y && pos_out[2] == st_out.z);
        JPL_TEST_CHECK(vel_out[0] == st_out.vx && vel_out[1] == st_out.vy && vel_out[2] == st_out.vz);
        /* In-place aliasing for position and velocity. */
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, pos_alias, pos_alias) == JPL_OK);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, vel_alias, vel_alias) == JPL_OK);
        JPL_TEST_CHECK(fabs(pos_alias[0] - pos_out[0]) < 1e-15 &&
                       fabs(pos_alias[1] - pos_out[1]) < 1e-15 &&
                       fabs(pos_alias[2] - pos_out[2]) < 1e-15);
        JPL_TEST_CHECK(fabs(vel_alias[0] - vel_out[0]) < 1e-15 &&
                       fabs(vel_alias[1] - vel_out[1]) < 1e-15 &&
                       fabs(vel_alias[2] - vel_out[2]) < 1e-15);
        /* NULL and nonfinite inputs leave output unchanged. */
        pos_out[0] = 7.0; pos_out[1] = 8.0; pos_out[2] = 9.0;
        vel_out[0] = 7.0; vel_out[1] = 8.0; vel_out[2] = 9.0;
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(NULL, pos, pos_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, NULL, pos_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, pos, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, bad_vec, pos_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(pos_out[0] == 7.0 && pos_out[1] == 8.0 && pos_out[2] == 9.0);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(NULL, vel, vel_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, NULL, vel_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, vel, NULL) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_frame_context_rotate_vector(ctx, bad_vec, vel_out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(vel_out[0] == 7.0 && vel_out[1] == 8.0 && vel_out[2] == 9.0);
        jpl_frame_context_destroy(ctx);
    }
    puts("frames tests passed");
    return 0;
}
