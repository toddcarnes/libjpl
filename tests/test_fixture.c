/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Always-on fixture validation for the committed 2000-2010 DE441 slice.
 * Usage: test_fixture <eph-path> <testpo-slice-path> <identity-list-path>
 * Strict data-only mode: shared T03 row parser/tolerance, seven/six/five
 * token coverage, DENUM and target/center/component validation, in-coverage
 * errors fail, angular skips only on header-proven absence, nutation X1-4
 * through state indices 0,1,3,4, explicit read/close error checks, and an
 * ordered interior-identity comparison against tests/data/fixture_rows.txt
 * (all 121 decimal-JD/target/center/component rows, not just endpoints).
 * Data integrity (hashes) is established by tests/cmake/check_fixture.cmake
 * against tests/data/MANIFEST.txt; this runner establishes numerical
 * identity. Neither count/endpoints alone nor a bare hash proves upstream
 * provenance -- see tests/data/PROVENANCE.md for the source/tool evidence
 * and its outstanding gaps. */
#include "jpl.h"
#include "jpl_private.h"
#include "jpl_test_util.h"
#include "jpl_testpo_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_CAP 8192

#define FIXTURE_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        if (file) fclose(file); \
        jpl_close(ephemeris); \
        return 1; \
    } \
} while (0)

