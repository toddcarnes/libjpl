/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Strict full-file testpo validator (test-only, owned by T15).
 * Usage: test_testpo <eph-path> <testpo-path>
 * Replaces the lenient runner: shared T03 row parser/tolerance, explicit
 * header/EOT framing, DENUM validation, in-coverage errors always fail,
 * angular skips only on header-proven absence, and counted exclusions. */
#include "jpl.h"
#include "jpl_private.h"
#include "jpl_test_util.h"
#include "jpl_testpo_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_CAP 8192

/* EOT framing tolerates trailing spaces/tabs (e.g. testpo.405 pads its
 * EOT line) and CRLF, matching the whitespace-tolerant row grammar.
 * Anything else on the line is not an EOT marker. */
static int is_eot_line(const char *line) {
    const unsigned char *p = (const unsigned char *)line;
    if (p[0] != 'E' || p[1] != 'O' || p[2] != 'T') return 0;
    p += 3;
    for (;;) {
        if (*p == '\0' || *p == '\n') return 1;
        if (*p != ' ' && *p != '\t' && *p != '\r') return 0;
        ++p;
    }
}

/* Line-numbered diagnostic categories the CMake driver matches. */
static int fail_row(long lineno, const char *category, const char *detail,
                    FILE *file, jpl_ephemeris *ephemeris) {
    fprintf(stderr, "line %ld: %s: %s\n", lineno, category, detail);
    fclose(file);
    jpl_close(ephemeris);
    return 1;
}

static const char *status_name(jpl_status status) {
    return jpl_status_string(status);
}

