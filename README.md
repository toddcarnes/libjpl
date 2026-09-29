<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# libjpl

`libjpl` is a small C17 library for reading JPL Development Ephemeris
(DE) binary files. It bundles ERFA v2.0.1, so no separate third-party
library installation is required. It targets Windows, macOS, and Linux.

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

The complete usage contract (two-part dates, status returns, error and
output-on-error behaviour, frame gating, modern AU) lives in
[`docs/migration-two-part-dates.md`](docs/migration-two-part-dates.md) and
in the Doxygen API reference for `jpl.h`, `jpl_frames.h`, and
`jpl_convenience.h`. Build dates as two-part arrays directly (a day part
plus a fraction part): never compute a scalar full epoch first and split
it afterward, because the already-rounded scalar cannot be repaired by
splitting. On-disk record epochs stay scalar; only in-memory computation
uses pairs.

```c
#include <jpl.h>

jpl_ephemeris *de;
if (jpl_open("de440.bin", &de) == JPL_OK) {
    double earth_from_sun[6];
    static const double jd_tdb[2] = {2451545.0, 0.0};
    if (jpl_get_state(de, jd_tdb, JPL_EARTH, JPL_SUN, earth_from_sun) != JPL_OK) { /* handle error */ }
    /* x,y,z in AU; vx,vy,vz in AU/day */
    jpl_close(de);
}
```

The project ships only a small validation fixture (see Test data below),
not full operational ephemerides. Obtain operational DE files from
JPL/NASA and ensure they use the standard binary record layout.

### Moon wrt Sun

```c
#include <jpl.h>
#include <jpl_convenience.h>

jpl_ephemeris *de; /* from jpl_open(...) */
static const double jd_tt[2] = {2451545.0, 0.0};
double moon_earth[6], earth_sun[6], moon_sun[6];
/* jd_tt is Terrestrial Time; geometric calls take TDB (millisecond-scale apart). */
if (jpl_moon_position(de, jd_tt, moon_earth) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, jd_tt, JPL_EARTH, JPL_SUN, earth_sun) != JPL_OK) { /* handle error */ }
for (int i = 0; i < 6; ++i) moon_sun[i] = moon_earth[i] + earth_sun[i];
```

Note: adding apparent legs mixes emission epochs; approximate only (see jpl_convenience.h).

```c
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>

jpl_ephemeris *ephemeris; /* from jpl_open(...) */
const double tt[2] = {2451545.0, 0.125};
double tdb[2], state[6];
if (jpl_tt_to_tdb(tt, tdb) != JPL_OK) return 1;
if (jpl_get_state(ephemeris, tdb, JPL_EARTH, JPL_SUN, state) != JPL_OK) return 1;
```

```c
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>

jpl_ephemeris *de; /* from jpl_open(...) */
static const double jd_tt[2] = {2451545.0, 0.0};
double jd_tdb[2];
double geo[6], app[6], lt;
if (jpl_tt_to_tdb(jd_tt, jd_tdb) != JPL_OK) { /* handle error */ }
if (jpl_get_state_lighttime(de, jd_tdb, JPL_MARS, JPL_SUN, geo, &lt) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, jd_tt, JPL_MARS, JPL_SUN, app) != JPL_OK) { /* handle error */ }
```

### Geocentric apparent ecliptic position

```c
#include <jpl.h>
#include <jpl_convenience.h>

jpl_ephemeris *de; /* from jpl_open(...) */
static const double jd_tt[2] = {2451545.0, 0.0};
jpl_apparent_ecliptic_result position;
if (jpl_apparent_ecliptic_position(de, jd_tt, JPL_MOON, &position) != JPL_OK) { /* handle error */ }
/* position.longitude_deg, position.latitude_deg, position.range_au;
 * longitude_rate_deg_per_day indicates prograde or retrograde motion;
 * range_rate_km_per_s is Horizons-style line-of-sight radial motion,
 * not the derivative of position.range_au. */
```

Planet targets follow the DE planetary-system barycenters, which may
differ from physical planet centers.

The `jpl_examples` CTest suite compiles and runs these patterns from
`tests/examples.c` against the committed fixture.

