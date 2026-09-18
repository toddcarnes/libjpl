#include "jpl.h"
#include "jpl_test_util.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Always-on fixture validation for the committed 2000-2010 DE441 slice.
 *
 * Usage: test_fixture <eph-path> <testpo-slice-path>
 *
 * Opens tests/data/de441_2000_2010.eph (paths arrive as argv so CTest needs
 * no working-directory coupling), asserts fixture provenance (DENUM 441,
 * exact coverage bounds 2451536.5-2455216.5, no TT-TDB table), then
 * cross-checks every committed testpo slice line via jpl_get_state().
 *
 * CRLF handling: the slice opens with fopen "rb" and tokenizes on
 * isspace(), which treats '\r' as a separator, so the parse is identical
 * whether core.autocrlf checks the fixture out as LF or CRLF. No
 * .gitattributes pinning is used; tolerance lives in the reader, not in
 * checkout configuration. The .eph binary is byte-detected binary by git
 * and is never newline-converted.
 *
 * Parse body mirrors tests/test_testpo.c with the T#17 path removed (this
 * binary carries no TT-TDB table, so any T#17 line is a hard failure, not
 * a skip). Blank lines are skipped; every other non-data line increments
 * a skip counter that fails the run: out-of-coverage lines, unsupported
 * targets, and unparseable data-shaped lines all fail, because
 * parsed == tested == 121 proves the committed slice is byte-verbatim
 * what the manifest describes. Nutation rate lines
 * (T#14 X#3/4) map to state[3]/state[4] (dpsi/deps rates per day), while
 * X#1/2 map to state[0]/state[1] like every other body; T#14 X#5/6 do not
 * exist and fail. T#15 tolerances match test_testpo.c exactly.
 *
 * Uses explicit checks (fprintf + return), never assert().
 */

#define FIXTURE_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        if (file) fclose(file); \
        jpl_close(ephemeris); \
        return 1; \
    } \
} while (0)

static int parse_long(const char *text, long *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') return 0;
    *out = value;
    return 1;
}

static int parse_double(const char *text, double *out) {
    char *end = NULL;
    double value = strtod(text, &end);
    if (end == text || *end != '\0') return 0;
    *out = value;
    return 1;
}

/* Extracts the JED from a fused label such as "441-13200.09.01-3099998.5"
 * or "-9999.01.01-1931076.5": the JED starts at the last dash, which doubles
 * as its minus sign (strtod-from-dash semantics). Never passes NULL to
 * strtod: a missing dash or empty tail fails. */
static int parse_fused_jd(const char *label, double *out) {
    const char *dash = strrchr(label, '-');
    if (dash == NULL || dash[1] == '\0') return 0;
    return parse_double(dash, out);
}

