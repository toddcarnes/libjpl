/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_testpo_util.h"

#include "jpl_private.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Test-only row parser/span limits. Grossly oversized lines are invalid;
 * legitimate testpo lines stay under 100 bytes (observed maximum 97). */
#define TESTPO_MAX_LINE 4095u
#define TESTPO_MAX_TOKENS 7u
#define TESTPO_MAX_TOKEN 127u
#define TESTPO_EPOCH_JD0 2440400.5

/* Parse a bounded NUL-terminated integer token with long range checking.
 * Rejects empty input, trailing characters, and values outside [lo, hi]. */
static int parse_int_range(const char *text, long lo, long hi, long *out) {
    char *end = NULL;
    long value;

    if (text == NULL || *text == '\0') {
        return 0;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return 0;
    }
    if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE) {
        return 0;
    }
    if (value < lo || value > hi) {
        return 0;
    }
    *out = value;
    return 1;
}

/* Whether a token is all ASCII decimal digits (at least one). */
static int all_digits(const char *text) {
    size_t i;

    if (text[0] == '\0') {
        return 0;
    }
    for (i = 0; text[i] != '\0'; ++i) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    return 1;
}

/* Parse the standalone DENUM token. Accepts a trailing 't'/'T' label
 * suffix (time-segmented files); the numeric part stays the key. */
static int parse_denum(const char *text, size_t prefix_len, long *out) {
    char digits[TESTPO_MAX_TOKEN + 1u];
    size_t len = strlen(text);
    size_t n = len;

    if (prefix_len > 0u) {
        if (len < prefix_len) {
            return 0;
        }
        n = prefix_len;
    }
    if (n == 0u || n >= sizeof digits) {
        return 0;
    }
    /* A fused prefix must be followed by the calendar separator. */
    if (prefix_len > 0u && text[n] != '-') {
        return 0;
    }
    memcpy(digits, text, n);
    digits[n] = '\0';
    /* Optional time-segment label suffix, e.g. "440t". */
    if (n > 1u && (digits[n - 1u] == 't' || digits[n - 1u] == 'T')) {
        digits[n - 1u] = '\0';
    }
    if (!all_digits(digits)) {
        return 0;
    }
    return parse_int_range(digits, 1, INT_MAX, out);
}

/* Split a decimal fixed-point JD token into integer/fraction digit runs.
 * Grammar: [sign] digits [. digits]; at least one digit overall, no
 * exponents, no whitespace, no other characters. Reports the sign, the
 * integer digits (no sign, no leading-plus), and the fraction digits
 * (no dot). Returns 0 when the grammar does not match. */
static int split_jd_token(const char *text, int *negative, const char **int_digits,
                          const char **frac_digits, size_t *int_len, size_t *frac_len) {
    const char *p = text;
    const char *dot;
    size_t head_len, tail_len, i;

    if (p[0] == '+' || p[0] == '-') {
        *negative = (p[0] == '-');
        ++p;
    } else {
        *negative = 0;
    }
    dot = strchr(p, '.');
    if (dot == NULL) {
        head_len = strlen(p);
        tail_len = 0;
    } else {
        /* A second dot is not fixed-point notation. */
        if (strchr(dot + 1, '.') != NULL) {
            return 0;
        }
        head_len = (size_t)(dot - p);
        tail_len = strlen(dot + 1);
    }
    if (head_len + tail_len == 0u) {
        return 0;
    }
    for (i = 0; i < head_len; ++i) {
        if (!isdigit((unsigned char)p[i])) {
            return 0;
        }
    }
    for (i = 0; i < tail_len; ++i) {
        if (!isdigit((unsigned char)dot[1 + i])) {
            return 0;
        }
    }
    *int_digits = p;
    *frac_digits = (dot == NULL) ? p + head_len : dot + 1;
    *int_len = head_len;
    *frac_len = tail_len;
    return 1;
}

/* Convert a digit run (no sign, bounded length) to double with overflow
 * detection. Accumulates in integer arithmetic so ERANGE/strtod rounding
 * cannot silently alter the value; fails when the run overflows double. */