## Language Bindings

Prebuilt shared-library bindings live under `bindings/<lang>/` and require
`-DLIBJPL_BUILD_SHARED=ON` plus the language-specific CMake option:

- **C++** (`bindings/cpp/`, `-DLIBJPL_BUILD_CPP_BINDING=ON`): header-only RAII wrapper
  `jpl::Ephemeris`, `jpl::JplError`, `jpl::planet_position`.
- **Python** (`bindings/python/`, `-DLIBJPL_BUILD_PYTHON_BINDING=ON`): cffi wrapper
  `libjpl.Ephemeris` context manager, `libjpl.JplError`.
- **C#** (`bindings/csharp/`, `-DLIBJPL_BUILD_CSHARP_BINDING=ON`): P/Invoke with
  `LibJpl.JplEphemeris : SafeHandle`, `LibJplApi.Open`, convenience helpers.
- **VB.NET** (`bindings/vbnet/`, `-DLIBJPL_BUILD_VBNET_BINDING=ON`): mirrors the
  C# API in VB.NET syntax.

Each binding's CMake option defaults to `OFF`; the test is absent (not failing)
if the toolchain (C++17, Python+cffi, .NET 8) is unavailable.

See [`BUILDING.md`](BUILDING.md#language-bindings) for per-language build
instructions.

## Credits

First-party code and documentation are MIT licensed, copyright
(c) 2026 Todd Carnes; see [`LICENSE`](LICENSE) and
[`LICENSES.md`](LICENSES.md), which also records ERFA and JPL test-data
provenance. Frame and time conversions use [ERFA](https://github.com/liberfa/erfa)
v2.0.1, Copyright (c) NumFOCUS Foundation, BSD-3-Clause; see
`src/erfa/LICENSE`. Both complete license texts install unconditionally
as `LICENSE` (project MIT) and `ERFA-LICENSE` (ERFA BSD-3).

## Test data

Always-on fixture (no network): `tests/data/de441_2000_2010.eph` is a
931 KB DE441 sub-ephemeris covering JD 2451536.5-2455216.5, and
`tests/data/testpo_441_slice.txt` holds 121 in-span reference rows.
`jpl_fixture` checks the file header and every reference row;
`jpl_fixture_integrity` checks manifest-pinned hashes. These tests do not
prove the original slicing command. See [fixture provenance](tests/data/PROVENANCE.md)
for source evidence and its limits.

The `jpl_horizons_offline` test compares apparent positions for the Sun,
Moon, and planet barycenters in the committed DE441 slice with pinned
Horizons values. `jpl_horizons_de441_local` checks the wider Horizons
overlap when `JPLEPH` points to a local directory containing `DE441.eph`.
Ordinary tests never query Horizons. The reference settings and limitations
are recorded in [fixture provenance](tests/data/PROVENANCE.md).

Opt-in fetch tier: `-DLIBJPL_FETCH_EPHEMERIDES=ON` (default OFF, approx
210 MB) downloads DE440/DE440t binaries plus `testpo.440`/`testpo.440t`
from JPL (`https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/` and
`.../de440t/`, filenames `linux_p1550p2650.440[t]` plus co-located
`testpo.440[t]`), verifies them per `tests/data/MANIFEST.txt` (exact byte
size and SHA-256 for all four downloaded files), and registers the integration
plus reference suites (`jpl_real_ephemeris`,
`jpl_all_ephemerides` (download-only builds, when the resolved download
dir is non-empty), `jpl_all_ephemerides_local` (only when `JPLEPH` is set),
`jpl_testpo_440`, `jpl_testpo_440t`).

Manifest updates on JPL re-cuts: refresh the changed URL/size/hash in
`tests/data/MANIFEST.txt` (the single source CMake reads at configure
time; `CMakeLists.txt` carries no download literals) and re-run
configure -- any mismatch fails configure naming file, expected vs
actual, and URL.

Maintainer note: the maintainer-local sweep stays local -- with the option ON,
`JPLEPH`/`JPL_TESTPO` dirs redirect the download sources per file instead
of auto-registering suites on their own (see `jpl_all_ephemerides[_local]`
in `CMakeLists.txt`).
