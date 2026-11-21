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
    const jpl_date jd_tdb = {2451545.0, 0.0};  // J2000.0 TDB
    jpl_state state;

    if (jpl_open("DE441.eph", &de) != JPL_OK)
        return 1;

    if (jpl_get_state(de, &jd_tdb, JPL_EARTH, JPL_SUN, &state) != JPL_OK) {
        jpl_close(de);
        return 1;
    }

    printf("Earth from Sun at J2000:\n");
    printf("  X: %.6f AU  Y: %.6f AU  Z: %.6f AU\n", state.x, state.y, state.z);
    printf("  VX: %.6f AU/d  VY: %.6f AU/d  VZ: %.6f AU/d\n",
           state.vx, state.vy, state.vz);

    jpl_close(de);
    return 0;
}
```

## Example: Moon and Planets

```c
static int moon_and_planets(const char *path) {
    jpl_ephemeris *de;
    static const jpl_date jd_tt = {2451545.0, 0.0};  // J2000.0 TT
    jpl_date jd_tdb = {0.0, 0.0};
    jpl_state moon_earth, earth_sun, moon_sun;

    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(&jd_tt, &jd_tdb) != JPL_OK) { jpl_close(de); return 1; }

    // Moon position (Earth-centered)
    if (jpl_moon_position(de, &jd_tt, &moon_earth) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Earth from Sun
    if (jpl_planet_position(de, &jd_tt, JPL_EARTH, JPL_SUN, &earth_sun) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Approximate Moon from Sun = Moon from Earth + Earth from Sun
    // (Note: adding apparent legs mixes emission epochs; approximate only.)
    moon_sun.x = moon_earth.x + earth_sun.x;
    moon_sun.y = moon_earth.y + earth_sun.y;
    moon_sun.z = moon_earth.z + earth_sun.z;
    moon_sun.vx = moon_earth.vx + earth_sun.vx;
    moon_sun.vy = moon_earth.vy + earth_sun.vy;
    moon_sun.vz = moon_earth.vz + earth_sun.vz;

    jpl_close(de);
    return 0;
}
```

## Example: Light Time and Aberration

```c
static int light_and_aberration(const char *path) {
    jpl_ephemeris *de;
    static const jpl_date jd_tt = {2451545.0, 0.0};
    jpl_date jd_tdb = {0.0, 0.0};
    jpl_state geo, app;
    double lt;

    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(&jd_tt, &jd_tdb) != JPL_OK) { jpl_close(de); return 1; }

    // Geometric state (light-time corrected, no aberration)
    if (jpl_get_state_lighttime(de, &jd_tdb, JPL_MARS, JPL_SUN, &geo, &lt) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // Apparent state (light-time + aberration)
    if (jpl_planet_position(de, &jd_tt, JPL_MARS, JPL_SUN, &app) != JPL_OK) {
        jpl_close(de); return 1;
    }

    printf("Geometric: %f  Apparent: %f  Light time: %f days\n", geo.x, app.x, lt);

    jpl_close(de);
    return 0;
}
```

## Example: Frame Conversions

```c
static int frames(const char *path) {
    jpl_ephemeris *de;
    static const jpl_date jd_tt = {2451545.0, 0.0};
    jpl_date jd_tdb = {0.0, 0.0};
    jpl_state state, icrs, ecliptic;
    double R[3][3];

    if (jpl_open(path, &de) != JPL_OK) return 1;
    if (jpl_tt_to_tdb(&jd_tt, &jd_tdb) != JPL_OK) { jpl_close(de); return 1; }
    if (jpl_get_state(de, &jd_tdb, JPL_EARTH, JPL_SUN, &state) != JPL_OK) {
        jpl_close(de); return 1;
    }

    // ICRS -> mean ecliptic of date
    if (jpl_icrs_to_ecliptic(&state, &jd_tt, &ecliptic) != JPL_OK) { jpl_close(de); return 1; }

    // Mean ecliptic of date -> ICRS (via a cached frame context)
    {
        jpl_frame_context *ctx = NULL;
        if (jpl_frame_context_create(&jd_tt, &ctx) != JPL_OK) { jpl_close(de); return 1; }
        if (jpl_frame_context_rotate_state(ctx, &ecliptic, &icrs) != JPL_OK) { jpl_frame_context_destroy(ctx); jpl_close(de); return 1; }
        jpl_frame_context_destroy(ctx);
    }

    // Rotation matrix
    if (jpl_frame_rotation_matrix(&jd_tt, R) != JPL_OK) { jpl_close(de); return 1; }

    jpl_close(de);
    return 0;
}
```

## Example: Time Conversions

```c
static int time_conv(const char *path) {
    jpl_ephemeris *de;
    jpl_date jd_tt = {2451545.0, 0.0};
    jpl_date jd_tdb = {0.0, 0.0}, jd_tt2 = {0.0, 0.0};

    if (jpl_open(path, &de) != JPL_OK) return 1;

    // TT -> TDB
    if (jpl_tt_to_tdb(&jd_tt, &jd_tdb) != JPL_OK) { jpl_close(de); return 1; }

    // TDB -> TT (round-trip)
    if (jpl_tdb_to_tt(&jd_tdb, &jd_tt2) != JPL_OK) { jpl_close(de); return 1; }

    // Round-trip should be accurate to ~1e-15 days
    double diff = fabs(jd_tt2.hi - jd_tt.hi) + fabs(jd_tt2.lo - jd_tt.lo);
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

1. **Two-part dates**: Always build `jpl_date` structs (high + low parts) directly. Never compute a scalar and split it.
2. **Error handling**: Every function returns `jpl_status`. Check for `JPL_OK`.
3. **Thread safety**: Use `jpl_ephemeris_clone()` for concurrent reads on FILE path, or `jpl_open_mmap()`.
4. **Frame restrictions**: `jpl_planet_position` requires `JPL_FRAME_ICRF`.
5. **Resource cleanup**: Always call `jpl_close()` exactly once per handle.