static int digits_to_double(const char *digits, size_t len, double *out) {
    double value = 0.0;
    size_t i;

    for (i = 0; i < len; ++i) {
        value = value * 10.0 + (double)(digits[i] - '0');
        if (!isfinite(value)) {
            return 0;
        }
    }
    *out = value;
    return 1;
}

/* Parse a fixed-point JD token into a normalized two-part date without
 * rounding the complete epoch through strtod. Handles the sign, integer
 * digits, and fractional digits separately: the sign applies to both
 * parts, so "-0.5" becomes {-0.5, -0.0 normalized} rather than a positive
 * residual. Rejects values whose integer digits already overflow double
 * and fractions that vanish below binary64 resolution is preserved via
 * the fraction-scaling path (no underflow failure: a zero residual is
 * still the correctly rounded pair). */
static int parse_jd_two_part(const char *text, double jd[2]) {
    int negative = 0;
    const char *int_digits = NULL, *frac_digits = NULL;
    size_t int_len = 0, frac_len = 0, i;
    double whole = 0.0, frac = 0.0, pair[2], norm[2];
    double scale = 1.0;

    if (!split_jd_token(text, &negative, &int_digits, &frac_digits, &int_len, &frac_len)) {
        return 0;
    }
    if (int_len > 0u && !digits_to_double(int_digits, int_len, &whole)) {
        return 0;
    }
    if (frac_len > 0u) {
        double digits = 0.0;
        if (!digits_to_double(frac_digits, frac_len, &digits)) {
            return 0;
        }
        for (i = 0; i < frac_len; ++i) {
            scale *= 10.0;
            if (!isfinite(scale)) {
                return 0;
            }
        }
        frac = digits / scale;
        /* A nonzero decimal fraction that rounds to zero is still an
         * inexact conversion path; keep the (signed) zero residual so the
         * pair normalizes honestly instead of failing the row. */
        if (!isfinite(frac)) {
            return 0;
        }
    }
    if (negative) {
        whole = -whole;
        /* Apply the sign to the fraction too, including "-0.5": without
         * this the residual would carry the wrong sign for negative
         * fractional dates. Signed zero normalizes away below. */
        frac = (frac == 0.0) ? 0.0 : -frac;
    }
    pair[0] = whole;
    pair[1] = frac;
    /* ERANGE-style guard: an integer part that strtod would clamp to
     * HUGE_VAL must fail here as well (digits_to_double already rejects
     * infinite accumulations; re-check the finite sum). */
    if (!isfinite(pair[0]) || !isfinite(pair[1])) {
        return 0;
    }
    if (jpl_date_normalize(pair, norm) != JPL_OK) {
        return 0;
    }
    jd[0] = norm[0];
    jd[1] = norm[1];
    return 1;
}

/* Split the JD tail out of a fused calendar/JD token. The accepted grammar
 * is decimal fixed-point, so an exponent minus (e-/E-/d-/D-) must not be
 * mistaken for a date separator: scan for the last '-' that is NOT part of
 * an exponent marker. The JD tail itself must then match fixed-point
 * grammar (parse_jd_two_part rejects exponents explicitly). */
static int split_fused_tail(const char *fused, const char **tail_out) {
    const char *sep = NULL, *p = fused;

    for (; *p != '\0'; ++p) {
        if (*p == '-') {
            /* A '-' directly following an exponent letter belongs to the
             * exponent, not to the calendar/JD separator. */
            if (p != fused && (p[-1] == 'e' || p[-1] == 'E' || p[-1] == 'd' || p[-1] == 'D')) {
                continue;
            }
            sep = p;
        }
    }
    if (sep == NULL || sep[1] == '\0') {
        return 0;
    }
    *tail_out = sep;
    return 1;
}

/* Extract the DENUM portion out of a fused calendar/JD token. The DENUM is
 * the leading digit run (with an optional 't'/'T' time-segment label
 * suffix), terminated by the calendar separator '-'. The calendar portion
 * that follows is not part of the DENUM. */
