# libjpl

`libjpl` is a small, dependency-free (ERFA vendored under `src/erfa/`) C17 library for reading JPL Development
Ephemeris (DE) binary files. It targets Windows, macOS, and Linux.

It opens standard DE binaries in either byte order, exposes headers/constants,
reads raw Chebyshev coefficient records, and interpolates planetary state
vectors at any supported TDB Julian date.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Use

```c
#include <jpl.h>

jpl_ephemeris *de;
if (jpl_open("de440.bin", &de) == JPL_OK) {
    double earth_from_sun[6];
    jpl_get_state(de, 2451545.0, JPL_EARTH, JPL_SUN, earth_from_sun);
    /* x,y,z in AU; vx,vy,vz in AU/day */
    jpl_close(de);
}
```

The project does not ship ephemeris data. Obtain DE files from JPL/NASA and
ensure they use the standard binary record layout.

### Moon wrt Sun

```c
double moon_earth[6], earth_sun[6], moon_sun[6];
if (jpl_moon_position(de, jd_tt, moon_earth) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, jd_tt, JPL_EARTH, JPL_SUN, earth_sun) != JPL_OK) { /* handle error */ }
for (int i = 0; i < 6; ++i) moon_sun[i] = moon_earth[i] + earth_sun[i];
```

```c
double geo[6], app[6], lt;
if (jpl_get_state_lighttime(de, 2451545.0, JPL_MARS, JPL_SUN, geo, &lt) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, 2451545.0, JPL_MARS, JPL_SUN, app) != JPL_OK) { /* handle error */ }
```

## Credits

Frame conversions use [ERFA](https://github.com/liberfa/erfa) v2.0.1,
Copyright (c) NumFOCUS Foundation, BSD-3-Clause. See `src/erfa/LICENSE`.
