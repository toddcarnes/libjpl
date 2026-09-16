/* Internal shared constants and helpers for libjpl sources.
 * NOT installed; tests use tests/jpl_test_util.h instead. */
#ifndef LIBJPL_PRIVATE_H
#define LIBJPL_PRIVATE_H

#include <math.h>

/* IAU / JPL physical constants (exact values used across the library). */
#define JPL_AU_KM 149597870.7
#define JPL_DAYSEC 86400.0
#define JPL_C_AU_PER_DAY 173.144632684
/* Two-part Julian-date split used by ERFA-style calls. */
#define JPL_DJM0 2400000.5

/* Light-time solver: tolerance-based convergence per design spec. */
#define JPL_LIGHTTIME_TOL_DAYS 1e-12
#define JPL_LIGHTTIME_MAX_ITER 10

static inline double jpl_vec_norm3(const double v[3]) {
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

#endif
