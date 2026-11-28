/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_private.h"
#include "jpl_test_util.h"

#include <math.h>
#include <string.h>

static void pair(double hi, double lo, double out[2]) {
    out[0] = hi;
    out[1] = lo;
}

int main(void) {
    jpl_header h;
    jpl_verified_epoch epoch;
    jpl_record_span record;
    jpl_subinterval_span sub;
    double out[2];

    memset(&h, 0, sizeof h);
    h.start_jd[0] = 2451545.0;
    h.end_jd[0] = 2451555.0;
    h.step_days = 1.0;

    /* Scheduled endpoint. */
    pair(2451545.0, 0.0, h.start_jd);
    JPL_TEST_CHECK(jpl_schedule_endpoint(h.start_jd, 1.0, 0, out) == JPL_OK);
    JPL_TEST_CHECK(out[0] == 2451545.0 && out[1] == 0.0);
    JPL_TEST_CHECK(jpl_schedule_endpoint(h.start_jd, 1.0, 5, out) == JPL_OK);
    JPL_TEST_CHECK(out[0] == 2451550.0 && out[1] == 0.0);
    JPL_TEST_CHECK(jpl_schedule_endpoint(NULL, 1.0, 0, out) == JPL_ERROR_FORMAT);

    /* Record lookup: interior, final advertised endpoint, and RANGE. */
    pair(2451550.5, 0.0, epoch.epoch);
    memcpy(epoch.cov_start, h.start_jd, sizeof epoch.cov_start);
    memcpy(epoch.cov_end, h.end_jd, sizeof epoch.cov_end);
    JPL_TEST_CHECK(jpl_schedule_record(&h, 10, &epoch, &record) == JPL_OK);
    JPL_TEST_CHECK(record.index == 5);
    JPL_TEST_CHECK(record.start[0] == 2451550.0);
    JPL_TEST_CHECK(record.end[0] == 2451551.0);
    pair(2451555.0, 0.0, epoch.epoch);
    JPL_TEST_CHECK(jpl_schedule_record(&h, 10, &epoch, &record) == JPL_OK);
    JPL_TEST_CHECK(record.index == 9 && record.end[0] == 2451555.0);
    pair(2451544.0, 0.0, epoch.epoch);
    JPL_TEST_CHECK(jpl_schedule_record(&h, 10, &epoch, &record) == JPL_ERROR_RANGE);

    /* Record-bounds verification: inclusive end. */
    {
        double dates[2] = {2451550.0, 2451551.0};
        pair(2451550.5, 0.0, epoch.epoch);
        JPL_TEST_CHECK(jpl_schedule_verify_record(5, dates, &epoch, &record) == JPL_OK);
        JPL_TEST_CHECK(record.index == 5 && record.start[0] == 2451550.0);
        pair(2451551.0, 0.0, epoch.epoch);
        JPL_TEST_CHECK(jpl_schedule_verify_record(5, dates, &epoch, &record) == JPL_OK);
        pair(2451549.5, 0.0, epoch.epoch);
        JPL_TEST_CHECK(jpl_schedule_verify_record(5, dates, &epoch, &record) == JPL_ERROR_FORMAT);
    }

    /* Subinterval selection: two halves of a one-day record. */
    {
        jpl_record_span span;
        span.index = 0;
        pair(2451550.0, 0.0, span.start);
        pair(2451551.0, 0.0, span.end);
        pair(2451550.1, 0.0, epoch.epoch);
        JPL_TEST_CHECK(jpl_schedule_subinterval(&span, 2, &epoch, &sub) == JPL_OK);
        JPL_TEST_CHECK(sub.index == 0 && fabs(sub.substep - 0.5) < 1e-15);
        JPL_TEST_CHECK(fabs(sub.argument + 0.6) < 1e-9);
        pair(2451550.9, 0.0, epoch.epoch);
        JPL_TEST_CHECK(jpl_schedule_subinterval(&span, 2, &epoch, &sub) == JPL_OK);
        JPL_TEST_CHECK(sub.index == 1 && fabs(sub.argument - 0.6) < 1e-9);
        JPL_TEST_CHECK(jpl_schedule_subinterval(&span, 0, &epoch, &sub) == JPL_ERROR_ARGUMENT);
    }
    return 0;
}