int main(int argc, char **argv) {
    jpl_ephemeris *ephemeris = NULL;
    const jpl_header *header = NULL;
    FILE *file = NULL;
    char line[1024];
    size_t tested = 0, parsed = 0, skipped = 0;
    double first_jd = 0.0, last_jd = 0.0;
    int have_first = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <eph-path> <testpo-slice-path>\n", argc > 0 ? argv[0] : "test_fixture");
        return 2;
    }
    JPL_TEST_CHECK(jpl_open(argv[1], &ephemeris) == JPL_OK && ephemeris != NULL);
    header = jpl_get_header(ephemeris);
    FIXTURE_CHECK(header != NULL);
    FIXTURE_CHECK(header->ephemeris_number == 441);
    FIXTURE_CHECK(header->start_jd == 2451536.5 && header->end_jd == 2455216.5);
    FIXTURE_CHECK(jpl_has_tt_tdb(ephemeris) == 0);
    /* "rb": CRLF-tolerant (see file comment); isspace() splits off any '\r'. */
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
        char *tokens[8];
        int ntok = 0;
        char *p = line;
        double jd = 0.0, expected = 0.0, state[6], actual, tolerance;
        long target = 0, center = 0, component = 0;
        jpl_status status;
        int ok;

        while (*p != '\0') {
            while (isspace((unsigned char)*p)) ++p;
            if (*p == '\0') break;
            if (ntok >= 8) break;
            tokens[ntok++] = p;
            while (*p != '\0' && !isspace((unsigned char)*p)) ++p;
            if (*p != '\0') *p++ = '\0';
        }
        if (ntok != 5 && ntok != 6 && ntok != 7) { if (ntok != 0) ++skipped; continue; }

        if (ntok == 7) {
            if (strcmp(tokens[0], "441") != 0) {
                fprintf(stderr, "fixture slice line is not DE441: %s\n", tokens[0]);
                fclose(file);
                jpl_close(ephemeris);
                return 1;
            }
            ok = parse_double(tokens[2], &jd) && parse_long(tokens[3], &target) &&
                 parse_long(tokens[4], &center) && parse_long(tokens[5], &component) &&
                 parse_double(tokens[6], &expected);
        } else if (ntok == 6) {
            ok = parse_fused_jd(tokens[1], &jd) && parse_long(tokens[2], &target) &&
                 parse_long(tokens[3], &center) && parse_long(tokens[4], &component) &&
                 parse_double(tokens[5], &expected);
            /* Fused labels are always "441-..." (tokens end at whitespace,
             * so no space alternative exists); reject anything else. */
            if (ok && !(tokens[1][0] == '4' && tokens[1][1] == '4' && tokens[1][2] == '1' &&
                        tokens[1][3] == '-')) ok = 0;
        } else {
            ok = parse_fused_jd(tokens[0], &jd) && parse_long(tokens[1], &target) &&
                 parse_long(tokens[2], &center) && parse_long(tokens[3], &component) &&
                 parse_double(tokens[4], &expected);
            if (ok && !(tokens[0][0] == '4' && tokens[0][1] == '4' && tokens[0][2] == '1' &&
                        tokens[0][3] == '-')) ok = 0;
        }
        if (!ok) { ++skipped; continue; }
        ++parsed;

        if (target == 17) {
            fprintf(stderr, "fixture slice must be T#17-free (binary has no TT-TDB): JD %.1f\n", jd);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (target < 1 || target > 15 || center < 0 || center > 15 ||
            component < 1 || component > 6 || !isfinite(jd) || !isfinite(expected)) {
            fprintf(stderr, "unsupported fixture line: JD %.1f target %ld center %ld component %ld\n",
                    jd, target, center, component);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if ((target == 14 || target == 15) && center != 0) {
            fprintf(stderr, "angle target with nonzero center: JD %.1f target %ld center %ld\n",
                    jd, target, center);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (jd < header->start_jd || jd > header->end_jd) {
            fprintf(stderr, "fixture line outside committed coverage: JD %.1f\n", jd);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (target == 14 && (component == 5 || component == 6)) {
            fprintf(stderr, "nutation line with no such value: JD %.1f component %ld\n", jd, component);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        status = jpl_get_state(ephemeris, jd, (int)target, (int)center, state);
        if (status != JPL_OK) {
            fprintf(stderr, "jpl_get_state failed for JD %.1f target %ld center %ld: %s\n",
                    jd, target, center, jpl_status_string(status));
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (target == 14) {
            /* Nutations interpolate dpsi/deps plus their rates: X#1/2 are
             * state[0]/state[1], X#3/4 are the rates state[3]/state[4]. */
            actual = component <= 2 ? state[(size_t)component - 1u] : state[(size_t)component];
            tolerance = 5e-13 * fmax(1.0, fabs(expected));
        } else {
            actual = state[(size_t)component - 1u];
            /* Same tolerance rule as tests/test_testpo.c, including the
             * lunar-libration exceptions (targets 15 only). */
            tolerance = 5e-13 * fmax(1.0, fabs(expected));
            if (target == 15 && component == 3) {
                tolerance *= 1.0 + 100.0 * fabs(jd - 2440400.5) / 365.25;
            } else if (target == 15 && component >= 4) {
                tolerance = 5e-12 * fmax(1.0, fabs(expected));
            }
        }
        if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
            fprintf(stderr, "MISMATCH JD %.1f target %ld center %ld component %ld: got %.17g, expected %.17g (diff %.3g, tol %.3g)\n",
                    jd, target, center, component, actual, expected, fabs(actual - expected), tolerance);
            fclose(file);
            jpl_close(ephemeris);
            return 1;
        }
        if (!have_first) {
            first_jd = jd;
            have_first = 1;
        }
        last_jd = jd;
        ++tested;
    }

    fclose(file);
    file = NULL;
    FIXTURE_CHECK(skipped == 0);
    FIXTURE_CHECK(parsed == 121);
    FIXTURE_CHECK(tested == 121);
    FIXTURE_CHECK(have_first && first_jd == 2451544.5 && last_jd == 2455197.5);
    jpl_close(ephemeris);
    printf("fixture validated %zu lines\n", tested);
    return 0;
}