int main(int argc, char **argv) {
    jpl_ephemeris *ephemeris = NULL;
    const jpl_header *header = NULL;
    FILE *file = NULL;
    static char line[LINE_CAP];
    size_t tested = 0, parsed = 0, skipped_absent = 0, skipped_t16 = 0;
    size_t count_body[18] = {0};
    size_t count_comp[8] = {0};
    double first_jd[2] = {0.0, 0.0}, last_jd[2] = {0.0, 0.0};
    /* Ordered interior-identity capture (G08): every parsed row's decimal
     * JD text plus target/center/component is recorded here in file order
     * so the committed identity list can be compared against all rows,
     * not just endpoints or the total count. */
    char identity_jd[121][32];
    int identity_target[121], identity_center[121], identity_component[121];
    int have_first = 0;
    long lineno = 0;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <eph-path> <testpo-slice-path> <identity-list-path>\n", argc > 0 ? argv[0] : "test_fixture");
        return 2;
    }
    JPL_TEST_CHECK(jpl_open(argv[1], &ephemeris) == JPL_OK && ephemeris != NULL);
    header = jpl_get_header(ephemeris);
    FIXTURE_CHECK(header != NULL);
    FIXTURE_CHECK(header->ephemeris_number == 441);
    /* Coverage fields are normalized pairs now; compare via the helper. */
    FIXTURE_CHECK(jpl_date_compare(header->start_jd, (const double[2]){2451536.5, 0.0}) == 0 &&
                  jpl_date_compare(header->end_jd, (const double[2]){2455216.5, 0.0}) == 0);
    FIXTURE_CHECK(jpl_has_tt_tdb(ephemeris) == 0);
    /* "rb": CRLF-tolerant (isspace() splits off any '\r'); data-only mode
     * tolerates only blank lines besides data rows. */
    file = fopen(argv[2], "rb");
    FIXTURE_CHECK(file != NULL);
    { FILE *sz = fopen(argv[1], "rb");
      long endpos;
      int szok;
      FIXTURE_CHECK(sz != NULL);
      szok = fseek(sz, 0, SEEK_END);
      endpos = szok == 0 ? ftell(sz) : -1L;
      fclose(sz);
      FIXTURE_CHECK(szok == 0);
      FIXTURE_CHECK(endpos == 952848L); }

    while (fgets(line, sizeof line, file) != NULL) {
        jpl_testpo_row row;
        int parsed_row;
        size_t linelen = strlen(line);
        double state[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double actual, tolerance;
        int slot;
        jpl_status status;

        ++lineno;
        if (linelen > 0 && linelen == sizeof line - 1 && line[linelen - 1] != '\n') {
            int c;
            do {
                c = fgetc(file);
            } while (c != '\n' && c != EOF);
            fprintf(stderr, "line %ld: invalid row: overlong line\n", lineno);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        parsed_row = jpl_testpo_parse_row(line, &row);
        if (parsed_row == 0) {
            continue;
        }
        if (parsed_row < 0) {
            fprintf(stderr, "line %ld: invalid row: malformed data row\n", lineno);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        ++parsed;
        /* Capture the row identity before any dispatch: the ordered
         * identity list is compared against every parsed row below, so
         * interior duplicates/replacements fail even when the count and
         * endpoints are preserved. The decimal JD text is taken from the
         * raw line's third whitespace field (all slice rows are the
         * seven-token form); body codes come from the parsed row. */
        if (parsed <= 121) {
            const char *p = line;
            int field = 0;
            const char *start = NULL;
            size_t k, len = 0;
            while (*p != '\0' && field < 3) {
                while (*p != '\0' && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
                    ++p;
                }
                if (*p == '\0') {
                    break;
                }
                if (field == 2) {
                    start = p;
                }
                while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                    ++p;
                }
                if (field == 2) {
                    len = (size_t)(p - start);
                }
                ++field;
            }
            if (start == NULL || len == 0 || len >= sizeof identity_jd[0]) {
                fprintf(stderr, "line %ld: cannot capture row identity\n", lineno);
                fclose(file);
                jpl_close(ephemeris);
                return 1;
            }
            for (k = 0; k < len; ++k) {
                identity_jd[parsed - 1][k] = start[k];
            }
            identity_jd[parsed - 1][len] = '\0';
            identity_target[parsed - 1] = row.target;
            identity_center[parsed - 1] = row.center;
            identity_component[parsed - 1] = row.component;
        }
        if (row.denum != header->ephemeris_number) {
            fprintf(stderr, "line %ld: denum mismatch: %d vs %d\n",
                    lineno, row.denum, header->ephemeris_number);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        /* Counted, explicit T#16 exclusion (no header slot exists; the
         * current shared parser surfaces T#16 as an invalid row, so
         * this branch counts a future parser form that returns them). */
        if (row.target == 16) {
            ++skipped_t16;
            continue;
        }
        if (row.target == 17) {
            double sec = 0.0, rate = 0.0;
            jpl_status status17;
            if (row.center != 0 || (row.component != 1 && row.component != 2)) {
                fprintf(stderr, "line %ld: invalid row: TT-TDB target 17 with center %d component %d\n",
                        lineno, row.center, row.component);
                fclose(file);
                jpl_close(ephemeris);
                return 1;
            }
            /* Fixture binaries carry no TT-TDB dataset: T#17 rows always
             * fail here rather than silently passing. */
            status17 = jpl_get_tt_minus_tdb(ephemeris, row.jd, &sec, &rate);
            (void)sec;
            (void)rate;
            fprintf(stderr, "line %ld: fixture slice must be T#17-free (binary has no TT-TDB, query %s)\n",
                    lineno, jpl_status_string(status17));
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (row.target < 1 || row.target > 15 || row.center < 0 || row.center > 15 ||
            row.component < 1 || row.component > 6) {
            fprintf(stderr, "line %ld: invalid row: target %d center %d component %d\n",
                    lineno, row.target, row.center, row.component);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if ((row.target == 14 || row.target == 15) && row.center != 0) {
            fprintf(stderr, "line %ld: angle target %d with nonzero center %d\n",
                    lineno, row.target, row.center);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (jpl_date_compare(row.jd, header->start_jd) < 0 ||
            jpl_date_compare(row.jd, header->end_jd) > 0) {
            fprintf(stderr, "line %ld: fixture line outside committed coverage\n", lineno);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        slot = jpl_testpo_state_index(&row);
        if (slot < 0) {
            fprintf(stderr, "line %ld: nutation line with no such value: component %d\n",
                    lineno, row.component);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (row.target == 14 || row.target == 15) {
            const jpl_pointer *pointer =
                row.target == 14 ? &header->pointers[JPL_TARGET_COUNT - 1]
                                 : &header->libration_pointer;
            if (pointer->start_index <= 0 ||
                pointer->coefficients_per_component <= 0 ||
                pointer->subintervals <= 0) {
                /* Header-proven absence skips (consistent with
                 * test_testpo.c); a present dataset must return OK
                 * from jpl_get_state below. */
                ++skipped_absent;
                continue;
            }
        }
        status = jpl_get_state(ephemeris, row.jd, row.target, row.center, state);
        if (status != JPL_OK) {
            fprintf(stderr, "line %ld: jpl_get_state failed for target %d center %d: %s\n",
                    lineno, row.target, row.center, jpl_status_string(status));
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        actual = state[slot];
        tolerance = jpl_testpo_tolerance(&row);
        /* Body-only reference conversion (T08): public output is modern AU
         * while historical references are file AU. Convert the actual value
         * back into reference units; never rescale angular rows (T#14/15). */
        if (row.target >= 1 && row.target <= 13) {
            actual = actual * (JPL_TEST_AU_KM / header->astronomical_unit_km);
        }
        if (actual != actual ||
            (actual - row.expected > tolerance || row.expected - actual > tolerance)) {
            fprintf(stderr, "line %ld: MISMATCH target %d center %d component %d: got %.17g, expected %.17g (diff %.3g, tol %.3g)\n",
                    lineno, row.target, row.center, row.component,
                    actual, row.expected, actual - row.expected, tolerance);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (!have_first) {
            first_jd[0] = row.jd[0];
            first_jd[1] = row.jd[1];
            have_first = 1;
        }
        last_jd[0] = row.jd[0];
        last_jd[1] = row.jd[1];
        ++tested;
        if ((size_t)row.target < sizeof count_body / sizeof count_body[0]) {
            ++count_body[row.target];
        }
        if ((size_t)row.component < sizeof count_comp / sizeof count_comp[0]) {
            ++count_comp[row.component];
        }
    }

    {
        int read_failed = ferror(file);
#if defined(JPL_TEST_READER_FAULT)
        /* Test-only fault injection: force the post-read branch so the
         * negative read-error case exercises it portably. */
        if (!read_failed) {
            read_failed = 1;
        }
#endif
        if (read_failed) {
            fprintf(stderr, "reference input read error\n");
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
    }
    fclose(file);
    file = NULL;
    FIXTURE_CHECK(skipped_absent == 0);
    FIXTURE_CHECK(skipped_t16 == 0);
    FIXTURE_CHECK(parsed == 121);
    FIXTURE_CHECK(tested == 121);
    FIXTURE_CHECK(have_first && jpl_date_compare(first_jd, (const double[2]){2451544.5, 0.0}) == 0 &&
                  jpl_date_compare(last_jd, (const double[2]){2455197.5, 0.0}) == 0);
    /* Interior identity check (G08): count and endpoint dates alone cannot
     * detect duplicated or replaced interior rows, so every parsed row's
     * decimal-JD/target/center/component identity must match the committed
     * ordered identity list in tests/data/fixture_rows.txt row for row.
     * The list's hash is pinned in the manifest and verified by
     * tests/cmake/check_fixture.cmake; this comparison binds the numerical
     * run to the same approved sequence. Uniqueness (121 distinct
     * identities) is implied by equality with the pinned list, whose own
     * hash pins its exact contents. Corrects the old comment that
     * parsed == tested == 121 alone proves byte-verbatim provenance: it
     * does not; the ordered identity plus hash checks below do. */
    {
        FILE *rows = fopen(argv[3], "rb");
        static char expected[256];
        size_t index = 0;
        FIXTURE_CHECK(rows != NULL);
        while (fgets(expected, sizeof expected, rows) != NULL) {
            char jd_text[32] = {0}, t_text[32] = {0}, c_text[32] = {0}, x_text[32] = {0};
            long t = 0, c = 0, x = 0;
            char *end = NULL;
            size_t linelen = strlen(expected);
            if (linelen > 0 && linelen == sizeof expected - 1 && expected[linelen - 1] != '\n') {
                int ch;
                do {
                    ch = fgetc(rows);
                } while (ch != '\n' && ch != EOF);
                fprintf(stderr, "identity list: overlong line %zu\n", index + 1);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            if (sscanf(expected, "%31s %31s %31s %31s", jd_text, t_text, c_text, x_text) != 4) {
                size_t k;
                int blank = 1;
                for (k = 0; k < linelen; ++k) {
                    if (expected[k] != ' ' && expected[k] != '\t' &&
                        expected[k] != '\r' && expected[k] != '\n') {
                        blank = 0;
                        break;
                    }
                }
                if (blank) {
                    fprintf(stderr, "identity list line %zu: blank line\n", index + 1);
                    fclose(rows);
                    jpl_close(ephemeris);
                    return 1;
                }
                fprintf(stderr, "identity list line %zu: malformed\n", index + 1);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            if (index >= parsed) {
                fprintf(stderr, "identity list line %zu: more identities than parsed rows (%zu)\n",
                        index + 1, parsed);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            t = strtol(t_text, &end, 10);
            if (end == t_text || *end != '\0') {
                fprintf(stderr, "identity list line %zu: invalid target\n", index + 1);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            c = strtol(c_text, &end, 10);
            if (end == c_text || *end != '\0') {
                fprintf(stderr, "identity list line %zu: invalid center\n", index + 1);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            x = strtol(x_text, &end, 10);
            if (end == x_text || *end != '\0') {
                fprintf(stderr, "identity list line %zu: invalid component\n", index + 1);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            if (strcmp(identity_jd[index], jd_text) != 0 ||
                identity_target[index] != (int)t ||
                identity_center[index] != (int)c ||
                identity_component[index] != (int)x) {
                fprintf(stderr, "identity mismatch row %zu: slice %s %d %d %d vs list %s %ld %ld %ld\n",
                        index + 1, identity_jd[index], identity_target[index],
                        identity_center[index], identity_component[index],
                        jd_text, t, c, x);
                fclose(rows);
                jpl_close(ephemeris);
                return 1;
            }
            ++index;
        }
        if (ferror(rows)) {
            fprintf(stderr, "identity list read error\n");
            fclose(rows);
            jpl_close(ephemeris);
            return 1;
        }
        fclose(rows);
        FIXTURE_CHECK(index == 121);
        FIXTURE_CHECK(index == parsed);
        FIXTURE_CHECK(index == tested);
    }
    jpl_close(ephemeris);
    printf("fixture validated %zu lines (by body:", tested);
    {
        int b;
        for (b = 1; b <= 17; ++b) {
            if (count_body[b] > 0) {
                printf(" T%d=%zu", b, count_body[b]);
            }
        }
        printf("; by component:");
        {
            int c;
            for (c = 1; c <= 6; ++c) {
                if (count_comp[c] > 0) {
                    printf(" X%d=%zu", c, count_comp[c]);
                }
            }
        }
        printf(")\n");
    }
    return 0;
}
