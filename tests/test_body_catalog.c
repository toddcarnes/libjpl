/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl.h"
#include "jpl_private.h"
#include "jpl_test_util.h"

#include <string.h>

int main(void) {
    jpl_header h;
    jpl_body_plan plan;

    memset(&h, 0, sizeof h);
    h.pointers[JPL_FILE_SLOT_EMB].start_index = 3;
    h.pointers[JPL_FILE_SLOT_EMB].coefficients_per_component = 1;
    h.pointers[JPL_FILE_SLOT_EMB].subintervals = 1;
    h.pointers[JPL_MOON - 1].start_index = 9;
    h.pointers[JPL_MOON - 1].coefficients_per_component = 1;
    h.pointers[JPL_MOON - 1].subintervals = 1;
    h.pointers[JPL_TARGET_COUNT - 1].start_index = 20;
    h.pointers[JPL_TARGET_COUNT - 1].coefficients_per_component = 1;
    h.pointers[JPL_TARGET_COUNT - 1].subintervals = 1;
    h.libration_pointer.start_index = 30;
    h.libration_pointer.coefficients_per_component = 1;
    h.libration_pointer.subintervals = 1;

    /* STATE: direct, derived, angular, and invalid combinations. */
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_MARS, 0, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_DIRECT && plan.target == JPL_MARS);
    JPL_TEST_CHECK(plan.primary == &h.pointers[JPL_MARS - 1]);
    JPL_TEST_CHECK(plan.components == 3);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_EARTH, 0, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_EMB_MOON);
    JPL_TEST_CHECK(plan.primary == &h.pointers[JPL_FILE_SLOT_EMB]);
    JPL_TEST_CHECK(plan.secondary == &h.pointers[JPL_MOON - 1]);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_MOON, JPL_EARTH, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_EMB_MOON);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_NUTATIONS, 0, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_ANGULAR && plan.components == 2);
    JPL_TEST_CHECK(plan.primary == &h.pointers[JPL_TARGET_COUNT - 1]);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_LIBRATIONS, 0, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_ANGULAR && plan.components == 3);
    JPL_TEST_CHECK(plan.primary == &h.libration_pointer);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_NUTATIONS, 1, &plan) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_TT_TDB, 0, &plan) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_SSB, 0, &plan) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_STATE, &h, JPL_MARS, 99, &plan) == JPL_ERROR_ARGUMENT);

    /* LIGHTTIME: bodies 1-13, centers 0-13; angular targets rejected. */
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_LIGHTTIME, &h, JPL_MARS, 0, &plan) == JPL_OK);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_LIGHTTIME, &h, JPL_EARTH_MOON_BARYCENTER,
                                     JPL_EARTH_MOON_BARYCENTER, &plan) == JPL_OK);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_LIGHTTIME, &h, JPL_SSB, 0, &plan) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_LIGHTTIME, &h, JPL_NUTATIONS, 0, &plan) == JPL_ERROR_ARGUMENT);

    /* PLANET: convenience validity (1-11 or EMB; center 0-13). */
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_PLANET, &h, JPL_SUN, JPL_EARTH_MOON_BARYCENTER, &plan) == JPL_OK);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_PLANET, &h, JPL_SOLAR_SYSTEM_BARYCENTER, 0, &plan) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_PLANET, &h, JPL_MARS, JPL_NUTATIONS, &plan) == JPL_ERROR_ARGUMENT);

    /* APPARENT: geocentric targets 1-11 excluding Earth. */
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_APPARENT, &h, JPL_MOON, JPL_EARTH, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_EMB_MOON);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_APPARENT, &h, JPL_SUN, JPL_EARTH, &plan) == JPL_OK);
    JPL_TEST_CHECK(plan.kind == JPL_BODY_DIRECT);
    JPL_TEST_CHECK(jpl_body_plan_for(JPL_QUERY_APPARENT, &h, JPL_EARTH, JPL_EARTH, &plan) == JPL_ERROR_ARGUMENT);

    /* Body lookup: SSB zero and out-of-range rejection. */
    JPL_TEST_CHECK(jpl_body_lookup(&h, JPL_SSB, &plan) == JPL_OK && plan.kind == JPL_BODY_ZERO);
    JPL_TEST_CHECK(jpl_body_lookup(&h, JPL_TT_TDB, &plan) == JPL_ERROR_ARGUMENT);
    JPL_TEST_CHECK(jpl_body_lookup(NULL, JPL_MARS, &plan) == JPL_ERROR_ARGUMENT);
    return 0;
}