int main(int argc, char **argv) {
    jpl_ephemeris *ephemeris = NULL;
    const jpl_header *header = NULL;
    FILE *file = NULL;
    static char line[LINE_CAP];
    long lineno = 0;
    size_t tested = 0, skipped_range = 0, skipped_absent = 0, skipped_t16 = 0;
    size_t count_body[18] = {0};
    size_t count_comp[8] = {0};
    /* F10 coverage matrix: successful rows by target and component. Ordinary
     * targets use components 1-6, nutation 1-4, libration 1-6, TT-TDB 1-2.
     * Angular rows never satisfy ordinary-body component requirements. */
    size_t body_component[18][7] = {{0}};
    int in_data = 0, saw_eot = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <eph-path> <testpo-path>\n",
                argc > 0 ? argv[0] : "test_testpo");
        return 2;
    }
    if (jpl_open(argv[1], &ephemeris) != JPL_OK || ephemeris == NULL) {
        fprintf(stderr, "could not open ephemeris %s\n", argv[1]);
        return 1;
    }
    header = jpl_get_header(ephemeris);
    if (header == NULL) {
        fputs("could not read ephemeris header\n", stderr);
        jpl_close(ephemeris);
        return 1;
    }
    file = fopen(argv[2], "rb");
    if (file == NULL) {
        fprintf(stderr, "could not open testpo file %s\n", argv[2]);
        jpl_close(ephemeris);
        return 1;
    }

    /* "rb" keeps the open mode CRLF-tolerant (the shared parser splits
     * off any '\r'). Header/EOT framing: full files require a header
     * transition before data rows; content after trailing EOT is ignored. */
    while (fgets(line, sizeof line, file) != NULL) {
        jpl_testpo_row row;
        int parsed;
        size_t linelen;
        char detail[256];
        int slot;
        double tolerance, actual = 0.0;
        jpl_status status;

        ++lineno;
        linelen = strlen(line);
        if (linelen > 0 && linelen == sizeof line - 1 && line[linelen - 1] != '\n') {
            int c;
            /* Overlong line: consume the tail, then fail with a category
             * the driver can match. */
            do {
                c = fgetc(file);
            } while (c != '\n' && c != EOF);
            return fail_row(lineno, "invalid row", "overlong line", file, ephemeris);
        }
    /* Header/EOT framing: full files require a recognized header
     * transition; arbitrary data-shaped malformed lines after EOT fail
     * via the strict row grammar below. Trailing EOT ends comparison;
     * content after it is ignored. */
        if (!in_data) {
            if (is_eot_line(line)) {
                in_data = 1;
                continue;
            }
            if (jpl_testpo_parse_row(line, &row) == 1) {
                return fail_row(lineno, "invalid row", "data row before EOT", file, ephemeris);
            }
            continue;
        }
        if (saw_eot) {
            continue;
        }
        {
            if (is_eot_line(line)) {
                saw_eot = 1;
                continue;
            }
        }
        parsed = jpl_testpo_parse_row(line, &row);
        if (parsed == 0) {
            /* Blank lines are tolerated in full-file mode. */
            continue;
        }
        if (parsed < 0) {
            return fail_row(lineno, "invalid row", "malformed data row", file, ephemeris);
        }
        /* DENUM validated before any coverage skip: a wrong-DE row is a
         * hard failure even when out of coverage. The shared parser
         * rejects T#16 rows as invalid (no header slot exists); the
         * explicit skipped_t16 branch below stays for a future
         * parser form that surfaces them as countable rows. */
        if (row.denum != header->ephemeris_number) {
            snprintf(detail, sizeof detail, "denum %d does not match ephemeris %d",
                     row.denum, header->ephemeris_number);
            return fail_row(lineno, "denum mismatch", detail, file, ephemeris);
        }
        if (row.target == 16) {
            ++skipped_t16;
            continue;
        }
        if (jpl_date_compare(row.jd, header->start_jd) < 0 ||
            jpl_date_compare(row.jd, header->end_jd) > 0) {
            ++skipped_range;
            continue;
        }
        if (row.target == 17) {
            double sec = 0.0, rate = 0.0, want;
            if (row.center != 0 || (row.component != 1 && row.component != 2)) {
                snprintf(detail, sizeof detail, "TT-TDB target 17 with center %d component %d",
                         row.center, row.component);
                return fail_row(lineno, "invalid row", detail, file, ephemeris);
            }
            if (!jpl_has_tt_tdb(ephemeris)) {
                snprintf(detail, sizeof detail, "TT-TDB row but binary has no TT-TDB dataset");
                return fail_row(lineno, "invalid row", detail, file, ephemeris);
            }
            status = jpl_get_tt_minus_tdb(ephemeris, row.jd, &sec, &rate);
            if (status != JPL_OK) {
                snprintf(detail, sizeof detail, "tt-tdb query failed: %s", status_name(status));
                return fail_row(lineno, "reference mismatch", detail, file, ephemeris);
            }
            want = row.component == 1 ? sec : rate;
            tolerance = jpl_testpo_tolerance(&row);
            if (want != want || row.expected != row.expected ||
                (want - row.expected > tolerance || row.expected - want > tolerance)) {
                snprintf(detail, sizeof detail,
                         "TT-TDB component %d: got %.17g, expected %.17g (tol %.3g)",
                         row.component, want, row.expected, tolerance);
                return fail_row(lineno, "reference mismatch", detail, file, ephemeris);
            }
            ++tested;
            if ((size_t)row.target < sizeof count_body / sizeof count_body[0]) {
                ++count_body[row.target];
            }
            if ((size_t)row.component < sizeof count_comp / sizeof count_comp[0]) {
                ++count_comp[row.component];
            }
            if ((size_t)row.target < 18 && (size_t)row.component < 7) {
                ++body_component[row.target][row.component];
            }
            continue;
        }
        if (row.target < 1 || row.target > 15 || row.center < 0 || row.center > 15 ||
            row.component < 1 || row.component > 6) {
            snprintf(detail, sizeof detail, "target %d center %d component %d",
                     row.target, row.center, row.component);
            return fail_row(lineno, "invalid row", detail, file, ephemeris);
        }
        if ((row.target == 14 || row.target == 15) && row.center != 0) {
            snprintf(detail, sizeof detail, "angle target %d with nonzero center %d",
                     row.target, row.center);
            return fail_row(lineno, "invalid row", detail, file, ephemeris);
        }
        slot = jpl_testpo_state_index(&row);
        if (slot < 0) {
            snprintf(detail, sizeof detail, "nutation target 14 has no component %d",
                     row.component);
            return fail_row(lineno, "invalid row", detail, file, ephemeris);
        }
        if (row.target == 14 || row.target == 15) {
            /* Angular datasets skip only after checking the absent header
             * pointer; any present dataset must return OK below. */
            const jpl_pointer *pointer =
                row.target == 14 ? &header->pointers[JPL_TARGET_COUNT - 1]
                                 : &header->libration_pointer;
            if (pointer->start_index <= 0 ||
                pointer->coefficients_per_component <= 0 ||
                pointer->subintervals <= 0) {
                ++skipped_absent;
                continue;
            }
        }
        {
            double state[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            status = jpl_get_state(ephemeris, row.jd, row.target, row.center, state);
            if (status != JPL_OK) {
                /* In-coverage body errors fail unconditionally: an
                 * UNSUPPORTED here means a missing ordinary body (or a
                 * boundary-unusable optional dataset), never a
                 * legitimate coverage skip (coverage was checked
                 * above). */
                snprintf(detail, sizeof detail,
                         "jpl_get_state failed for target %d center %d: %s",
                         row.target, row.center, status_name(status));
                return fail_row(lineno, "reference mismatch", detail, file, ephemeris);
            }
            actual = state[slot];
            tolerance = jpl_testpo_tolerance(&row);
            /* Body-only reference conversion (T08): public output is modern
             * AU while historical references are file AU. Convert the
             * actual value back into reference units so the reference
             * tolerance stays in its original units. Never rescale
             * angular rows (T#14/15) here; only body coordinates/rates. */
            if (row.target >= 1 && row.target <= 13) {
                actual = actual * (JPL_TEST_AU_KM / header->astronomical_unit_km);
            }
            if (actual != actual ||
                (actual - row.expected > tolerance || row.expected - actual > tolerance)) {
                snprintf(detail, sizeof detail,
                         "target %d center %d component %d: got %.17g, expected %.17g (tol %.3g)",
                         row.target, row.center, row.component,
                         actual, row.expected, tolerance);
                return fail_row(lineno, "reference mismatch", detail, file, ephemeris);
            }
            ++tested;
            if ((size_t)row.target < sizeof count_body / sizeof count_body[0]) {
                ++count_body[row.target];
            }
            if (row.component >= 1 &&
                (size_t)row.component < sizeof count_comp / sizeof count_comp[0]) {
                ++count_comp[row.component];
            }
            if ((size_t)row.target < 18 && row.component >= 1 && (size_t)row.component < 7) {
                ++body_component[row.target][row.component];
            }
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
    if (!in_data) {
        fprintf(stderr, "missing EOT header in %s\n", argv[2]);
        jpl_close(ephemeris);
        return 1;
    }
    /* F10 coverage requirements: the 100-row minimum is only a truncation
     * guard. Additionally require, for every dataset present in the binary
     * and represented by the supported validator contract, each valid
     * component at least once: at least one available ordinary body 1-13
     * with all six components; T14 X1-X4 when nutation metadata is present
     * AND the reference file contains at least one T14 row; T15 X1-X6 when
     * libration metadata is present AND the file contains at least one T15
     * row; T17 X1-X2 when TT-TDB is present AND the file contains at least
     * one T17 row. Absent optional datasets, or authoritative files that
     * legitimately carry no rows for a present dataset (e.g. testpo.200
     * has no T14 rows), create no requirement. Duplicate rows cannot
     * satisfy missing components. */
    {
        int ordinary_ok = 0, nut_ok = 1, lib_ok = 1, ttdb_ok = 1, t;
        const jpl_pointer *nut = &header->pointers[JPL_TARGET_COUNT - 1];
        int nut_present = nut->start_index > 0 && nut->coefficients_per_component > 0 && nut->subintervals > 0;
        int lib_present = header->libration_pointer.start_index > 0 &&
            header->libration_pointer.coefficients_per_component > 0 &&
            header->libration_pointer.subintervals > 0;
        for (t = 1; t <= 13; ++t) {
            const jpl_pointer *p = NULL;
            int c, present = 0, all = 1;
            if (t == JPL_EARTH) {
                p = &header->pointers[JPL_FILE_SLOT_EMB];
            } else if (t == JPL_MOON) {
                p = &header->pointers[JPL_MOON - 1];
            } else if (t >= JPL_MERCURY && t <= JPL_SUN) {
                p = &header->pointers[t - 1];
            } else if (t == JPL_SOLAR_SYSTEM_BARYCENTER || t == JPL_EARTH_MOON_BARYCENTER) {
                /* SSB/EMB need no file coefficients for coverage. */
                present = 1;
            }
            if (p != NULL) {
                present = p->start_index > 0 && p->coefficients_per_component > 0 && p->subintervals > 0;
            }
            if (!present) continue;
            for (c = 1; c <= 6; ++c) {
                if (body_component[t][c] == 0) { all = 0; break; }
            }
            if (all) ordinary_ok = 1;
        }
        if (nut_present) {
            int c;
            nut_ok = 1;
            if (count_body[14] == 0) {
                /* Authoritative file carries no nutation rows: no
                 * requirement (e.g. testpo.200). */
                nut_ok = 1;
            } else {
                for (c = 1; c <= 4; ++c) {
                    if (body_component[14][c] == 0) { nut_ok = 0; break; }
                }
            }
        }
        if (lib_present) {
            int c;
            lib_ok = 1;
            if (count_body[15] == 0) {
                lib_ok = 1;
            } else {
                for (c = 1; c <= 6; ++c) {
                    if (body_component[15][c] == 0) { lib_ok = 0; break; }
                }
            }
        }
        if (header->has_tt_tdb) {
            if (count_body[17] == 0) {
                ttdb_ok = 1;
            } else {
                ttdb_ok = body_component[17][1] > 0 && body_component[17][2] > 0;
            }
        }
        if (!ordinary_ok || !nut_ok || !lib_ok || !ttdb_ok) {
            fprintf(stderr, "insufficient reference coverage:");
            if (!ordinary_ok) fprintf(stderr, " no ordinary body 1-13 with X1-X6;");
            if (!nut_ok) {
                int c;
                for (c = 1; c <= 4; ++c) {
                    if (body_component[14][c] == 0) fprintf(stderr, " T14 X%d missing;", c);
                }
            }
            if (!lib_ok) {
                int c;
                for (c = 1; c <= 6; ++c) {
                    if (body_component[15][c] == 0) fprintf(stderr, " T15 X%d missing;", c);
                }
            }
            if (!ttdb_ok) {
                if (body_component[17][1] == 0) fprintf(stderr, " T17 X1 missing;");
                if (body_component[17][2] == 0) fprintf(stderr, " T17 X2 missing;");
            }
            fprintf(stderr, " (tested %zu)\n", tested);
            jpl_close(ephemeris);
            return 1;
        }
    }
    jpl_close(ephemeris);
    printf("testpo %s: validated %zu lines (%zu out of coverage, %zu absent angular, %zu T16 excluded)\n",
           argv[2], tested, skipped_range, skipped_absent, skipped_t16);
    printf("by body:");
    {
        int b;
        for (b = 1; b <= 17; ++b) {
            if (count_body[b] > 0) {
                printf(" T%d=%zu", b, count_body[b]);
            }
        }
        printf("\nby component:");
        {
            int c;
            for (c = 1; c <= 6; ++c) {
                if (count_comp[c] > 0) {
                    printf(" X%d=%zu", c, count_comp[c]);
                }
            }
        }
        printf("\n");
    }
    if (tested < 100) {
        fprintf(stderr, "too few testpo lines validated: %zu\n", tested);
        return 1;
    }
    return 0;
}
