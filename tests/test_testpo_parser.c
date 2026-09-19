/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_testpo_util.h"

#include "jpl_test_util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int check_tol(double got, double want, const char *expr, int line) {
    if (!(fabs(got - want) <= 1e-18)) {
        fprintf(stderr, "CHECK FAILED %s:%d: %s: got %.17g, want %.17g\n",
                __FILE__, line, expr, got, want);
        return 0;
    }
    return 1;
}

#define CHECK_TOL(got, want) JPL_TEST_CHECK(check_tol((got), (want), #got, __LINE__))

int main(void) {
    jpl_testpo_row r;
    jpl_testpo_row sentinel;

    memset(&sentinel, 0xA5, sizeof sentinel);

    /* Required parser assertions from the task. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 4 12 1 1.25", &r) == 1);
    JPL_TEST_CHECK(r.denum == 441 && r.jd[0] == 2451544.5 && r.expected == 1.25);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 -9999.01.01-1931076.5 4 12 1 1.25", &r) == 1);
    JPL_TEST_CHECK(r.jd[0] == -1931076.5);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441-13200.09.01-3099998.5 4 12 1 1.25", &r) == 1);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 17 0 1 nan", &r) == -1);
    /* Invalid rows preserve the output buffer. */
    JPL_TEST_CHECK(memcmp(&r, &sentinel, sizeof r) == 0);

    /* Required precision regression: a 1e-12 day fraction invisible to a
     * scalar JD must survive in the low part. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row(
        "441 2000.01.01 2451545.000000000001 4 12 1 1", &r) == 1);
    JPL_TEST_CHECK(r.jd[0] == 2451545.0 && r.jd[1] > 0.0);
    JPL_TEST_CHECK(fabs(r.jd[1] - 1e-12) < 1e-27);

    /* A copied six-token rule that checks DENUM in the wrong field must
     * fail this row: the calendar label is not the DENUM field. */
    {
        /* Deliberate stale-rule reproduction: compares the fused label. */
        const char *line = "441 -9999.01.01-1931076.5 4 12 1 1.25";
        char copy[256];
        char *tokens[8];
        int ntok = 0;
        char *p;
        memcpy(copy, line, strlen(line) + 1u);
        p = copy;
        while (*p != '\0') {
            while (*p == ' ' || *p == '\t') {
                ++p;
            }
            if (*p == '\0') {
                break;
            }
            tokens[ntok++] = p;
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                ++p;
            }
            if (*p != '\0') {
                *p++ = '\0';
            }
        }
        JPL_TEST_CHECK(ntok == 6);
        /* Stale rule: DENUM check against the fused label fails. */
        JPL_TEST_CHECK(strcmp(tokens[1], "441") != 0);
        /* The checked parser accepts the same row via token zero. */
        r = sentinel;
        JPL_TEST_CHECK(jpl_testpo_parse_row(line, &r) == 1);
        JPL_TEST_CHECK(r.denum == 441);
    }

    /* Row forms, blanks, bad rows, and sentinel preservation. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("", &r) == 0);
    JPL_TEST_CHECK(jpl_testpo_parse_row("   \t  ", &r) == 0);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 12 4 5 -0.0138", &r) == 1);
    JPL_TEST_CHECK(r.target == 12 && r.center == 4 && r.component == 5);
    /* Bad DENUM / token counts. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("442 2000.01.01 2451544.5 12 4 5 -0.0138", &r) == 1);
    JPL_TEST_CHECK(r.denum == 442);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 12 4 5", &r) == -1);
    JPL_TEST_CHECK(memcmp(&r, &sentinel, sizeof r) == 0);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 12 4 5 -0.0138 extra", &r) == -1);
    JPL_TEST_CHECK(memcmp(&r, &sentinel, sizeof r) == 0);
    /* Headers/EOT never validate. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("EOT", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("KSIZE= 2036", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("de#  -- date -- -- jed -- t# c# x# -- coordinate ---", &r) == -1);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row(
        " DE-0441LE-0441 -13200-AUG-15 00:00 JD -3100015.50 to 17191-MAR-15 00:00 JD 8000016.50",
        &r) == -1);

    /* Bad body/component integers and huge values. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 0 12 1 1.25", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 16 0 1 1.25", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 4 16 1 1.25", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 4 12 7 1.25", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 4 12 1 1.25", &sentinel) == 1);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 99999999999999999999 12 1 1", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 4 12 99999999999999999999 1", &r) == -1);

    /* Signed zero and negative fractional dates. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 -0.5 4 12 1 1.0", &r) == 1);
    JPL_TEST_CHECK(r.jd[0] == -0.5 && r.jd[1] == 0.0);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 0.0 4 12 1 1.0", &r) == 1);
    JPL_TEST_CHECK(r.jd[0] == 0.0 && r.jd[1] == 0.0 && memcmp(r.jd, (double[2]){0.0, 0.0}, 16) == 0);

    /* Missing separator and malformed fused labels. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 20000101 2451544.5 4 12 1 1.25", &r) == 1);
    JPL_TEST_CHECK(r.jd[0] == 2451544.5);
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 -9999.01.01 4 12 1 1.25", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441-13200.09.01 4 12 1 1.25", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01- 4 12 1 1.25", &r) == -1);

    /* JD grammar: exponent notation rejected, fixed-point required. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2.451545e6 4 12 1 1", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2.451545D6 4 12 1 1", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01-2.451545e6 4 12 1 1", &r) == -1);
    /* An exponent minus inside the fused label must not split as a
     * calendar separator: this tail is still an invalid JD. */
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 4 12 1 1e3", &r) == 1);
    JPL_TEST_CHECK(r.expected == 1000.0);
    r = sentinel;

    /* Numeric conversion failures: incomplete conversions, nonfinite
     * expected, and overflow/underflow that changes the value. */
    r = sentinel;
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5x 4 12 1 1", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 4x 12 1 1", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 4 12 1 inf", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 4 12 1 -nan", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 4 12 1 1e999", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 4 12 1 1e-999", &r) == -1);
    JPL_TEST_CHECK(jpl_testpo_parse_row(
        "441 2000.01.01 9999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999"
        "999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999.5 4 12 1 1",
        &r) == -1);

    /* Oversize tokens and lines fail. */
    {
        static char long_token[140];
        static char long_line[512];
        static char over_line[4200];
        size_t k;
        for (k = 0; k < sizeof long_token - 1u; ++k) {
            long_token[k] = '9';
        }
        long_token[sizeof long_token - 1u] = '\0';
        for (k = 0; k < sizeof long_line - 1u; ++k) {
            long_line[k] = ' ';
        }
        long_line[sizeof long_line - 1u] = '\0';
        memcpy(long_line, "441 2000.01.01 ", 14u);
        memcpy(long_line + 14u, long_token, sizeof long_token - 1u);
        r = sentinel;
        JPL_TEST_CHECK(jpl_testpo_parse_row(long_line, &r) == -1);
        for (k = 0; k < sizeof over_line - 1u; ++k) {
            over_line[k] = ' ';
        }
        over_line[sizeof over_line - 1u] = '\0';
        memcpy(over_line, "441 2000.01.01 2451545.5 4 12 1 1 extra-token", 43u);
        r = sentinel;
        JPL_TEST_CHECK(jpl_testpo_parse_row(over_line, &r) == -1);
    }

    /* Component mapping: ordinary bodies, nutation rates, invalid slots. */
    {
        jpl_testpo_row body = {441, 4, 12, 3, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row nut1 = {441, 14, 0, 1, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row nut2 = {441, 14, 0, 2, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row nut3 = {441, 14, 0, 3, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row nut4 = {441, 14, 0, 4, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row nut5 = {441, 14, 0, 5, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row nut6 = {441, 14, 0, 6, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row lib = {441, 15, 0, 6, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row t17 = {441, 17, 0, 1, {2451545.0, 0.0}, 1.0};
        JPL_TEST_CHECK(jpl_testpo_state_index(&body) == 2);
        JPL_TEST_CHECK(jpl_testpo_state_index(&nut1) == 0);
        JPL_TEST_CHECK(jpl_testpo_state_index(&nut2) == 1);
        JPL_TEST_CHECK(jpl_testpo_state_index(&nut3) == 3);
        JPL_TEST_CHECK(jpl_testpo_state_index(&nut4) == 4);
        JPL_TEST_CHECK(jpl_testpo_state_index(&nut5) == -1);
        JPL_TEST_CHECK(jpl_testpo_state_index(&nut6) == -1);
        JPL_TEST_CHECK(jpl_testpo_state_index(&lib) == 5);
        JPL_TEST_CHECK(jpl_testpo_state_index(&t17) == -1);
        JPL_TEST_CHECK(jpl_testpo_state_index(NULL) == -1);
    }

    /* Tolerance policy: absolute envelopes, no magnitude factor. */
    {
        jpl_testpo_row body = {441, 4, 12, 1, {2451545.0, 0.0}, 1.0e6};
        jpl_testpo_row psi = {441, 15, 0, 3, {8000000.5, 0.0}, 1.0e6};
        jpl_testpo_row psi_rate = {441, 15, 0, 4, {2451545.0, 0.0}, 1.0};
        jpl_testpo_row ttv = {441, 17, 0, 1, {2451545.0, 0.0}, 0.001};
        double days = fabs(8000000.5 - 2440400.5);
        double want_psi = 5e-13 * (1.0 + 100.0 * days / 365.25);
        CHECK_TOL(jpl_testpo_tolerance(&body), 5e-13);
        CHECK_TOL(jpl_testpo_tolerance(&psi), want_psi);
        CHECK_TOL(jpl_testpo_tolerance(&psi_rate), 5e-12);
        CHECK_TOL(jpl_testpo_tolerance(&ttv), 1e-12);
        /* Far-epoch psi regression: a 1e-3 rad error must fail the new
         * bound (old double-scaled 5e-13*1e6*(epoch factor) accepted it). */
        JPL_TEST_CHECK(1e-3 > jpl_testpo_tolerance(&psi));
        JPL_TEST_CHECK(1e-3 < 5e-13 * 1.0e6 * (1.0 + 100.0 * days / 365.25));
    }

    puts("testpo parser tests passed");
    return 0;
}
