#include "jpl.h"
#include "jpl_test_util.h"
#include "erfa.h"
#include <math.h>
#include <stdio.h>

int main(void) {
    double st[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    double vobs[3] = {0.0, 0.01, 0.0};
    double out[6] = {0};
    jpl_apply_aberration(st, vobs, out);
    JPL_TEST_CHECK(fabs(out[0] - st[0]) > 0.0);
    double r0 = test_norm3(st);
    double r1 = test_norm3(out);
    JPL_TEST_CHECK(fabs(r0 - r1) < 1e-9);
    /* Velocity passed through unchanged. */
    JPL_TEST_CHECK(out[3]==st[3] && out[4]==st[4] && out[5]==st[5]);
    double io[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    jpl_apply_aberration(io, vobs, io);
    JPL_TEST_CHECK(isfinite(io[0]) && isfinite(io[3]));
    JPL_TEST_CHECK(io[3]==0.0 && io[4]==0.01 && io[5]==0.0);
    /* NULL-safe: must not crash. */
    jpl_apply_aberration(NULL, vobs, out);
    jpl_apply_aberration(st, NULL, out);
    jpl_apply_aberration(st, vobs, NULL);
    /* Zero position passes through. */
    {
        double z[6]={0,0,0,0.001,0.002,0.003}, zo[6];
        jpl_apply_aberration(z, vobs, zo);
        for (int i=0;i<6;++i) JPL_TEST_CHECK(zo[i]==z[i]);
    }
    /* Zero observer velocity leaves position unchanged. */
    {
        double zobs[3]={0,0,0}, so[6];
        jpl_apply_aberration(st, zobs, so);
        JPL_TEST_CHECK(fabs(so[0]-st[0])<1e-15 && fabs(so[1]-st[1])<1e-15 && fabs(so[2]-st[2])<1e-15);
    }
    /* ERFA t_erfa_c vector (t_ab): solar-gravity term differs <=0.4uas, so 1e-9. */
    {
        static const double C = JPL_TEST_C_AU_PER_DAY;
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*C, vc[1]*C, vc[2]*C};
        double o2[6];
        jpl_apply_aberration(s2, vo2, o2);
        JPL_TEST_CHECK(fabs(o2[0] - (-0.7631631094219556269)) < 1e-9);
        JPL_TEST_CHECK(fabs(o2[1] - (-0.6087553082505590832)) < 1e-9);
        JPL_TEST_CHECK(fabs(o2[2] - (-0.2167926269368471279)) < 1e-9);
    }
    /* Grid vs eraAb directly (s=1e12 nulls the gravity term; expect 1e-12). */
    {
        static const double C = JPL_TEST_C_AU_PER_DAY;
        double dirs[4][3] = {{1,0,0},{0,1,0},{0,0,1},{0.5773502691896258,0.5773502691896258,0.5773502691896258}};
        double vels[3][3] = {{0.01,0.002,-0.001},{-0.005,0.008,0.003},{0.0,0.0,0.0}};
        int d, q, k;
        for (d = 0; d < 4; ++d) for (q = 0; q < 3; ++q) {
            double s6[6], vo[3], got[6], be[3], exp[3], b2 = 0.0, bm;
            for (k = 0; k < 3; ++k) { s6[k] = dirs[d][k]*2.5; s6[k+3] = 0.001*k; vo[k] = vels[q][k]; be[k] = vo[k]/C; b2 += be[k]*be[k]; }
            bm = sqrt(1.0 - b2);
            jpl_apply_aberration(s6, vo, got);
            eraAb(dirs[d], be, 1e12, bm, exp);
            for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]*2.5) < 1e-12);
            for (k = 0; k < 3; ++k) JPL_TEST_CHECK(got[k+3] == s6[k+3]);
        }
    }
    /* Full variant matches eraAb including the solar term: same t_ab
     * inputs at the true Sun distance agree to 1e-12. */
    {
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*JPL_TEST_C_AU_PER_DAY, vc[1]*JPL_TEST_C_AU_PER_DAY, vc[2]*JPL_TEST_C_AU_PER_DAY};
        double o2[6];
        jpl_apply_aberration_full(s2, vo2, 0.99980921395708788, o2);
        JPL_TEST_CHECK(fabs(o2[0] - (-0.7631631094219556269)) < 1e-12);
        JPL_TEST_CHECK(fabs(o2[1] - (-0.6087553082505590832)) < 1e-12);
        JPL_TEST_CHECK(fabs(o2[2] - (-0.2167926269368471279)) < 1e-12);
        /* Non-positive Sun distance falls back to wrapper behavior. */
        { double ow[6]; jpl_apply_aberration(s2, vo2, ow);
          jpl_apply_aberration_full(s2, vo2, 0.0, o2);
          for (int k = 0; k < 6; ++k) JPL_TEST_CHECK(o2[k] == ow[k]); }
        /* NULL-safe. */
        jpl_apply_aberration_full(NULL, vo2, 1.0, o2);
        jpl_apply_aberration_full(s2, NULL, 1.0, o2);
        jpl_apply_aberration_full(s2, vo2, 1.0, NULL);
    }
    /* Differential vs eraAb at finite Sun distance (catches SRS drift). */
    {
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*JPL_TEST_C_AU_PER_DAY, vc[1]*JPL_TEST_C_AU_PER_DAY, vc[2]*JPL_TEST_C_AU_PER_DAY};
        double got[6], be[3], exp[3];
        double s_au = 0.99980921395708788;
        int k;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        jpl_apply_aberration_full(s2, vo2, s_au, got);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]) < 1e-12);
    }
    /* SRS-amplifying differential: at small Sun distance the solar term
     * exceeds 1e-12, so half-SRS code fails this block (verified by
     * stash experiment); pins JPL_SRS_AU against eraAb. */
    {
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*JPL_TEST_C_AU_PER_DAY, vc[1]*JPL_TEST_C_AU_PER_DAY, vc[2]*JPL_TEST_C_AU_PER_DAY};
        double got[6], be[3], exp[3];
        double s_au = 0.05;
        int k;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        jpl_apply_aberration_full(s2, vo2, s_au, got);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]) < 1e-12);
    }
    puts("aberration tests passed");
    return 0;
}
