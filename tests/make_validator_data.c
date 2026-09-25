/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Validator data generator (test-only, owned by T15).
 * Usage: jpl_validator_data <output-dir>
 * Generates small valid mock ephemerides plus full-file runner inputs
 * (recognized header/EOT framing) covering every negative branch the
 * strict validators must reject. Uses the T04 mock writer only.
 * Exit 0 on success, 1 on any failure. */
#include "jpl_test_util.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define GOOD_DOUBLES 984
#define SMALL_DOUBLES 360

static int make_dir(const char *path) {
#if defined(_WIN32)
    if (_mkdir(path) != 0) {
        if (errno == EEXIST) {
            return 0;
        }
        return 1;
    }
    return 0;
#else
    if (mkdir(path, 0755) != 0) {
        if (errno == EEXIST) {
            return 0;
        }
        return 1;
    }
    return 0;
#endif
}

/* Full mock: Mars {3,1,1}, nutation {6,1,1}, libration {900,1,1},
 * TT-TDB {939,11,4}; coverage 2451545.0-2451546.0 step 1.0 DENUM 441.
 * Coefficient values are documented below so row writers match them. */
static int write_full_mock(const char *path, int denum, int with_jupiter) {    static unsigned char hdr[GOOD_DOUBLES * 8];
    static double constants[GOOD_DOUBLES];
    static double records[5 * GOOD_DOUBLES];
    jpl_test_mock mock;
    size_t i, r;

    memset(hdr, 0, sizeof hdr);
    memcpy(hdr, "validator full", 14);
    test_mock_header(hdr, 2451545.0, 2451550.0, 1.0, 2);
    test_put_ptr(hdr, 3, 3, 1, 1); /* Mars target 4 */
    if (with_jupiter) {
        test_put_ptr(hdr, 4, 20, 1, 1); /* Jupiter target 5 */
    }
    test_put_ptr(hdr, 11, 6, 1, 1); /* nutations */
    test_put_ptr(hdr, 12, (uint32_t)denum, 900, 1); /* DENUM + libration start/coeff */
    test_put_u32(hdr + 2852, 1); /* libration subintervals */
    test_put_u32(hdr + 2856, 1); /* T14 triple (ignored) */
    test_put_u32(hdr + 2860, 1000000u);
    test_put_u32(hdr + 2864, 1000000u);
    test_put_u32(hdr + 2868, 939); /* T15 = TT-TDB triple */
    test_put_u32(hdr + 2872, 11);
    test_put_u32(hdr + 2876, 4);
    for (i = 0; i < GOOD_DOUBLES; ++i) {
        constants[i] = 0.0;
    }
    constants[0] = 11.0;
    constants[1] = 22.0;
    /* Five advertised one-day records cover 2451545.0-2451550.0 so every
     * runner row lands in coverage; file size stays tiny (7*7872).
     * TT-TDB coefficients are identical in every record (c0 0.001, with
     * higher coefficients zero so multi-coefficient evaluation matches the
     * midpoint value exactly); the X1/X2 writer uses only record 0's
     * subinterval midpoint. */
    for (r = 0; r < 5; ++r) {
        double *rec = records + r * GOOD_DOUBLES;
        for (i = 0; i < GOOD_DOUBLES; ++i) {
            rec[i] = 0.0;
        }
        rec[0] = 2451545.0 + (double)r;
        rec[1] = 2451546.0 + (double)r;
        rec[2] = 5.0;
        rec[3] = 6.0;
        rec[4] = 7.0;
        if (with_jupiter) {
            rec[19] = 50.0;
            rec[20] = 60.0;
            rec[21] = 70.0;
        }
        rec[5] = 0.001;
        rec[6] = 0.002;
        rec[899] = 0.1;
        rec[900] = 0.2;
        rec[901] = 0.3;
        rec[938] = 0.001;
        rec[939] = 0.00002;
        rec[940] = 0.0;
        rec[941] = 0.0;
        rec[942] = 0.0;
        rec[943] = 0.0;
        rec[944] = 0.0;
        rec[945] = 0.0;
        rec[946] = 0.0;
        rec[947] = 0.0;
        rec[948] = 0.0;
        rec[949] = 0.002;
        rec[950] = 0.00003;
        rec[960] = 0.003;
        rec[961] = -0.00001;
        rec[971] = 0.004;
        rec[972] = 0.00005;
    }
    if (denum == -441) {
        records[2 * GOOD_DOUBLES + 0] = NAN; /* NaN interior start */
    }
    memset(&mock, 0, sizeof mock);
    mock.record_doubles = GOOD_DOUBLES;
    mock.advertised_records = 5;
    mock.denum = denum < 0 ? -denum : denum;
    mock.constant_count = 2;
    mock.start[0] = 2451545.0;
    mock.end[0] = 2451550.0;
    mock.step_days = 1.0;
    /* Nonmodern file AU (T08): reference rows stay file-AU while public
     * output is modern AU, exercising the runner conversion. Angular and
     * TT-TDB rows are never rescaled, so their values stay valid. */
    mock.au_km = JPL_TEST_NONMODERN_AU_KM;
    mock.emrat = JPL_TEST_MASS_RATIO;
    mock.header = hdr;
    mock.constants = constants;
    mock.records = records;
    return test_write_mock(path, &mock);
}

