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
    double st[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    double vobs[3] = {0.0, 0.01, 0.0};
    double out[6] = {0};
    JPL_TEST_CHECK(jpl_apply_aberration(st, vobs, out) == JPL_OK);
    JPL_TEST_CHECK(fabs(out[0] - st[0]) > 0.0);
    double r0 = test_norm3(st);
    double r1 = test_norm3(out);
    JPL_TEST_CHECK(fabs(r0 - r1) < 1e-9);
    /* Velocity passed through unchanged. */
    JPL_TEST_CHECK(out[3]==st[3] && out[4]==st[4] && out[5]==st[5]);
    double io[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    JPL_TEST_CHECK(jpl_apply_aberration(io, vobs, io) == JPL_OK);
    JPL_TEST_CHECK(isfinite(io[0]) && isfinite(io[3]));
    JPL_TEST_CHECK(io[3]==0.0 && io[4]==0.01 && io[5]==0.0);
    /* NULL inputs report ARGUMENT and leave outputs unchanged. */
    {
        double keep[6] = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
        memcpy(out, keep, sizeof out);
        JPL_TEST_CHECK(jpl_apply_aberration(NULL, vobs, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(out, keep, sizeof out) == 0);
        JPL_TEST_CHECK(jpl_apply_aberration(st, NULL, out) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(out, keep, sizeof out) == 0);
        JPL_TEST_CHECK(jpl_apply_aberration(st, vobs, NULL) == JPL_ERROR_ARGUMENT);
    }
    /* Zero position passes through with unchanged velocity. */
    {
        double z[6]={0,0,0,0.001,0.002,0.003}, zo[6];
        JPL_TEST_CHECK(jpl_apply_aberration(z, vobs, zo) == JPL_OK);
        for (int i=0;i<6;++i) JPL_TEST_CHECK(zo[i]==z[i]);
    }
    /* Zero observer velocity leaves position unchanged. */
    {
        double zobs[3]={0,0,0}, so[6];
        JPL_TEST_CHECK(jpl_apply_aberration(st, zobs, so) == JPL_OK);
        JPL_TEST_CHECK(fabs(so[0]-st[0])<1e-15 && fabs(so[1]-st[1])<1e-15 && fabs(so[2]-st[2])<1e-15);
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
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*C_KM_S*DAYSEC/AU_KM, vc[1]*C_KM_S*DAYSEC/AU_KM, vc[2]*C_KM_S*DAYSEC/AU_KM};
        double o2[6];
        JPL_TEST_CHECK(jpl_apply_aberration(s2, vo2, o2) == JPL_OK);
        JPL_TEST_CHECK(fabs(o2[0] - (-0.7631631094219556269)) < 1e-9);
        JPL_TEST_CHECK(fabs(o2[1] - (-0.6087553082505590832)) < 1e-9);
        JPL_TEST_CHECK(fabs(o2[2] - (-0.2167926269368471279)) < 1e-9);
    }
    /* Grid vs eraAb directly (s=1e12 nulls the gravity term; expect 1e-12). */
    {
        static const double C_AU_DAY = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        double dirs[4][3] = {{1,0,0},{0,1,0},{0,0,1},{0.5773502691896258,0.5773502691896258,0.5773502691896258}};
        double vels[3][3] = {{0.01,0.002,-0.001},{-0.005,0.008,0.003},{0.0,0.0,0.0}};
        int d, q, k;
        for (d = 0; d < 4; ++d) for (q = 0; q < 3; ++q) {
            double s6[6], vo[3], got[6], be[3], exp[3], b2 = 0.0, bm;
            for (k = 0; k < 3; ++k) { s6[k] = dirs[d][k]*2.5; s6[k+3] = 0.001*k; vo[k] = vels[q][k]; be[k] = vo[k]/C_AU_DAY; b2 += be[k]*be[k]; }
            bm = sqrt(1.0 - b2);
            JPL_TEST_CHECK(jpl_apply_aberration(s6, vo, got) == JPL_OK);
            eraAb(dirs[d], be, 1e12, bm, exp);
            for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]*2.5) < 1e-12);
            for (k = 0; k < 3; ++k) JPL_TEST_CHECK(got[k+3] == s6[k+3]);
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
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {0.0, 0.0, 0.0};
        double o2[6];
        vo2[0] = vc[0] * c_au_day; vo2[1] = vc[1] * c_au_day; vo2[2] = vc[2] * c_au_day;
        JPL_TEST_CHECK(jpl_apply_aberration_full(s2, vo2, 0.99980921395708788, o2) == JPL_OK);
        JPL_TEST_CHECK(fabs(o2[0] - (-0.7631631094219556269)) < 1e-12);
        JPL_TEST_CHECK(fabs(o2[1] - (-0.6087553082505590832)) < 1e-12);
        JPL_TEST_CHECK(fabs(o2[2] - (-0.2167926269368471279)) < 1e-12);
        /* Non-positive Sun distance falls back to wrapper behavior. */
        { double ow[6]; JPL_TEST_CHECK(jpl_apply_aberration(s2, vo2, ow) == JPL_OK);
          JPL_TEST_CHECK(jpl_apply_aberration_full(s2, vo2, 0.0, o2) == JPL_OK);
          for (int k = 0; k < 6; ++k) JPL_TEST_CHECK(o2[k] == ow[k]);
          /* Negative finite distance omits the term identically. */
          JPL_TEST_CHECK(jpl_apply_aberration_full(s2, vo2, -2.5, o2) == JPL_OK);
          for (int k = 0; k < 6; ++k) JPL_TEST_CHECK(o2[k] == ow[k]); }
        /* NULL-safe: ARGUMENT with unchanged outputs. */
        { double keep[6] = {3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
          memcpy(o2, keep, sizeof o2);
          JPL_TEST_CHECK(jpl_apply_aberration_full(NULL, vo2, 1.0, o2) == JPL_ERROR_ARGUMENT);
          JPL_TEST_CHECK(memcmp(o2, keep, sizeof o2) == 0);
          JPL_TEST_CHECK(jpl_apply_aberration_full(s2, NULL, 1.0, o2) == JPL_ERROR_ARGUMENT);
          JPL_TEST_CHECK(memcmp(o2, keep, sizeof o2) == 0);
          JPL_TEST_CHECK(jpl_apply_aberration_full(s2, vo2, 1.0, NULL) == JPL_ERROR_ARGUMENT); }
    }
    /* Differential vs eraAb at finite Sun distance (catches SRS drift). */
    {
        static const double C_KM_S = JPL_TEST_C_KM_PER_SECOND;
        static const double AU_KM = JPL_TEST_AU_KM;
        static const double DAYSEC = JPL_TEST_DAYSEC;
        double c_au_day = C_KM_S * DAYSEC / AU_KM;
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {0.0, 0.0, 0.0};
        double got[6], be[3], exp[3];
        double s_au = 0.99980921395708788;
        int k;
        vo2[0] = vc[0] * c_au_day; vo2[1] = vc[1] * c_au_day; vo2[2] = vc[2] * c_au_day;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        JPL_TEST_CHECK(jpl_apply_aberration_full(s2, vo2, s_au, got) == JPL_OK);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]) < 1e-12);
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
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {0.0, 0.0, 0.0};
        double got[6], be[3], exp[3];
        double s_au = 0.05;
        int k;
        vo2[0] = vc[0] * c_au_day; vo2[1] = vc[1] * c_au_day; vo2[2] = vc[2] * c_au_day;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        JPL_TEST_CHECK(jpl_apply_aberration_full(s2, vo2, s_au, got) == JPL_OK);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]) < 1e-12);
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
            double s[6] = {0.6 * sc, -0.8 * sc, 0.0, 1.0, 2.0, 3.0};
            double v[3] = {0.0, 0.01, 0.0};
            double got[6], aliased[6], dir[3], n;
            memcpy(aliased, s, sizeof s);
            JPL_TEST_CHECK(jpl_apply_aberration(s, v, got) == JPL_OK);
            /* Velocity slots pass through unchanged. */
            JPL_TEST_CHECK(got[3] == 1.0 && got[4] == 2.0 && got[5] == 3.0);
            /* Exact permitted aliasing: in-place equals out-of-place. */
            JPL_TEST_CHECK(jpl_apply_aberration(aliased, v, aliased) == JPL_OK);
            for (k = 0; k < 6; ++k) JPL_TEST_CHECK(aliased[k] == got[k]);
            /* Direction is finite and shared across scales. The test norm
             * itself must be scale-safe (nested hypot): an unscaled
             * sqrt(x*x+...) underflows at 1e-200. */
            n = hypot(hypot(got[0], got[1]), got[2]);
            JPL_TEST_CHECK(isfinite(n) && n > 0.0);
            for (k = 0; k < 3; ++k) {
                dir[k] = got[k] / n;
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
        double s[6] = {1e200,0,0,1,2,3}, v[3] = {0,0.01,0}, out2[6];
        JPL_TEST_CHECK(jpl_apply_aberration(s, v, out2) == JPL_OK);
        JPL_TEST_CHECK(isfinite(out2[0]) && isfinite(out2[1]));
        JPL_TEST_CHECK(out2[1] > 0.0);
        JPL_TEST_CHECK(out2[3] == 1 && out2[4] == 2 && out2[5] == 3);
    }
    /* Standalone validation matrix: nonfinite slots, superluminal/c
     * observer, and nonfinite Sun distance report ARGUMENT with outputs
     * unchanged; validation precedes the zero-position shortcut. */
    {
        static const double C_AU_DAY = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        double keep[6] = {1.5, -2.5, 3.5, 4.5, 5.5, 6.5};
        double bad[6], vv[3], oo[6], z[6] = {0,0,0,0.001,0.002,0.003};
        memcpy(oo, keep, sizeof oo);
        memcpy(bad, st, sizeof bad); bad[2] = NAN;
        JPL_TEST_CHECK(jpl_apply_aberration(bad, vobs, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        memcpy(bad, st, sizeof bad); bad[5] = INFINITY;
        JPL_TEST_CHECK(jpl_apply_aberration(bad, vobs, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        vv[0] = vobs[0]; vv[1] = vobs[1]; vv[2] = NAN;
        JPL_TEST_CHECK(jpl_apply_aberration(st, vv, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        /* Observer at exactly c and above c are both invalid. */
        vv[0] = C_AU_DAY; vv[1] = 0.0; vv[2] = 0.0;
        JPL_TEST_CHECK(jpl_apply_aberration(st, vv, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        vv[0] = 2.0 * C_AU_DAY;
        JPL_TEST_CHECK(jpl_apply_aberration(st, vv, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        /* Zero position with a nonfinite velocity slot still fails. */
        vv[0] = 0.0; vv[1] = INFINITY; vv[2] = 0.0;
        JPL_TEST_CHECK(jpl_apply_aberration(z, vv, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        /* Nonfinite Sun distance is ARGUMENT, not a silent omission. */
        JPL_TEST_CHECK(jpl_apply_aberration_full(st, vobs, NAN, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
        JPL_TEST_CHECK(jpl_apply_aberration_full(st, vobs, INFINITY, oo) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
    }
    /* T16: pinned apparent-oracle velocity slot. The oracle's
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
        double keep[6] = {1.5, -2.5, 3.5, 4.5, 5.5, 6.5};
        double oo[6];
        memcpy(oo, keep, sizeof oo);
        JPL_TEST_CHECK(jpl_apply_aberration_full(st, vobs, 1e-320, oo) == JPL_ERROR_NUMERIC);
        JPL_TEST_CHECK(memcmp(oo, keep, sizeof oo) == 0);
    }
    /* F05: finite tiny-Sun regression. A unit-scale nonradial state with a
     * finite subluminal observer velocity and sun_distance_au = 1e-200
     * must succeed with finite output, preserved distance, unchanged
     * velocity, and exact in-place aliasing. The expected direction is an
     * independent scale-safe normalization of the same published expression
     * (cf. ERFA eraAb), compared within 1e-12. Before the fix, direct
     * squaring of the large p vector returned JPL_ERROR_NUMERIC. */
    {
        static const double tiny_state[6] = {0.6, -0.8, 0.0, 1.0, 2.0, 3.0};
        static const double tiny_vel[3] = {0.0, 0.01, 0.0};
        double got[6], aliased[6], dir[3], n, ref_dir[3], ref_n;
        double beta[3], pnat[3], p[3], bm1, pdv = 0.0, w1, w2;
        double C_AU_DAY = JPL_TEST_C_KM_PER_SECOND * JPL_TEST_DAYSEC / JPL_TEST_AU_KM;
        double SRS = 1.97412574336e-8;
        double r;
        int k;
        memcpy(aliased, tiny_state, sizeof aliased);
        JPL_TEST_CHECK(jpl_apply_aberration_full(tiny_state, tiny_vel, 1e-200, got) == JPL_OK);
        for (k = 0; k < 6; ++k) JPL_TEST_CHECK(isfinite(got[k]));
        /* Independent reference: same published formula, scale-safe norm. */
        r = hypot(hypot(tiny_state[0], tiny_state[1]), tiny_state[2]);
        for (k = 0; k < 3; ++k) {
            beta[k] = tiny_vel[k] / C_AU_DAY;
            pnat[k] = tiny_state[k] / r;
        }
        bm1 = sqrt(1.0 - (beta[0] * beta[0] + beta[1] * beta[1] + beta[2] * beta[2]));
        for (k = 0; k < 3; ++k) pdv += pnat[k] * beta[k];
        w1 = 1.0 + pdv / (1.0 + bm1);
        w2 = SRS / 1e-200;
        for (k = 0; k < 3; ++k) p[k] = pnat[k] * bm1 + w1 * beta[k] + w2 * (beta[k] - pdv * pnat[k]);
        ref_n = hypot(hypot(p[0], p[1]), p[2]);
        JPL_TEST_CHECK(isfinite(ref_n) && ref_n > 0.0);
        for (k = 0; k < 3; ++k) ref_dir[k] = p[k] / ref_n;
        n = hypot(hypot(got[0], got[1]), got[2]);
        JPL_TEST_CHECK(isfinite(n) && n > 0.0);
        for (k = 0; k < 3; ++k) {
            dir[k] = got[k] / n;
            JPL_TEST_CHECK(isfinite(dir[k]));
            JPL_TEST_CHECK(fabs(dir[k] - ref_dir[k]) < 1e-12);
        }
        JPL_TEST_CHECK(fabs(n - 1.0) < 1e-12);
        JPL_TEST_CHECK(got[3] == 1.0 && got[4] == 2.0 && got[5] == 3.0);
        JPL_TEST_CHECK(jpl_apply_aberration_full(aliased, tiny_vel, 1e-200, aliased) == JPL_OK);
        for (k = 0; k < 6; ++k) JPL_TEST_CHECK(aliased[k] == got[k]);
    }
    puts("aberration tests passed");
    return 0;
}
