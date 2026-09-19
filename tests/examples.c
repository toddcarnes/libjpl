/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* README-compiled usage examples (test-only, owned by T21).
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
    const double tt[2] = {2451545.0, 0.125};
    double tdb[2], state[6];
    if (jpl_open(path, &ephemeris) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(tt, tdb) != JPL_OK) { jpl_close(ephemeris); return 1; }
    if (jpl_get_state(ephemeris, tdb, JPL_EARTH, JPL_SUN, state) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    printf("basic: x=%f v=%f\n", state[0], state[3]);
    jpl_close(ephemeris);
    return 0;
}

static int moon_sun(const char *path) {
    jpl_ephemeris *ephemeris = NULL;
    static const double jd_tt[2] = {2451545.0, 0.0};
    double moon_earth[6], earth_sun[6], moon_sun[6];
    int i;
    if (jpl_open(path, &ephemeris) != JPL_OK) return 1;
    if (jpl_moon_position(ephemeris, jd_tt, moon_earth) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    if (jpl_planet_position(ephemeris, jd_tt, JPL_EARTH, JPL_SUN, earth_sun) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    for (i = 0; i < 6; ++i) moon_sun[i] = moon_earth[i] + earth_sun[i];
    printf("moon_sun: x=%f\n", moon_sun[0]);
    jpl_close(ephemeris);
    return 0;
}

static int geometric_vs_apparent(const char *path) {
    jpl_ephemeris *ephemeris = NULL;
    static const double jd_tt[2] = {2451545.0, 0.0};
    double jd_tdb[2];
    double geo[6], app[6], lt;
    if (jpl_open(path, &ephemeris) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(jd_tt, jd_tdb) != JPL_OK) { jpl_close(ephemeris); return 1; }
    if (jpl_get_state_lighttime(ephemeris, jd_tdb, JPL_MARS, JPL_SUN, geo, &lt) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    if (jpl_planet_position(ephemeris, jd_tt, JPL_MARS, JPL_SUN, app) != JPL_OK) {
        jpl_close(ephemeris);
        return 1;
    }
    printf("geo=%f app=%f lt=%f\n", geo[0], app[0], lt);
    jpl_close(ephemeris);
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
    puts("examples passed");
    return 0;
}
