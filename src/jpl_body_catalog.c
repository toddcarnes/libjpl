/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_private.h"

#include <string.h>

static jpl_status body_lookup(const jpl_header *header, int body,
                              jpl_body_plan *out) {
    if (!header || !out) return JPL_ERROR_ARGUMENT;
    memset(out, 0, sizeof *out);
    out->target = body;
    if (body == JPL_SSB || body == JPL_SOLAR_SYSTEM_BARYCENTER) {
        out->kind = JPL_BODY_ZERO;
        return JPL_OK;
    }
    if (body == JPL_NUTATIONS) {
        out->kind = JPL_BODY_ANGULAR;
        out->primary = &header->pointers[JPL_TARGET_COUNT - 1];
        out->components = 2;
        return JPL_OK;
    }
    if (body == JPL_LIBRATIONS) {
        out->kind = JPL_BODY_ANGULAR;
        out->primary = &header->libration_pointer;
        out->components = 3;
        return JPL_OK;
    }
    if (body >= JPL_MERCURY && body <= JPL_SUN &&
        body != JPL_EARTH && body != JPL_MOON) {
        out->kind = JPL_BODY_DIRECT;
        out->primary = &header->pointers[body - 1];
        out->components = 3;
        return JPL_OK;
    }
    if (body == JPL_EARTH || body == JPL_MOON || body == JPL_EARTH_MOON_BARYCENTER) {
        out->kind = JPL_BODY_EMB_MOON;
        out->primary = &header->pointers[JPL_FILE_SLOT_EMB];
        out->secondary = &header->pointers[JPL_MOON - 1];
        out->components = 3;
        return JPL_OK;
    }
    return JPL_ERROR_ARGUMENT;
}

jpl_status jpl_body_lookup(const jpl_header *header, int body,
                           jpl_body_plan *out) {
    return body_lookup(header, body, out);
}

jpl_status jpl_body_plan_for(jpl_query_mode mode, const jpl_header *header,
                             int target, int center, jpl_body_plan *out) {
    if (!header || !out) return JPL_ERROR_ARGUMENT;
    switch (mode) {
    case JPL_QUERY_STATE:
        /* Target zero is invalid; center zero is the SSB alias. */
        if (target <= 0 || target > JPL_LIBRATIONS) return JPL_ERROR_ARGUMENT;
        if (target == JPL_TT_TDB) return JPL_ERROR_ARGUMENT;
        if (target == JPL_NUTATIONS || target == JPL_LIBRATIONS)
            return center == JPL_SSB ? body_lookup(header, target, out) : JPL_ERROR_ARGUMENT;
        if (center < JPL_SSB || center > JPL_EARTH_MOON_BARYCENTER)
            return JPL_ERROR_ARGUMENT;
        if (center == JPL_SSB && target == JPL_SSB) return JPL_ERROR_ARGUMENT;
        return body_lookup(header, target, out);
    case JPL_QUERY_LIGHTTIME:
        if (target <= JPL_SSB || target > JPL_EARTH_MOON_BARYCENTER)
            return JPL_ERROR_ARGUMENT;
        if (center < JPL_SSB || center > JPL_EARTH_MOON_BARYCENTER)
            return JPL_ERROR_ARGUMENT;
        return body_lookup(header, target, out);
    case JPL_QUERY_PLANET:
        if (!((target >= JPL_MERCURY && target <= JPL_SUN) ||
              target == JPL_EARTH_MOON_BARYCENTER))
            return JPL_ERROR_ARGUMENT;
        if (center < JPL_SSB || center > JPL_EARTH_MOON_BARYCENTER)
            return JPL_ERROR_ARGUMENT;
        return body_lookup(header, target, out);
    case JPL_QUERY_APPARENT:
        if (target < JPL_MERCURY || target > JPL_SUN || target == JPL_EARTH)
            return JPL_ERROR_ARGUMENT;
        return body_lookup(header, target, out);
    }
    return JPL_ERROR_ARGUMENT;
}
