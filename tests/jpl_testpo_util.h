/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Shared checked parser for JPL testpo reference rows (test-only).
 * Test-only helper: parses the seven-token, six-token, and five-token data
 * layouts into two-part Julian dates, maps components to state slots, and
 * exposes the single comparison-tolerance policy shared by the fixture and
 * full-file validators. */
#ifndef LIBJPL_TESTPO_UTIL_H
#define LIBJPL_TESTPO_UTIL_H

/** Parsed testpo data row. jd is a normalized two-part Julian date. */
typedef struct jpl_testpo_row {
    int denum;      /**< DE number from the DENUM field or fused prefix. */
    int target;     /**< T# body code (1-17, including TT-TDB 17). */
    int center;     /**< C# center code. */
    int component;  /**< X# 1-based component index. */
    double jd[2];   /**< Normalized two-part Julian date (TDB). */
    double expected; /**< Reference coordinate in file units. */
} jpl_testpo_row;

/**
 * @brief Parse one testpo line into a data row.
 *
 * Accepts the seven-token (`441 DATE JD T# C# X# COORD`), six-token
 * (`441 DATE-JD T# C# X# COORD`), and five-token (`441DATE-JD T# C# X# COORD`)
 * forms. Returns 1 for a valid data row, 0 for a blank line, and -1 for an
 * invalid data row. Output is unchanged when -1 is returned.
 *
 * @param line NUL-terminated input line (may carry a trailing CR).
 * @param out Destination row, updated only on success.
 * @return 1 (valid row), 0 (blank line), or -1 (invalid row).
 */
int jpl_testpo_parse_row(const char *line, jpl_testpo_row *out);

/**
 * @brief Map a parsed row to its state-vector slot.
 *
 * Ordinary bodies and librations map X#1-6 to slots 0-5; nutations
 * (T#14) map X#1/2 to slots 0/1 and X#3/4 (rates) to slots 3/4.
 * T#17 and anything else map to -1.
 *
 * @param row Valid parsed row.
 * @return State slot 0-5, or -1 for an unsupported combination.
 */
int jpl_testpo_state_index(const jpl_testpo_row *row);

/**
 * @brief Comparison tolerance for a parsed row, in reference units.
 *
 * Absolute 5e-13, except T#15 X#3 (psi) which also carries JPL's epoch
 * factor `1 + 100*|JD - 2440400.5|/365.25`, T#15 X#4-6 which use 5e-12,
 * and T#17 X#1-2 which use 1e-12. Never call with an invalid row.
 *
 * @param row Valid parsed row.
 * @return Nonnegative tolerance in reference units.
 */
double jpl_testpo_tolerance(const jpl_testpo_row *row);

#endif