static int parse_fused_denum(const char *fused, long *out) {
    size_t i = 0;
    char digits[TESTPO_MAX_TOKEN + 1u];

    while (isdigit((unsigned char)fused[i])) {
        ++i;
    }
    if (i == 0u || i >= sizeof digits) {
        return 0;
    }
    memcpy(digits, fused, i);
    digits[i] = '\0';
    if (fused[i] == 't' || fused[i] == 'T') {
        /* Time-segment label suffix: numeric part stays the key. */
        ++i;
    }
    /* The DENUM run must end at the calendar separator. */
    if (fused[i] != '-') {
        return 0;
    }
    return parse_int_range(digits, 1, INT_MAX, out);
}

/* Extract the JD tail of a six-token fused calendar/JD field (token one)
 * or a five-token fused DENUM/calendar/JD field (token zero), parsing it
 * with the fixed-point JD grammar. Also validates the DENUM portion via
 * parse_denum so hard-coded "441-" prefixes never appear here. */
static int parse_fused_jd_field(const char *fused, int fused_denum, long *denum_out,
                                double jd[2]) {
    const char *tail = NULL;
    long denum = 0;

    if (!split_fused_tail(fused, &tail)) {
        return 0;
    }
    /* The JD tail doubles as its own minus sign; a bare "-" fails. */
    if (!parse_jd_two_part(tail, jd)) {
        return 0;
    }
    if (fused_denum) {
        if (!parse_fused_denum(fused, &denum)) {
            return 0;
        }
        *denum_out = denum;
    }
    return 1;
}

/* Checked conversion of the COORD field: complete strtod conversion with
 * ERANGE failure when it changes the value, and nonfinite rejection. */
static int parse_expected(const char *text, double *out) {
    char *end = NULL;
    double value;

    if (text == NULL || *text == '\0') {
        return 0;
    }
    errno = 0;
    value = strtod(text, &end);
    if (end == text || *end != '\0') {
        return 0;
    }
    if (!isfinite(value)) {
        return 0;
    }
    if (errno == ERANGE && (value == 0.0 || fabs(value) == HUGE_VAL)) {
        return 0;
    }
    *out = value;
    return 1;
}

