/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* README-compiled usage examples (test-only).
 * Every snippet below also appears in README.md; CTest runs this
 * executable with the committed fixture path so the examples stay
 * buildable and status-checked. Never compute a scalar full epoch and
 * split it: pairs are built directly as day plus fraction parts. */
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>
#include <stdio.h>

static int basic_state(const char *path) {
    jpl_ephemeris *ephemeris = NULL;
    const jpl_date tt = {2451545.0, 0.125};
    jpl_date tdb = {0.0, 0.0};
    jpl_state state;
    if (jpl_open(path, &ephemeris) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(&tt, &tdb) != JPL_OK) { jpl_close(ephemeris); return 1; }
    if (jpl_get_state(ephemeris, &tdb, JPL_EARTH, JPL_SUN, &state) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    printf("basic: x=%f v=%f\n", state.x, state.vx);
    jpl_close(ephemeris);
    return 0;
}

static int moon_sun(const char *path) {
    jpl_ephemeris *ephemeris = NULL;
    static const jpl_date jd_tt = {2451545.0, 0.0};
    jpl_state moon_earth, earth_sun, moon_sun;
    if (jpl_open(path, &ephemeris) != JPL_OK) return 1;
    if (jpl_moon_position(ephemeris, &jd_tt, &moon_earth) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    if (jpl_planet_position(ephemeris, &jd_tt, JPL_EARTH, JPL_SUN, &earth_sun) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    moon_sun.x = moon_earth.x + earth_sun.x;
    moon_sun.y = moon_earth.y + earth_sun.y;
    moon_sun.z = moon_earth.z + earth_sun.z;
    moon_sun.vx = moon_earth.vx + earth_sun.vx;
    moon_sun.vy = moon_earth.vy + earth_sun.vy;
    moon_sun.vz = moon_earth.vz + earth_sun.vz;
    printf("moon_sun: x=%f\n", moon_sun.x);
    jpl_close(ephemeris);
    return 0;
}

static int geometric_vs_apparent(const char *path) {
    jpl_ephemeris *ephemeris = NULL;
    static const jpl_date jd_tt = {2451545.0, 0.0};
    jpl_date jd_tdb = {0.0, 0.0};
    jpl_state geo, app;
    double lt;
    if (jpl_open(path, &ephemeris) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(&jd_tt, &jd_tdb) != JPL_OK) { jpl_close(ephemeris); return 1; }
    if (jpl_get_state_lighttime(ephemeris, &jd_tdb, JPL_MARS, JPL_SUN, &geo, &lt) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    if (jpl_planet_position(ephemeris, &jd_tt, JPL_MARS, JPL_SUN, &app) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    printf("geo=%f app=%f lt=%f\n", geo.x, app.x, lt);
    jpl_close(ephemeris);
    return 0;
}

static int geocentric_ecliptic(const char *path) {
    jpl_ephemeris *de = NULL;
    static const jpl_date jd_tt = {2451545.0, 0.0};
    jpl_apparent_ecliptic_result position;
    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_apparent_ecliptic_position(de, &jd_tt, JPL_MOON, &position) != JPL_OK) {
        jpl_close(de);
        return 1;
    }
    printf("moon: lon=%f lat=%f range=%f lon_rate=%f range_rate=%f\n",
           position.longitude_deg, position.latitude_deg, position.range_au,
           position.longitude_rate_deg_per_day, position.range_rate_km_per_s);
    jpl_close(de);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: jpl_examples <fixture.eph>\n");
        return 1;
    }
    if (basic_state(argv[1]) != 0) return 1;
    if (moon_sun(argv[1]) != 0) return 1;
    if (geometric_vs_apparent(argv[1]) != 0) return 1;
    if (geocentric_ecliptic(argv[1]) != 0) return 1;
    puts("examples passed");
    return 0;
}