static int write_small_mock(const char *path, int denum) {
    static unsigned char hdr[SMALL_DOUBLES * 8];
    static double constants[SMALL_DOUBLES];
    static double records[SMALL_DOUBLES];
    jpl_test_mock mock;
    size_t i;

    memset(hdr, 0, sizeof hdr);
    memcpy(hdr, "validator sweep", 15);
    test_mock_header(hdr, 2451545.0, 2451546.0, 1.0, 2);
    test_put_ptr(hdr, 3, 3, 1, 1);
    test_put_ptr(hdr, 12, (uint32_t)denum, 0, 0);
    for (i = 0; i < SMALL_DOUBLES; ++i) {
        constants[i] = 0.0;
    }
    constants[0] = 11.0;
    constants[1] = 22.0;
    for (i = 0; i < SMALL_DOUBLES; ++i) {
        records[i] = 0.0;
    }
    records[0] = 2451545.0;
    records[1] = 2451546.0;
    records[2] = 5.0;
    records[3] = 6.0;
    records[4] = 7.0;
    memset(&mock, 0, sizeof mock);
    mock.record_doubles = SMALL_DOUBLES;
    mock.advertised_records = 1;
    mock.denum = denum;
    mock.constant_count = 2;
    mock.start[0] = 2451545.0;
    mock.end[0] = 2451546.0;
    mock.step_days = 1.0;
    mock.au_km = JPL_TEST_AU_KM;
    mock.emrat = JPL_TEST_MASS_RATIO;
    mock.header = hdr;
    mock.constants = constants;
    mock.records = records;
    return test_write_mock(path, &mock);
}

static int write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    size_t n;
    if (f == NULL) {
        return 1;
    }
    n = strlen(text);
    if (n > 0 && fwrite(text, 1, n, f) != n) {
        fclose(f);
        return 1;
    }
    return fclose(f) != 0 ? 1 : 0;
}

static const char *header_block(void) {
    return " DE-0441LE-0441 validator span 2451545.0 to 2451546.0\n"
           "\n"
           "KSIZE= 2092\n"
           "\n"
           "de#  -- date -- -- jed -- t# c# x# -- coordinate ---\n"
           "EOT\n";
}

/* A dynamic string buffer for building testpo files. */
typedef struct textbuf {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} textbuf;

static void tb_append(textbuf *tb, const char *text) {
    size_t n;
    if (tb->failed) {
        return;
    }
    n = strlen(text);
    if (tb->len + n + 1 > tb->cap) {
        size_t want = tb->len + n + 1 + 4096;
        char *grown = (char *)realloc(tb->data, want);
        if (grown == NULL) {
            tb->failed = 1;
            return;
        }
        tb->data = grown;
        tb->cap = want;
    }
    memcpy(tb->data + tb->len, text, n + 1);
    tb->len += n;
}

int main(int argc, char **argv) {
    const char *out = NULL;
    char path[1024];
    /* Reference rows live in file AU (nonmodern T08 fixture), exactly as
     * historical testpo rows do; the runners convert actuals back. */
    double au = JPL_TEST_NONMODERN_AU_KM;
    textbuf good, nan_rows, missing, angular, denum, malformed, badcomp;
    textbuf opos, oneg, overlong, farpsi, ttdbwrong, duponly;
    int rc = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: jpl_validator_data <output-dir>\n");
        return 2;
    }
    out = argv[1];