int jpl_testpo_parse_row(const char *line, jpl_testpo_row *out) {
    char tokens[TESTPO_MAX_TOKENS][TESTPO_MAX_TOKEN + 1u];
    size_t ntok = 0, pos = 0, len;
    jpl_testpo_row row;
    long denum = 0, target = 0, center = 0, component = 0;
    double jd[2] = {0.0, 0.0}, expected = 0.0;
    int ok = 0;

    if (line == NULL || out == NULL) {
        return -1;
    }
    len = strlen(line);
    if (len > TESTPO_MAX_LINE) {
        return -1;
    }
    /* Tokenize into bounded local storage; more than seven whitespace
     * separated fields is invalid (headers such as the 13-token span line
     * and the 10-token title line never validate as data). */
    while (pos < len) {
        size_t tlen = 0;
        while (pos < len && isspace((unsigned char)line[pos])) {
            ++pos;
        }
        if (pos >= len) {
            break;
        }
        if (ntok >= TESTPO_MAX_TOKENS) {
            return -1;
        }
        /* A trailing CR is whitespace and already skipped above, so no
         * special CRLF handling is needed beyond isspace splitting. */
        while (pos < len && !isspace((unsigned char)line[pos])) {
            if (tlen >= TESTPO_MAX_TOKEN) {
                return -1;
            }
            tokens[ntok][tlen++] = line[pos++];
        }
        tokens[ntok][tlen] = '\0';
        ++ntok;
    }
    if (ntok == 0u) {
        return 0;
    }
    if (ntok != 5u && ntok != 6u && ntok != 7u) {
        return -1;
    }

    if (ntok == 7u) {
        /* Seven tokens: DE DATE JD T# C# X# COORD. Tokens[1] is the
         * calendar label (unchecked grammar); token[2] is fixed-point JD. */
        if (!parse_denum(tokens[0], 0u, &denum)) {
            return -1;
        }
        if (!parse_jd_two_part(tokens[2], jd)) {
            return -1;
        }
        ok = parse_int_range(tokens[3], INT_MIN, INT_MAX, &target) &&
             parse_int_range(tokens[4], INT_MIN, INT_MAX, &center) &&
             parse_int_range(tokens[5], INT_MIN, INT_MAX, &component) &&
             parse_expected(tokens[6], &expected);
    } else if (ntok == 6u) {
        /* Six tokens: DE fused-DATE-JD T# C# X# COORD. DENUM is token
         * zero; the fused calendar/JD field is token one. */
        if (!parse_denum(tokens[0], 0u, &denum)) {
            return -1;
        }
        if (!parse_fused_jd_field(tokens[1], 0, NULL, jd)) {
            return -1;
        }
        ok = parse_int_range(tokens[2], INT_MIN, INT_MAX, &target) &&
             parse_int_range(tokens[3], INT_MIN, INT_MAX, &center) &&
             parse_int_range(tokens[4], INT_MIN, INT_MAX, &component) &&
             parse_expected(tokens[5], &expected);
    } else {
        /* Five tokens: fused DE-DATE-JD T# C# X# COORD. */
        if (!parse_fused_jd_field(tokens[0], 1, &denum, jd)) {
            return -1;
        }
        ok = parse_int_range(tokens[1], INT_MIN, INT_MAX, &target) &&
             parse_int_range(tokens[2], INT_MIN, INT_MAX, &center) &&
             parse_int_range(tokens[3], INT_MIN, INT_MAX, &component) &&
             parse_expected(tokens[4], &expected);
    }
    if (!ok) {
        return -1;
    }
    /* Range gate: body/component integers must fit their dispatch ranges
     * before the row validates; huge values fail rather than truncate. */
    if (target < 1 || target > 17 || center < 0 || center > 15 ||
        component < 1 || component > 6) {
        return -1;
    }
    if (target == 16) {
        return -1;
    }
    if (!isfinite(jd[0]) || !isfinite(jd[1]) || !isfinite(expected)) {
        return -1;
    }
    row.denum = (int)denum;
    row.target = (int)target;
    row.center = (int)center;
    row.component = (int)component;
    row.jd[0] = jd[0];
    row.jd[1] = jd[1];
    row.expected = expected;
    *out = row;
    return 1;
}

int jpl_testpo_state_index(const jpl_testpo_row *r) {
    static const int nutation[4] = {0, 1, 3, 4};

    if (r == NULL) {
        return -1;
    }
    if (r->target == 14) {
        return r->component >= 1 && r->component <= 4 ? nutation[r->component - 1] : -1;
    }
    if (r->target == 17) {
        return -1;
    }
    return r->target >= 1 && r->target <= 15 && r->component >= 1 && r->component <= 6
        ? r->component - 1
        : -1;
}

double jpl_testpo_tolerance(const jpl_testpo_row *row) {
    static const double epoch[2] = {TESTPO_EPOCH_JD0, 0.0};
    double delta[2] = {0.0, 0.0};
    double days = 0.0;

    if (row == NULL) {
        return 5e-13;
    }
    if (row->target == 17) {
        return 1e-12;
    }
    if (row->target == 15 && row->component >= 4 && row->component <= 6) {
        return 5e-12;
    }
    if (row->target == 15 && row->component == 3) {
        /* JPL's psi epoch factor only; no angle-magnitude scaling. The
         * offset goes through the compensated date helpers so far-epoch
         * rows keep full two-part precision. */
        if (jpl_date_subtract(row->jd, epoch, delta) != JPL_OK) {
            return 5e-13;
        }
        days = fabs(delta[0] + delta[1]);
        return 5e-13 * (1.0 + 100.0 * days / 365.25);
    }
    return 5e-13;
}
