/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Test-only hook declarations. Included by test binaries that link the
 * non-installed jpl_testing archive; not installed alongside the public API
 * headers. The LIBJPL_TESTING macro gates visibility of these declarations
 * so production builds and installed headers never expose them. */
#ifndef LIBJPL_TESTING_H
#define LIBJPL_TESTING_H

#include "jpl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LIBJPL_TESTING

#include "jpl_private.h"

/* Private deterministic light-time iteration-budget hook for tests.
 * Defined in the non-installed jpl_testing archive under LIBJPL_TESTING;
 * absent from the installed jpl archive. The public solver
 * calls the same internal implementation with JPL_LIGHTTIME_FAST_MAX_ITER
 * and JPL_LIGHTTIME_SAFE_MAX_ITER. Budgets count that iteration family
 * only (zero fast + zero safeguarded forces CONVERGENCE on non-root
 * cases). Negative budgets are rejected as JPL_ERROR_ARGUMENT with no
 * output written.
 *
 * @brief Evaluate light-time with explicit iteration budgets.
 * @param ephemeris Open ephemeris handle.
 * @param jd Reception Julian date (two-part).
 * @param target Target body code (1-13).
 * @param center Center body code (0-13, 0 = SSB).
 * @param state Output relative state[6] (AU, AU/day), unchanged on failure.
 * @param delay Output light-time in days, unchanged on failure.
 * @param fast_iterations Fixed-point iteration budget (0 forces CONVERGENCE).
 * @param safeguarded_iterations Bisection budget (0 forces CONVERGENCE).
 * @return JPL_OK on convergence; JPL_ERROR_ARGUMENT for NULL args or
 *         negative budgets; JPL_ERROR_RANGE for out-of-coverage;
 *         JPL_ERROR_CONVERGENCE on budget exhaustion; other codes from
 *         underlying interpolation.
 * @note Only available when LIBJPL_TESTING is defined.
 */
jpl_status jpl_test_lighttime_with_budget(const jpl_ephemeris *ephemeris,
    const double jd[2], int target, int center, double state[6], double *delay,
    int fast_iterations, int safeguarded_iterations);

/* Bounded-read test hook: caps parser source reads at max_bytes when
 * nonzero (0 disables). Production builds without LIBJPL_TESTING bypass it
 * entirely; the hook is test-only and never declared in installed headers.
 *
 * @brief Limit parser read size (test-only).
 * @param max_bytes Maximum bytes per read, or 0 to disable.
 * @note Only available when LIBJPL_TESTING is defined.
 */
void jpl_test_set_parser_read_limit(size_t max_bytes);

/* Exercise the selector's own range checks without interpolate's outer
 * coverage guard. This hook exists only in the private testing archive.
 *
 * @brief Select record index for a date (test-only).
 * @param ephemeris Open ephemeris handle.
 * @param jd Julian date (two-part).
 * @param index Output record index.
 * @param start Output scheduled record start (two-part).
 * @param end Output scheduled record end (two-part).
 * @return JPL_OK on success; JPL_ERROR_RANGE if out of coverage.
 * @note Only available when LIBJPL_TESTING is defined.
 */
jpl_status jpl_test_select_record(const jpl_ephemeris *ephemeris,
    const double jd[2], size_t *index, double start[2], double end[2]);

/* Public jpl_get_state call counter for the Step-1 observer-read pin.
 * Tests reset the counter, run one convenience query, and assert that the
 * geometry path no longer performs the redundant observer read.
 *
 * @brief Return the number of public jpl_get_state calls since reset.
 * @return Call count.
 * @note Only available when LIBJPL_TESTING is defined.
 */
unsigned long jpl_test_get_state_call_count(void);

/** @brief Reset the public jpl_get_state call counter (test-only).
 *  @note Only available when LIBJPL_TESTING is defined. */
void jpl_test_reset_get_state_call_count(void);

/* Entry-gate run counter for the one-gate-per-query pin.
 *
 * @brief Return the number of jpl_entry_prepare runs since reset.
 * @return Gate run count.
 * @note Only available when LIBJPL_TESTING is defined.
 */
unsigned long jpl_test_entry_prepare_call_count(void);

/** @brief Reset the entry-gate run counter (test-only).
 *  @note Only available when LIBJPL_TESTING is defined. */
void jpl_test_reset_entry_prepare_call_count(void);

#endif /* LIBJPL_TESTING */

#ifdef __cplusplus
}
#endif

#endif /* LIBJPL_TESTING_H */
