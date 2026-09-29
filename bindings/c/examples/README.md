<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# C API Examples

The C library provides usage examples in `tests/examples.c`, which is compiled as the `jpl_examples` test executable.

## Building

```bash
# Standard build
cmake -S . -B build
cmake --build build --config Debug

# Run examples
./build/jpl_examples /path/to/DE441.eph
```

## Example: Basic State

```c
#include <jpl.h>
#include <stdio.h>

int main(int argc, char** argv) {
    jpl_ephemeris *de;
    const double jd_tdb[2] = {2451545.0, 0.0};  // J2000.0 TDB
    double state[6];

    if (jpl_open("DE441.eph", &de) != JPL_OK)
        return 1;

    if (jpl_get_state(de, jd_tdb, JPL_EARTH, JPL_SUN, state) != JPL_OK) {
        jpl_close(de);
        return 1;
    }

    printf("Earth from Sun at J2000:\n");
    printf("  X: %.6f AU  Y: %.6f AU  Z: %.6f AU\n", state[0], state[1], state[2]);
    printf("  VX: %.6f AU/d  VY: %.6f AU/d  VZ: %.6f AU/d\n",
           state[3], state[4], state[5]);

    jpl_close(de);
    return 0;
}
```

## Example: Moon and Planets

```c
static int moon_and_planets(const char *path) {
    jpl_ephemeris *de;
    static const double jd_tt[2] = {2451545.0, 0.0};  // J2000.0 TT
    double jd_tdb[2], moon_earth[6], earth_sun[6], moon_sun[6];

    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(jd_tt, jd_tdb) != JPL_OK) { jpl_close(de); return 1; }

    // Moon position (Earth-centered)
    if (jpl_moon_position(de, jd_tt, moon_earth) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Earth from Sun
    if (jpl_planet_position(de, jd_tt, JPL_EARTH, JPL_SUN, earth_sun) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Moon from Sun = Moon from Earth + Earth from Sun
    for (int i = 0; i < 6; ++i) moon_sun[i] = moon_earth[i] + earth_sun[i];

    jpl_close(de);
    return 0;
}
```

## Example: Light Time and Aberration

```c
static int light_and_aberration(const char *path) {
    jpl_ephemeris *de;
    static const double jd_tt[2] = {2451545.0, 0.0};
    double jd_tdb[2], geo[6], app[6], lt;

    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(jd_tt, jd_tdb) != JPL_OK) { jpl_close(de); return 1; }

    // Geometric state (light-time corrected, no aberration)
    if (jpl_get_state_lighttime(de, jd_tdb, JPL_MARS, JPL_SUN, geo, &lt) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Apparent state (light-time + aberration)
    if (jpl_planet_position(de, jd_tt, JPL_MARS, JPL_SUN, app) != JPL_OK) {
        jpl_close(de); return 1;
    }

    printf("Geometric: %f  Apparent: %f  Light time: %f days\n", geo[0], app[0], lt);

    jpl_close(de);
    return 0;
}
```

## Example: Frame Conversions

```c
static int frames(const char *path) {
    jpl_ephemeris *de;
    static const double jd_tt[2] = {2451545.0, 0.0};
    double state[6], icrs[6], ecliptic[6], R[3][3];

    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_get_state(de, jd_tdb, JPL_EARTH, JPL_SUN, state) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Ecliptic -> ICRS
    if (jpl_ecliptic_to_icrs(state, jd_tt, icrs) != JPL_OK) { jpl_close(de); return 1; }

    // ICRS -> Ecliptic
    if (jpl_icrs_to_ecliptic(state, jd_tt, ecliptic) != JPL_OK) { jpl_close(de); return 1; }

    // Rotation matrix
    if (jpl_frame_rotation_matrix(jd_tt, R) != JPL_OK) { jpl_close(de); return 1; }

    jpl_close(de);
    return 0;
}
```

## Example: Time Conversions

```c
static int time_conv(const char *path) {
    jpl_ephemeris *de;
    double jd_tt[2] = {2451545.0, 0.0};
    double jd_tdb[2], jd_tt2[2];

    if (jpl_open(path, &de) != JPL_OK) return 1;

    // TT -> TDB
    if (jpl_tt_to_tdb(jd_tt, jd_tdb) != JPL_OK) { jpl_close(de); return 1; }

    // TDB -> TT (round-trip)
    if (jpl_tdb_to_tt(jd_tdb, jd_tt2) != JPL_OK) { jpl_close(de); return 1; }

    // Round-trip should be accurate to ~1e-15 days
    double diff = fabs(jd_tt2[0] - jd_tt[0]) + fabs(jd_tt2[1] - jd_tt[1]);
    printf("Round-trip error: %.3e days\n", diff);

    jpl_close(de);
    return 0;
}
```

## Running the Examples

```bash
# Build
cmake -S . -B build
cmake --build build --config Debug

# Run
./build/jpl_examples /path/to/DE441.eph
```

## Key Points

1. **Two-part dates**: Always use `double[2]` for Julian dates (day + fraction). Never compute a scalar and split it.
2. **Error handling**: Every function returns `jpl_status`. Check for `JPL_OK`.
3. **Thread safety**: Use `jpl_ephemeris_clone()` for concurrent reads on FILE path.
4. **Frame restrictions**: `jpl_planet_position` requires `JPL_FRAME_ICRF`.
5. **Two-part dates**: Build day+fraction pairs directly. Never split a scalar epoch.
6. **Resource cleanup**: Always call `jpl_close()` exactly once per handle.