#define JOIN(dst, name)                                                        \
    do {                                                                       \
        if (snprintf(path, sizeof path, "%s/%s", out, name) < 0 ||             \
            (size_t)snprintf(path, sizeof path, "%s/%s", out, name) >=         \
                sizeof path) {                                                 \
            fprintf(stderr, "path too long: %s/%s\n", out, name);               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

    memset(&good, 0, sizeof good);
    memset(&nan_rows, 0, sizeof nan_rows);
    memset(&missing, 0, sizeof missing);
    memset(&angular, 0, sizeof angular);
    memset(&denum, 0, sizeof denum);
    memset(&malformed, 0, sizeof malformed);
    memset(&badcomp, 0, sizeof badcomp);
    memset(&opos, 0, sizeof opos);
    memset(&oneg, 0, sizeof oneg);
    memset(&overlong, 0, sizeof overlong);
    memset(&farpsi, 0, sizeof farpsi);
    memset(&ttdbwrong, 0, sizeof ttdbwrong);
    memset(&duponly, 0, sizeof duponly);

    /* Output directory plus sweep subdirectory (idempotent: later CTest
     * cases reuse the directory created by the first case). */
    JOIN(path, "sweep");
    if (make_dir(out) != 0 || make_dir(path) != 0) {
        fprintf(stderr, "could not create output dir %s (%s)\n", out, strerror(errno));
        return 1;
    }

    /* Ephemerides: five one-day records cover 2451545.0-2451550.0 so all
     * full-file rows land in coverage. */
    JOIN(path, "good.eph");
    if (write_full_mock(path, 441, 1) != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }
    JOIN(path, "missing.eph");
    if (write_full_mock(path, 441, 0) != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }
    JOIN(path, "angular.eph");
    if (write_full_mock(path, -441, 1) != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }
    JOIN(path, "farpsi.eph");
    if (write_full_mock(path, 441, 1) != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }
    /* Sweep directory: one good named DE441 file plus one five-byte named
     * DE102.eph (the first name the sweep probes, so it cannot be skipped
     * as absent). The five-byte probe exercises the size<2856 branch. */
    JOIN(path, "sweep/DE441.eph");
    if (write_small_mock(path, 441) != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }
    JOIN(path, "sweep/DE102.eph");
    if (write_text(path, "junk\n") != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }
    /* F11 interior-corruption sweep fixture: three covered records with
     * clean first/last endpoints and a NaN date in record one. Lives in
     * its own directory so open succeeds but the sweep must fail. */
    JOIN(path, "sweep_corrupt");
    if (make_dir(path) != 0) {
        fprintf(stderr, "could not create %s (%s)\n", path, strerror(errno));
        goto done;
    }
    {
        static unsigned char chdr[SMALL_DOUBLES * 8];
        static double crecs[3 * SMALL_DOUBLES];
        jpl_test_mock cmock;
        size_t k;
        memset(chdr, 0, sizeof chdr);
        memcpy(chdr, "validator corrupt", 17);
        test_mock_header(chdr, 2451545.0, 2451548.0, 1.0, 2);
        test_put_ptr(chdr, 3, 3, 1, 1);
        test_put_ptr(chdr, 12, (uint32_t)441, 0, 0);
        for (k = 0; k < 3 * SMALL_DOUBLES; ++k) crecs[k] = 0.0;
        crecs[0 * SMALL_DOUBLES + 0] = 2451545.0;
        crecs[0 * SMALL_DOUBLES + 1] = 2451546.0;
        crecs[0 * SMALL_DOUBLES + 2] = 5.0;
        crecs[1 * SMALL_DOUBLES + 0] = NAN;
        crecs[1 * SMALL_DOUBLES + 1] = 2451547.0;
        crecs[1 * SMALL_DOUBLES + 2] = 5.0;
        crecs[2 * SMALL_DOUBLES + 0] = 2451547.0;
        crecs[2 * SMALL_DOUBLES + 1] = 2451548.0;
        crecs[2 * SMALL_DOUBLES + 2] = 5.0;
        memset(&cmock, 0, sizeof cmock);
        cmock.record_doubles = SMALL_DOUBLES;
        cmock.advertised_records = 3;
        cmock.denum = 441;
        cmock.constant_count = 2;
        cmock.start[0] = 2451545.0;
        cmock.end[0] = 2451548.0;
        cmock.step_days = 1.0;
        cmock.au_km = JPL_TEST_AU_KM;
        cmock.emrat = JPL_TEST_MASS_RATIO;
        cmock.header = chdr;
        cmock.constants = NULL;
        cmock.records = crecs;
        JOIN(path, "sweep_corrupt/DE441.eph");
        if (test_write_mock(path, &cmock) != 0) {
            fprintf(stderr, "could not write %s\n", path);
            goto done;
        }
    }
    /* Trailing-record sweep control: two covered records plus one physical
     * trailing record. The sweep must pass: the final physical record
     * starts exactly at coverage end and stays ordered. */
    JOIN(path, "sweep_trailing");
    if (make_dir(path) != 0) {
        fprintf(stderr, "could not create %s (%s)\n", path, strerror(errno));
        goto done;
    }
    {
        static unsigned char thdr[SMALL_DOUBLES * 8];
        static double trecs[3 * SMALL_DOUBLES];
        jpl_test_mock tmock;
        size_t k;
        memset(thdr, 0, sizeof thdr);
        memcpy(thdr, "validator trailing", 18);
        test_mock_header(thdr, 2451545.0, 2451547.0, 1.0, 2);
        test_put_ptr(thdr, 3, 3, 1, 1);
        test_put_ptr(thdr, 12, (uint32_t)441, 0, 0);
        for (k = 0; k < 3 * SMALL_DOUBLES; ++k) trecs[k] = 0.0;
        trecs[0 * SMALL_DOUBLES + 0] = 2451545.0;
        trecs[0 * SMALL_DOUBLES + 1] = 2451546.0;
        trecs[0 * SMALL_DOUBLES + 2] = 5.0;
        trecs[1 * SMALL_DOUBLES + 0] = 2451546.0;
        trecs[1 * SMALL_DOUBLES + 1] = 2451547.0;
        trecs[1 * SMALL_DOUBLES + 2] = 5.0;
        trecs[2 * SMALL_DOUBLES + 0] = 2451547.0;
        trecs[2 * SMALL_DOUBLES + 1] = 2451548.0;
        trecs[2 * SMALL_DOUBLES + 2] = 5.0;
        memset(&tmock, 0, sizeof tmock);
        tmock.record_doubles = SMALL_DOUBLES;
        tmock.advertised_records = 2;
        tmock.trailing_record = 1;
        tmock.denum = 441;
        tmock.constant_count = 2;
        tmock.start[0] = 2451545.0;
        tmock.end[0] = 2451547.0;
        tmock.step_days = 1.0;
        tmock.au_km = JPL_TEST_AU_KM;
        tmock.emrat = JPL_TEST_MASS_RATIO;
        tmock.header = thdr;
        tmock.constants = NULL;
        tmock.records = trecs;
        JOIN(path, "sweep_trailing/DE441.eph");
        if (test_write_mock(path, &tmock) != 0) {
            fprintf(stderr, "could not write %s\n", path);
            goto done;
        }
    }
    /* Sweep control: a directory with only the good file (must pass). */
    JOIN(path, "sweep_good");
    if (make_dir(path) != 0) {
        fprintf(stderr, "could not create %s (%s)\n", path, strerror(errno));
        goto done;
    }
    JOIN(path, "sweep_good/DE441.eph");
    if (write_small_mock(path, 441) != 0) {
        fprintf(stderr, "could not write %s\n", path);
        goto done;
    }

    /* Good full-file input: Mars X1-X6, nutation X1-X4, libration X1-X6,
     * and TT-TDB X1-X2, using the actual mock coefficients (zero rates as
     * exact expected values). All three supported row grammars stay
     * represented by the spot checks below. */
    /* T#17 rows use Chebyshev midpoint value/rate, not raw c0/c1:
     * at a subinterval midpoint value = c0, rate = 2*c1/substep. All
     * five records carry identical coefficients, so every record's
     * first-subinterval midpoint reads 0.001 s. The TT-TDB wrong-sign
     * file below reuses the 2451545.125 midpoint for its deliberate
     * value failure. */
    tb_append(&good, header_block());
    {
        char row[256];
        int i;
        for (i = 0; i < 15; ++i) {
            double jd = 2451545.5 + (double)(i % 5);
            int c;
            /* Mars X1-X6 7-token, spread over all five records. */
            for (c = 1; c <= 6; ++c) {
                double want = c <= 3 ? (double)(c + 4) / au : 0.0;
                snprintf(row, sizeof row, "441 2000.01.01 %.1f 4 0 %d %.17g\n", jd, c, want);
                tb_append(&good, row);
            }
            /* Nutation X1-X4 7-token (X3/X4 are the rates at 0.0). */
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 14 0 1 %.17g\n", jd, 0.001);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 14 0 2 %.17g\n", jd, 0.002);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 14 0 3 %.17g\n", jd, 0.0);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 14 0 4 %.17g\n", jd, 0.0);
            tb_append(&good, row);
            /* Libration X1-X6 7-token (X4-X6 are rates at 0.0). */
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 15 0 1 %.17g\n", jd, 0.1);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 15 0 2 %.17g\n", jd, 0.2);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 15 0 3 %.17g\n", jd, 0.3);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 15 0 4 %.17g\n", jd, 0.0);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 15 0 5 %.17g\n", jd, 0.0);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 %.1f 15 0 6 %.17g\n", jd, 0.0);
            tb_append(&good, row);
            /* TT-TDB X1/X2 7-token: 2451545.5 sits at subinterval 2's left
             * edge (normalized -1), so value = c0-c1 = 0.003+0.00001 and
             * rate = 2*c1/substep with c1 = rec[961] = -0.00001, substep
             * 0.25 d. */
            snprintf(row, sizeof row, "441 2000.01.01 2451545.5 17 0 1 %.17g\n",
                     0.003 - -0.00001);
            tb_append(&good, row);
            snprintf(row, sizeof row, "441 2000.01.01 2451545.5 17 0 2 %.17g\n",
                     2.0 * -0.00001 / 0.25);
            tb_append(&good, row);
        }
        /* Token-form spot checks: identical epochs in 6-token and
         * 5-token fused forms, exercising all three grammars in both
         * fixture-capable (T4/T15) and full-file (T17) paths. */
        snprintf(row, sizeof row, "441 -9999.01.01-1931076.5 4 0 1 %.17g\n", 5.0 / au);
        tb_append(&good, row);
        snprintf(row, sizeof row, "441-13200.09.01-3099998.5 15 0 1 %.17g\n", 0.1);
        tb_append(&good, row);
    }
    /* F10 duplicate-only negative: 120 identical valid Mars X1 rows exceed
     * the 100-row minimum but fail the diversity check. */
    tb_append(&duponly, header_block());
    {
        char row[256];
        int i;
        for (i = 0; i < 120; ++i) {
            snprintf(row, sizeof row, "441 2000.01.01 2451545.5 4 0 1 %.17g\n", 5.0 / au);
            tb_append(&duponly, row);
        }
    }
    /* 100 NaN TT-TDB rows with proper framing so the failure is the NaN
     * reference, not missing framing. NaN COORD never parses as a valid
     * row (T03: nonfinite expected is invalid), so provoke the NaN
     * diagnostic separately: the first row carries NaN, and the runner
     * must report it as an invalid row (nonfinite reference). */
    tb_append(&nan_rows, header_block());
    {
        char row[256];
        int i;
        for (i = 0; i < 100; ++i) {
            snprintf(row, sizeof row, "441 2000.01.01 2451545.125 17 0 1 nan\n");
            tb_append(&nan_rows, row);
        }
    }
    /* 100 finite valid Mars rows plus one missing ordinary-body query
     * (Jupiter target 5 absent from missing.eph). */
    tb_append(&missing, header_block());
    {
        char row[256];
        int i;
        for (i = 0; i < 100; ++i) {
            snprintf(row, sizeof row, "441 2000.01.01 2451545.5 4 0 1 %.17g\n", 5.0 / au);
            tb_append(&missing, row);
        }
        snprintf(row, sizeof row, "441 2000.01.01 2451545.5 5 0 1 %.17g\n", 50.0 / au);
        tb_append(&missing, row);
    }
    /* Present angular dataset with a deliberate read error: nutation is
     * present in angular.eph but the middle record is corrupt, so the
     * in-coverage query at 2451547.5 must fail, not skip. */
    tb_append(&angular, header_block());
    {
        char row[256];
        snprintf(row, sizeof row, "441 2000.01.01 2451547.5 14 0 1 %.17g\n", 0.001);
        tb_append(&angular, row);
    }
    /* Wrong DENUM. */
    tb_append(&denum, header_block());
    {
        char row[256];
        snprintf(row, sizeof row, "442 2000.01.01 2451545.5 4 0 1 %.17g\n", 5.0 / au);
        tb_append(&denum, row);
    }
    /* Malformed six-token row: fused field without a JD tail. */
    tb_append(&malformed, header_block());
    {
        tb_append(&malformed, "441 -9999.01.01 4 12 1 1.25\n");
    }
    /* Nonexistent component: nutation X5. */
    tb_append(&badcomp, header_block());
    {
        char row[256];
        snprintf(row, sizeof row, "441 2000.01.01 2451545.5 14 0 5 %.17g\n", 0.0);
        tb_append(&badcomp, row);
    }
    /* Positive / negative numeric overflow in COORD. */
    tb_append(&opos, header_block());
    tb_append(&opos, "441 2000.01.01 2451545.5 4 0 1 1e999\n");
    tb_append(&oneg, header_block());
    tb_append(&oneg, "441 2000.01.01 2451545.5 4 0 1 -1e999\n");
    /* Trailing-EOT content stays ignored: T15 runners stop comparing at
     * the closing EOT; a valid row after it must not fail. */
    tb_append(&oneg, "EOT\n");
    tb_append(&oneg, "441 2000.01.01 2451545.5 4 0 1 1e-999\n");
    /* Overlong row: exceed the runner LINE_CAP (8192) so the overlong
     * branch, not the parser token limits, rejects it. */
    tb_append(&overlong, header_block());
    {
        static char big[8200];
        size_t k;
        for (k = 0; k < sizeof big - 2; ++k) {
            big[k] = ' ';
        }
        big[sizeof big - 2] = '\n';
        big[sizeof big - 1] = '\0';
        memcpy(big, "441 2000.01.01 2451545.5 4 0 1 1 extra-token", 43);
        tb_append(&overlong, big);
    }
    /* Far-epoch libration psi perturbation at an in-coverage epoch: the
     * mock carries 0.3 at X3 while the reference claims a 1e6-rad
     * offset (negative perturbation the corrected absolute tolerance
     * must reject; T03 documents the 1e-3 old-policy acceptance bound). */
    tb_append(&farpsi, header_block());
    {
        char row[256];
        snprintf(row, sizeof row, "441 2000.01.01 2451547.5 15 0 3 %.17g\n", 0.3 - 1e6);
        tb_append(&farpsi, row);
    }
    /* Finite wrong-sign TT-TDB value: mock reads +0.001 s, reference
     * claims -0.001 s, so the check must compare values, not finiteness. */
    tb_append(&ttdbwrong, header_block());
    {
        char row[256];
        snprintf(row, sizeof row, "441 2000.01.01 2451545.125 17 0 1 %.17g\n", -0.001);
        tb_append(&ttdbwrong, row);
    }

    if (good.failed || nan_rows.failed || missing.failed || angular.failed ||
        denum.failed || malformed.failed || badcomp.failed || opos.failed ||
        oneg.failed || overlong.failed || farpsi.failed || ttdbwrong.failed ||
        duponly.failed) {
        fprintf(stderr, "out of memory building validator data\n");
        goto done;
    }
