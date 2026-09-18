# libjpl

`libjpl` is a small, dependency-free (ERFA vendored under `src/erfa/`) C17 library for reading JPL Development
Ephemeris (DE) binary files. It targets Windows, macOS, and Linux.

It opens standard DE binaries in either byte order, exposes headers/constants,
reads raw Chebyshev coefficient records, and interpolates planetary state
vectors at any supported TDB Julian date.

## Build

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

(`--config`/`-C` required on multi-config generators like MSVC; omit on
single-config Makefiles/Ninja, which default to Release.)

## Use

```c
#include <jpl.h>

jpl_ephemeris *de;
if (jpl_open("de440.bin", &de) == JPL_OK) {
    double earth_from_sun[6];
    if (jpl_get_state(de, 2451545.0, JPL_EARTH, JPL_SUN, earth_from_sun) != JPL_OK) { /* handle error */ }
    /* x,y,z in AU; vx,vy,vz in AU/day */
    jpl_close(de);
}
```

The project does not ship ephemeris data. Obtain DE files from JPL/NASA and
ensure they use the standard binary record layout.

### Moon wrt Sun

```c
#include <jpl.h>
#include <jpl_convenience.h>

jpl_ephemeris *de; /* from jpl_open(...) */
double jd_tt = 2451545.0;
double moon_earth[6], earth_sun[6], moon_sun[6];
/* jd_tt is Terrestrial Time; geometric calls take TDB (~70 s apart). */
if (jpl_moon_position(de, jd_tt, moon_earth) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, jd_tt, JPL_EARTH, JPL_SUN, earth_sun) != JPL_OK) { /* handle error */ }
for (int i = 0; i < 6; ++i) moon_sun[i] = moon_earth[i] + earth_sun[i];
```

Note: adding apparent legs mixes emission epochs; approximate only (see jpl_convenience.h).

```c
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>

jpl_ephemeris *de; /* from jpl_open(...) */
double jd_tt = 2451545.0, jd_tdb = jpl_tt_to_tdb(jd_tt);
double geo[6], app[6], lt;
if (jpl_get_state_lighttime(de, jd_tdb, JPL_MARS, JPL_SUN, geo, &lt) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, jd_tt, JPL_MARS, JPL_SUN, app) != JPL_OK) { /* handle error */ }
```

## Credits

Frame conversions use [ERFA](https://github.com/liberfa/erfa) v2.0.1,
Copyright (c) NumFOCUS Foundation, BSD-3-Clause. See `src/erfa/LICENSE`.

## Test data

Always-on fixture (no network): `tests/data/de441_2000_2010.eph` is a
931 KB DE441 sub-ephemeris sliced with `sub_eph DE441.eph <out> 2000 2010`
(JD 2451536.5-2455216.5), and `tests/data/testpo_441_slice.txt` holds the
121 in-span `testpo.441` lines. The `jpl_fixture` suite asserts this
provenance (DENUM 441, exact coverage) plus all 121 cross-checks, so a
fresh clone is green with no downloads.

Opt-in fetch tier: `-DLIBJPL_FETCH_EPHEMERIDES=ON` (default OFF, approx
210 MB) downloads DE440/DE440t binaries plus `testpo.440`/`testpo.440t`
from JPL (`https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/` and
`.../de440t/`, filenames `linux_p1550p2650.440[t]` plus co-located
`testpo.440[t]`), verifies them per `tests/data/MANIFEST.txt` (exact byte
size for binaries, SHA-256 for testpo), and registers the integration
plus reference suites (`jpl_real_ephemeris`,
`jpl_all_ephemerides` (download-only builds, when the resolved download
dir is non-empty), `jpl_all_ephemerides_local` (only when `JPLEPH` is set),
`jpl_testpo_440`, `jpl_testpo_440t`).

Manifest updates on JPL re-cuts: refresh the changed URL/size/hash in
`tests/data/MANIFEST.txt` (the single source CMake reads at configure
time; `CMakeLists.txt` carries no download literals) and re-run
configure -- any mismatch fails configure naming file, expected vs
actual, and URL.

Maintainer note: the 31-file local sweep stays local -- with the option ON,
`JPLEPH`/`JPL_TESTPO` dirs redirect the download sources per file instead
of auto-registering suites on their own.
