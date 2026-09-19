/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Independent numerical-reference generator (test development only).
 * Compiled with upstream ERFA sources directly; never links libjpl and
 * never calls its production APIs. Ordinary tests consume the checked-in
 * header and never regenerate it. Usage:
 *   jpl_generate_reference --output <new-header-path>
 * Refuses to overwrite an existing path so tracked truth cannot be
 * replaced silently. Exit 0 on success, 1 on any failure. */
#include "erfa.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Exact physical inputs shared with the test-side mock builders.
 * Cited sources: IAU 2012 Resolution B2 (AU = 149597870.7 km exact),
 * SI light speed 299792.458 km/s exact, day 86400 s exact. Light speed
 * in AU/day is derived, never a rounded literal. */
#define GEN_AU_KM 149597870.7
#define GEN_DAYSEC 86400.0
#define GEN_C_KM_S 299792.458
#define GEN_C_AU_DAY (GEN_C_KM_S * GEN_DAYSEC / GEN_AU_KM)
/* Reference TDB epoch (sum) for the linear apparent-position oracle and
 * the 1-day mock coverage [2451545.0, 2451546.0]. */
#define GEN_T0 2451545.5
#define GEN_SPAN_START 2451545.0

static const double GEN_P[3] = {1.0, 0.4, -0.2};
static const double GEN_V[3] = {0.005, -0.003, 0.001};
static const double GEN_O[3] = {0.1, -0.2, 0.03};
static const double GEN_W[3] = {0.002, 0.004, -0.001};
static const double GEN_A[3] = {0.0001, -0.00005, 0.00002};