#define WRITE(name, tb)                                                        \
    do {                                                                       \
        JOIN(path, name);                                                      \
        if (write_text(path, (tb).data ? (tb).data : "") != 0) {               \
            fprintf(stderr, "could not write %s\n", path);                     \
            goto done;                                                         \
        }                                                                      \
    } while (0)
    WRITE("good.testpo", good);
    WRITE("nan.testpo", nan_rows);
    WRITE("missing.testpo", missing);
    WRITE("angular.testpo", angular);
    WRITE("denum.testpo", denum);
    WRITE("malformed6.testpo", malformed);
    WRITE("badcomp.testpo", badcomp);
    WRITE("overflow_pos.testpo", opos);
    WRITE("overflow_neg.testpo", oneg);
    WRITE("overlong.testpo", overlong);
    WRITE("farpsi.testpo", farpsi);
    WRITE("ttdbwrong.testpo", ttdbwrong);
    WRITE("duplicate_only.testpo", duponly);
#undef WRITE
#undef JOIN

    rc = 0;
done:
    free(good.data);
    free(nan_rows.data);
    free(missing.data);
    free(angular.data);
    free(denum.data);
    free(malformed.data);
    free(badcomp.data);
    free(opos.data);
    free(oneg.data);
    free(overlong.data);
    free(farpsi.data);
    free(ttdbwrong.data);
    free(duponly.data);
    if (rc == 0) {
        printf("validator data written\n");
    }
    return rc;
}
