/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_test_util.h"
#include "numerical_reference.h"
#include "erfa.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    jpl_state st = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    double vobs[3] = {0.0, 0.01, 0.0};
    jpl_state out = {0};
    JPL_TEST_CHECK(jpl_apply_aberration(&st, vobs, 0.0, &out) == JPL_OK);
    JPL_TEST_CHECK(fabs(out.x - st.x) > 0.0);
    double st_pos[3] = {st.x, st.y, st.z};
    double out_pos[3] = {out.x, out.y, out.z};
    double r0 = test_norm3(st_pos);
    double r1 = test_norm3(out_pos);
    JPL_TEST_CHECK(fabs(r0 - r1) < 1e-9);
    /* Velocity passed through unchanged. */
    JPL_TEST_CHECK(out.vx==st.vx && out.vy==st.vy && out.vz==st.vz);
    jpl_state io = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    JPL_TEST_CHECK(jpl_apply_aberration(&io, vobs, 0.0, &io) == JPL_OK);
    JPL_TEST_CHECK(isfinite(io.x) && isfinite(io.vx));
    JPL_TEST_CHECK(io.vx==0.0 && io.vy==0.01 && io.vz==0.0);
    /* NULL inputs report ARGUMENT and leave outputs unchanged. */
    {
        jpl_state keep = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
        memcpy(&out, &keep, sizeof out);
        JPL_TEST_CHECK(jpl_apply_aberration(NULL, vobs, 0.0, &out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&out, &keep, sizeof out) == 0);
        JPL_TEST_CHECK(jpl_apply_aberration(&st, NULL, 0.0, &out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&out, &keep, sizeof out) == 0);
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vobs, 0.0, NULL) == JPL_ERROR_ARGUMENT);
    }
    /* Zero position passes through with unchanged velocity. */
    {
        jpl_state z={0,0,0,0.001,0.002,0.003}, zo;
        JPL_TEST_CHECK(jpl_apply_aberration(&z, vobs, 0.0, &zo) == JPL_OK);
        for (int i=0;i<6;++i) JPL_TEST_CHECK(test_state_component(&zo, i)==test_state_component(&z, i));
    }
    /* Zero observer velocity leaves position unchanged. */
    {
        double zobs[3]={0,0,0};
        jpl_state so;
        JPL_TEST_CHECK(jpl_apply_aberration(&st, zobs, 0.0, &so) == JPL_OK);
        JPL_TEST_CHECK(fabs(so.x-st.x)<1e-15 && fabs(so.y-st.y)<1e-15 && fabs(so.z-st.z)<1e-15);
    }
    /* ERFA t_erfa_c vector (t_ab): solar-gravity term differs <=0.4uas, so 1e-9.
     * beta comes from AU/day observer velocity over independently derived
     * light speed (SI c), never a rounded literal. */
    {
        static const double C_KM_S = JPL_TEST_C_KM_PER_SECOND;
        static const double AU_KM = JPL_TEST_AU_KM;
        static const double DAYSEC = JPL_TEST_DAYSEC;
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        jpl_state s2 = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*C_KM_S*DAYSEC/AU_KM, vc[1]*C_KM_S*DAYSEC/AU_KM, vc[2]*C_KM_S*DAYSEC/AU_KM};
        jpl_state o2;
        JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, 0.0, &o2) == JPL_OK);
        JPL_TEST_CHECK(fabs(o2.x - (-0.7631631094219556269)) < 1e-9);
        JPL_TEST_CHECK(fabs(o2.y - (-0.6087553082505590832)) < 1e-9);
        JPL_TEST_CHECK(fabs(o2.z - (-0.2167926269368471279)) < 1e-9);
    }
    /* Grid vs eraAb directly (s=1e12 nulls the gravity term; expect 1e-12). */
    {
        static const double C_AU_DAY = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        double dirs[4][3] = {{1,0,0},{0,1,0},{0,0,1},{0.5773502691896258,0.5773502691896258,0.5773502691896258}};
        double vels[3][3] = {{0.01,0.002,-0.001},{-0.005,0.008,0.003},{0.0,0.0,0.0}};
        int d, q, k;
        for (d = 0; d < 4; ++d) for (q = 0; q < 3; ++q) {
            jpl_state s6, got;
            double vo[3], be[3], exp[3], b2 = 0.0, bm;
            for (k = 0; k < 3; ++k) { (&s6.x)[k] = dirs[d][k]*2.5; (&s6.x)[k+3] = 0.001*k; vo[k] = vels[q][k]; be[k] = vo[k]/C_AU_DAY; b2 += be[k]*be[k]; }
            bm = sqrt(1.0 - b2);
            JPL_TEST_CHECK(jpl_apply_aberration(&s6, vo, 0.0, &got) == JPL_OK);
            eraAb(dirs[d], be, 1e12, bm, exp);
            for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(test_state_component(&got, k) - exp[k]*2.5) < 1e-12);
            for (k = 0; k < 3; ++k) JPL_TEST_CHECK(test_state_component(&got, k+3) == test_state_component(&s6, k+3));
        }
    }
    /* Full variant matches eraAb including the solar term: same t_ab
     * inputs at the true Sun distance agree to 1e-12. Observer velocity
     * uses independently derived light speed from SI c. */
    {
        static const double C_KM_S = JPL_TEST_C_KM_PER_SECOND;
        static const double AU_KM = JPL_TEST_AU_KM;
        static const double DAYSEC = JPL_TEST_DAYSEC;
        double c_au_day = C_KM_S * DAYSEC / AU_KM;
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        jpl_state s2 = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {0.0, 0.0, 0.0};
        jpl_state o2;
        vo2[0] = vc[0] * c_au_day; vo2[1] = vc[1] * c_au_day; vo2[2] = vc[2] * c_au_day;
        JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, 0.99980921395708788, &o2) == JPL_OK);
        JPL_TEST_CHECK(fabs(o2.x - (-0.7631631094219556269)) < 1e-12);
        JPL_TEST_CHECK(fabs(o2.y - (-0.6087553082505590832)) < 1e-12);
        JPL_TEST_CHECK(fabs(o2.z - (-0.2167926269368471279)) < 1e-12);
        /* Zero and negative finite Sun distances omit the solar term. */
        { jpl_state ow;
          JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, 0.0, &ow) == JPL_OK);
          JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, -2.5, &o2) == JPL_OK);
          JPL_TEST_CHECK(memcmp(&o2, &ow, sizeof o2) == 0); }
        /* NULL-safe: ARGUMENT with unchanged outputs. */
        { jpl_state keep = {3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
          memcpy(&o2, &keep, sizeof o2);
          JPL_TEST_CHECK(jpl_apply_aberration(NULL, vo2, 1.0, &o2) == JPL_ERROR_ARGUMENT);
          JPL_TEST_CHECK(memcmp(&o2, &keep, sizeof o2) == 0);
          JPL_TEST_CHECK(jpl_apply_aberration(&s2, NULL, 1.0, &o2) == JPL_ERROR_ARGUMENT);
          JPL_TEST_CHECK(memcmp(&o2, &keep, sizeof o2) == 0);
          JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, 1.0, NULL) == JPL_ERROR_ARGUMENT); }
    }
    /* Differential vs eraAb at finite Sun distance (catches SRS drift). */
    {
        static const double C_KM_S = JPL_TEST_C_KM_PER_SECOND;
        static const double AU_KM = JPL_TEST_AU_KM;
        static const double DAYSEC = JPL_TEST_DAYSEC;
        double c_au_day = C_KM_S * DAYSEC / AU_KM;
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        jpl_state s2 = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {0.0, 0.0, 0.0};
        jpl_state got;
        double be[3], exp[3];
        double s_au = 0.99980921395708788;
        int k;
        vo2[0] = vc[0] * c_au_day; vo2[1] = vc[1] * c_au_day; vo2[2] = vc[2] * c_au_day;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, s_au, &got) == JPL_OK);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(test_state_component(&got, k) - exp[k]) < 1e-12);
    }
    /* SRS-amplifying differential: at small Sun distance the solar term
     * exceeds 1e-12, so half-SRS code fails this block (verified by
     * stash experiment); pins JPL_SRS_AU against eraAb. */
    {
        static const double C_KM_S = JPL_TEST_C_KM_PER_SECOND;
        static const double AU_KM = JPL_TEST_AU_KM;
        static const double DAYSEC = JPL_TEST_DAYSEC;
        double c_au_day = C_KM_S * DAYSEC / AU_KM;
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        jpl_state s2 = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {0.0, 0.0, 0.0};
        jpl_state got;
        double be[3], exp[3];
        double s_au = 0.05;
        int k;
        vo2[0] = vc[0] * c_au_day; vo2[1] = vc[1] * c_au_day; vo2[2] = vc[2] * c_au_day;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        JPL_TEST_CHECK(jpl_apply_aberration(&s2, vo2, s_au, &got) == JPL_OK);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(test_state_component(&got, k) - exp[k]) < 1e-12);
    }
    /* Positive-scale invariance: 1e-200, 1, 1e200 share one normalized
     * direction; absolute position error would hide tiny/overflow cases,
     * so compare unit directions and assert representability per scale. */
    {
        static const double scales[3] = {1e-200, 1.0, 1e200};
        double ref[3] = {0.0, 0.0, 0.0};
        int si, k;
        for (si = 0; si < 3; ++si) {
            double sc = scales[si];
            jpl_state s = {0.6 * sc, -0.8 * sc, 0.0, 1.0, 2.0, 3.0};
            double v[3] = {0.0, 0.01, 0.0};
            jpl_state got, aliased;
            double dir[3], n;
            aliased = s;
            JPL_TEST_CHECK(jpl_apply_aberration(&s, v, 0.0, &got) == JPL_OK);
            /* Velocity slots pass through unchanged. */
            JPL_TEST_CHECK(got.vx == 1.0 && got.vy == 2.0 && got.vz == 3.0);
            /* Exact permitted aliasing: in-place equals out-of-place. */
            JPL_TEST_CHECK(jpl_apply_aberration(&aliased, v, 0.0, &aliased) == JPL_OK);
            JPL_TEST_CHECK(memcmp(&aliased, &got, sizeof got) == 0);
            /* Direction is finite and shared across scales. The test norm
             * itself must be scale-safe (nested hypot): an unscaled
             * sqrt(x*x+...) underflows at 1e-200. */
            n = hypot(hypot(got.x, got.y), got.z);
            JPL_TEST_CHECK(isfinite(n) && n > 0.0);
            for (k = 0; k < 3; ++k) {
                dir[k] = test_state_component(&got, k) / n;
                JPL_TEST_CHECK(isfinite(dir[k]));
            }
            if (si == 0) {
                /* Tiny: representable nonzero output, not flushed. */
                JPL_TEST_CHECK(n > 0.0 && n < 1e-190);
                memcpy(ref, dir, sizeof ref);
            } else {
                for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(dir[k] - ref[k]) < 1e-12);
                if (si == 2) JPL_TEST_CHECK(n > 1e190 && isfinite(n));
            }
        }
    }
    /* Exact large-scale contract from the plan. */
    {
        jpl_state s = {1e200,0,0,1,2,3}, out2;
        double v[3] = {0,0.01,0};
        JPL_TEST_CHECK(jpl_apply_aberration(&s, v, 0.0, &out2) == JPL_OK);
        JPL_TEST_CHECK(isfinite(out2.x) && isfinite(out2.y));
        JPL_TEST_CHECK(out2.y > 0.0);
        JPL_TEST_CHECK(out2.vx == 1 && out2.vy == 2 && out2.vz == 3);
    }
    /* Standalone validation matrix: nonfinite slots, superluminal/c
     * observer, and nonfinite Sun distance report ARGUMENT with outputs
     * unchanged; validation precedes the zero-position shortcut. */
    {
        static const double C_AU_DAY = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        jpl_state keep = {1.5, -2.5, 3.5, 4.5, 5.5, 6.5};
        jpl_state bad, oo;
        double vv[3];
        jpl_state z = {0,0,0,0.001,0.002,0.003};
        oo = keep;
        bad = st; bad.z = NAN;
        JPL_TEST_CHECK(jpl_apply_aberration(&bad, vobs, 0.0, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        bad = st; bad.vz = INFINITY;
        JPL_TEST_CHECK(jpl_apply_aberration(&bad, vobs, 0.0, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        vv[0] = vobs[0]; vv[1] = vobs[1]; vv[2] = NAN;
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vv, 0.0, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        /* Observer at exactly c and above c are both invalid. */
        vv[0] = C_AU_DAY; vv[1] = 0.0; vv[2] = 0.0;
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vv, 0.0, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        vv[0] = 2.0 * C_AU_DAY;
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vv, 0.0, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        /* Zero position with a nonfinite velocity slot still fails. */
        vv[0] = 0.0; vv[1] = INFINITY; vv[2] = 0.0;
        JPL_TEST_CHECK(jpl_apply_aberration(&z, vv, 0.0, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        /* Nonfinite Sun distance is ARGUMENT, not a silent omission. */
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vobs, NAN, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vobs, INFINITY, &oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
    }
    /* Pinned apparent-oracle velocity slot. The oracle's
     * velocity is V-W by construction (mixed-epoch difference passed
     * through aberration); the first pinned case uses W directly. */
    {
        static const double W[3] = {0.002, 0.004, -0.001};
        static const double V[3] = {0.005, -0.003, 0.001};
        int k;
        for (k = 0; k < 3; ++k) {
            double want = V[k] - W[k];
            double got = jpl_apparent_references[0].state[k + 3];
            JPL_TEST_CHECK(fabs(got - want) < 1e-12);
        }
    }
    /* Genuinely unrepresentable result: a finite but microscopic Sun
     * distance overflows the SRS quotient, so NUMERIC with no output. */
    {
        jpl_state keep = {1.5, -2.5, 3.5, 4.5, 5.5, 6.5};
        jpl_state oo;
        oo = keep;
        JPL_TEST_CHECK(jpl_apply_aberration(&st, vobs, 1e-320, &oo) == JPL_ERROR_NUMERIC);
        JPL_TEST_CHECK(memcmp(&oo, &keep, sizeof oo) == 0);
    }
    /* Finite tiny-Sun regression: a unit-scale nonradial state with a
     * finite subluminal observer velocity and sun_distance_au = 1e-200
     * must succeed with finite output, preserved distance, unchanged
     * velocity, and exact in-place aliasing. The expected direction is an
      * independent scale-safe normalization of the same published expression
      * (cf. ERFA eraAb), compared within 1e-12. Squaring the large
      * intermediate directly would overflow even though its direction is
      * representable. */
    {
        static const jpl_state tiny_state = {0.6, -0.8, 0.0, 1.0, 2.0, 3.0};
        static const double tiny_vel[3] = {0.0, 0.01, 0.0};
        jpl_state got, aliased;
        double dir[3], n, ref_dir[3], ref_n;
        double beta[3], pnat[3], p[3], bm1, pdv = 0.0, w1, w2;
        double C_AU_DAY = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        double SRS = 1.97412574336e-8;
        double r;
        int k;
        aliased = tiny_state;
        JPL_TEST_CHECK(jpl_apply_aberration(&tiny_state, tiny_vel, 1e-200, &got) == JPL_OK);
        JPL_TEST_CHECK(isfinite(got.x) && isfinite(got.y) && isfinite(got.z) &&
                       isfinite(got.vx) && isfinite(got.vy) && isfinite(got.vz));
        /* Independent reference: same published formula, scale-safe norm. */
        r = hypot(hypot(tiny_state.x, tiny_state.y), tiny_state.z);
        for (k = 0; k < 3; ++k) {
            beta[k] = tiny_vel[k] / C_AU_DAY;
            pnat[k] = test_state_component(&tiny_state, k) / r;
        }
        bm1 = sqrt(1.0 - (beta[0] * beta[0] + beta[1] * beta[1] + beta[2] * beta[2]));
        for (k = 0; k < 3; ++k) pdv += pnat[k] * beta[k];
        w1 = 1.0 + pdv / (1.0 + bm1);
        w2 = SRS / 1e-200;
        for (k = 0; k < 3; ++k) p[k] = pnat[k] * bm1 + w1 * beta[k] + w2 * (beta[k] - pdv * pnat[k]);
        ref_n = hypot(hypot(p[0], p[1]), p[2]);
        JPL_TEST_CHECK(isfinite(ref_n) && ref_n > 0.0);
        for (k = 0; k < 3; ++k) ref_dir[k] = p[k] / ref_n;
        n = hypot(hypot(got.x, got.y), got.z);
        JPL_TEST_CHECK(isfinite(n) && n > 0.0);
        for (k = 0; k < 3; ++k) {
            dir[k] = test_state_component(&got, k) / n;
            JPL_TEST_CHECK(isfinite(dir[k]));
            JPL_TEST_CHECK(fabs(dir[k] - ref_dir[k]) < 1e-12);
        }
        JPL_TEST_CHECK(fabs(n - 1.0) < 1e-12);
        JPL_TEST_CHECK(got.vx == 1.0 && got.vy == 2.0 && got.vz == 3.0);
        JPL_TEST_CHECK(jpl_apply_aberration(&aliased, tiny_vel, 1e-200, &aliased) == JPL_OK);
        JPL_TEST_CHECK(memcmp(&aliased, &got, sizeof got) == 0);
    }
    puts("aberration tests passed");
    return 0;
}