static double vec_norm3(const double v[3]) {
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Analytic positive root of |r - v*tau| = c*tau with the observer held
 * at reception: tau = rr / (sqrt(rv*rv + (c*c-vv)*rr) + rv). */
static double analytic_tau(const double r[3], const double v[3], double c) {
    double rv = r[0] * v[0] + r[1] * v[1] + r[2] * v[2];
    double vv = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    double rr = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
    return rr / (sqrt(rv * rv + (c * c - vv) * rr) + rv);
}

static void print_double(FILE *f, double v) {
    fprintf(f, "%a", v);
}

int main(int argc, char **argv) {
    static const double epochs[][2] = {
        {2451545.0, 0.0}, {2451620.5, 0.0}, {2451800.5, 0.0},
        {-3100000.5, 0x1p-40}, {8000000.5, 0x1p-40}
    };
    /* TT-TDB anchor and per-epoch corrections live here; the emission
     * test uses tdb directly so the low-part arithmetic is identical on
     * both sides. */
    static const double tt_inputs[][2] = {{2451545.0, 0.5}, {2451545.0, 0.6}};
    static const int use_accel[2] = {0, 1};
    const char *out_path;
    FILE *probe, *out;
    double anchor;
    size_t e, k;
    double app_tt[2][2], app_state[2][6], app_tau[2];
    double tt_corr[5], tt_ecm[5][3][3];

    if (argc != 3 || strcmp(argv[1], "--output") != 0) {
        fprintf(stderr, "usage: jpl_generate_reference --output <new-header-path>\n");
        return 1;
    }
    out_path = argv[2];
    /* Never silently overwrite tracked truth. */
    probe = fopen(out_path, "rb");
    if (probe != NULL) {
        fclose(probe);
        fprintf(stderr, "jpl_generate_reference: refusing to overwrite existing '%s'\n", out_path);
        return 1;
    }
    /* Published ERFA anchor (ERFA t_erfa_c.c, t_dtdb): checks the
     * generator's upstream configuration before emitting anything. */
    anchor = eraDtdb(2448939.5, 0.123, 0.76543, 5.0123, 5525.242, 3190.0);
    if (fabs(anchor - (-0.1280368005936998991e-2)) > 1e-15) {
        fprintf(stderr, "jpl_generate_reference: ERFA anchor mismatch: %a\n", anchor);
        return 1;
    }
    /* Absolute geocentric time/frame values from upstream primitives. */
    for (e = 0; e < 5; ++e) {
        tt_corr[e] = eraDtdb(epochs[e][0], epochs[e][1], 0.0, 0.0, 0.0, 0.0);
        if (!isfinite(tt_corr[e])) {
            fprintf(stderr, "jpl_generate_reference: nonfinite eraDtdb at epoch %u\n",
                (unsigned)e);
            return 1;
        }
        eraEcm06(epochs[e][0], epochs[e][1], tt_ecm[e]);
        for (k = 0; k < 9; ++k) {
            if (!isfinite((&tt_ecm[e][0][0])[k])) {
                fprintf(stderr, "jpl_generate_reference: nonfinite eraEcm06 at epoch %u\n",
                    (unsigned)e);
                return 1;
            }
        }
    }
    /* Independent analytic end-to-end apparent-position oracle. */
    for (e = 0; e < 2; ++e) {
        double tt0 = tt_inputs[e][0], tt1 = tt_inputs[e][1];
        double corr, tdb, d_rec, d_emi, tau, range, sdist, bm1, n, resid;
        double rt[3], ro[3], r[3], re[3], g[3], pnat[3], vobs[3], beta[3];
        double ppr[3], pos[3];
        int accel = use_accel[e];
        corr = eraDtdb(tt0, tt1, 0.0, 0.0, 0.0, 0.0);
        tdb = (tt0 + tt1) + corr / GEN_DAYSEC;
        d_rec = tdb - GEN_T0;
        for (k = 0; k < 3; ++k) {
            rt[k] = GEN_P[k] + GEN_V[k] * d_rec;
            ro[k] = GEN_O[k] + GEN_W[k] * d_rec + (accel ? GEN_A[k] * d_rec * d_rec : 0.0);
            r[k] = rt[k] - ro[k];
        }
        tau = analytic_tau(r, GEN_V, GEN_C_AU_DAY);
        d_emi = d_rec - tau;
        for (k = 0; k < 3; ++k) {
            re[k] = GEN_P[k] + GEN_V[k] * d_emi;
            g[k] = re[k] - ro[k];
            vobs[k] = GEN_W[k] + (accel ? 2.0 * GEN_A[k] * d_rec : 0.0);
            beta[k] = vobs[k] / GEN_C_AU_DAY;
        }
        range = vec_norm3(g);
        /* Cross-check the analytic equation residual before pinning. */
        resid = fabs(tau - range / GEN_C_AU_DAY);
        if (!(resid <= 1e-12)) {
            fprintf(stderr, "jpl_generate_reference: light-time residual %a too large\n", resid);
            return 1;
        }
        for (k = 0; k < 3; ++k) pnat[k] = g[k] / range;
        sdist = vec_norm3(ro);
        {
            double b2 = beta[0] * beta[0] + beta[1] * beta[1] + beta[2] * beta[2];
            bm1 = sqrt(1.0 - b2);
        }
        eraAb(pnat, beta, sdist, bm1, ppr);
        n = vec_norm3(ppr);
        for (k = 0; k < 3; ++k) {
            pos[k] = ppr[k] * (range / n);
            app_state[e][k] = pos[k];
            app_state[e][k + 3] = GEN_V[k] - vobs[k];
            if (!isfinite(app_state[e][k]) || !isfinite(app_state[e][k + 3])) {
                fprintf(stderr, "jpl_generate_reference: nonfinite apparent state\n");
                return 1;
            }
        }
        app_tt[e][0] = tt0;
        app_tt[e][1] = tt1;
        app_tau[e] = tau;
    }
    out = fopen(out_path, "wb");
    if (out == NULL) {
        fprintf(stderr, "jpl_generate_reference: cannot open '%s' for writing\n", out_path);
        return 1;
    }
    fprintf(out,
        "/* Copyright (c) 2026 Todd Carnes\n"
        " * SPDX-License-Identifier: MIT\n"
        " * See LICENSE in the project root for the full license text.\n"
        " */\n"
        "/* Pinned independent numerical reference (test-only, generated).\n"
        " * Produced by tools/generate_numerical_reference.c calling upstream\n"
        " * ERFA primitives directly (eraDtdb/eraEcm06/eraAb), never libjpl.\n"
        " * ERFA v2.0.1 (SOFA 20231011); anchor eraDtdb(2448939.5, 0.123,\n"
        " * 0.76543, 5.0123, 5525.242, 3190.0) = -0.1280368005936998991e-2\n"
        " * (ERFA t_erfa_c.c, t_dtdb, tolerance 1e-15), verified at generation.\n"
        " * Review tools/generate_numerical_reference.c and\n"
        " * tests/data/numerical_reference.md before trusting these values.\n"
        " * Apparent-oracle mock: barycentric target P={1,0.4,-0.2},\n"
        " * V={0.005,-0.003,0.001}; observer O={0.1,-0.2,0.03},\n"
        " * W={0.002,0.004,-0.001} AU/day modern AU, Sun fixed at origin;\n"
        " * case 1 linear, case 2 adds A={0.0001,-0.00005,0.00002}*d^2 to\n"
        " * the observer; T0=2451545.5, coverage [2451545.0, 2451546.0],\n"
        " * TT inputs {2451545.0,0.5} and {2451545.0,0.6}; velocity V-W.\n"
        " */\n"
        "#ifndef LIBJPL_NUMERICAL_REFERENCE_H\n"
        "#define LIBJPL_NUMERICAL_REFERENCE_H\n\n"
        "#include <stddef.h>\n\n"
        "typedef struct jpl_time_reference {\n"
        "    double tt[2], tdb_minus_tt_seconds, icrs_to_ecliptic[3][3];\n"
        "} jpl_time_reference;\n"
        "typedef struct jpl_apparent_reference {\n"
        "    double tt[2], state[6], delay_days;\n"
        "} jpl_apparent_reference;\n\n");
    fprintf(out, "static const jpl_time_reference jpl_time_references[] = {\n");
    for (e = 0; e < 5; ++e) {
        int i, j;
        fprintf(out, "    { { ");
        print_double(out, epochs[e][0]);
        fprintf(out, ", ");
        print_double(out, epochs[e][1]);
        fprintf(out, " }, ");
        print_double(out, tt_corr[e]);
        fprintf(out, ", { ");
        for (i = 0; i < 3; ++i) {
            fprintf(out, "{ ");
            for (j = 0; j < 3; ++j) {
                print_double(out, tt_ecm[e][i][j]);
                if (j < 2) fprintf(out, ", ");
            }
            fprintf(out, " }");
            if (i < 2) fprintf(out, ", ");
        }
        fprintf(out, " } }%s\n", e < 4 ? "," : "");
    }
    fprintf(out, "};\nstatic const size_t jpl_time_reference_count = 5;\n\n");
    fprintf(out, "static const jpl_apparent_reference jpl_apparent_references[] = {\n");
    for (e = 0; e < 2; ++e) {
        fprintf(out, "    { { ");
        print_double(out, app_tt[e][0]);
        fprintf(out, ", ");
        print_double(out, app_tt[e][1]);
        fprintf(out, " }, { ");
        for (k = 0; k < 6; ++k) {
            print_double(out, app_state[e][k]);
            if (k < 5) fprintf(out, ", ");
        }
        fprintf(out, " }, ");
        print_double(out, app_tau[e]);
        fprintf(out, " }%s\n", e < 1 ? "," : "");
    }
    fprintf(out, "};\nstatic const size_t jpl_apparent_reference_count = 2;\n\n#endif\n");
    if (fclose(out) != 0) {
        fprintf(stderr, "jpl_generate_reference: failed to close '%s'\n", out_path);
        return 1;
    }
    printf("generated %s: 5 time + 2 apparent references (anchor %a)\n", out_path, anchor);
    return 0;
}
