<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# C++ Binding Examples

## Building

```bash
# Configure and build the static library and C++ binding tests
cmake -S . -B build -DLIBJPL_BUILD_CPP_BINDING=ON
cmake --build build --config Debug --parallel

# Run the C++ binding test suite
ctest --test-dir build -C Debug -R jpl_cpp_binding --output-on-failure
```

To compile `planet_positions.cpp` standalone, compile with C++17 against
`bindings/cpp/include`, `include/`, `build/generated/`, and the built `jpl`
static library.

## Example: Planet Positions at J2000.0

The `planet_positions.cpp` example demonstrates:
- Opening an ephemeris file
- Converting TT to TDB
- Getting body states relative to SSB and Earth relative to Sun
- Handling `jpl::JplError` exceptions

## API Overview

The C++ binding provides a modern, RAII C++ interface:

```cpp
#include <jpl/jpl.hpp>

using namespace jpl;

int main() {
    // Open ephemeris (throws on error)
    Ephemeris eph("DE441.eph");

    // Two-part Julian dates (day + fraction)
    std::array<double, 2> jd_tt = {2451545.0, 0.0};
    auto jd_tdb = Ephemeris::tt_to_tdb(jd_tt);

    // Get state (position + velocity)
    auto state = eph.get_state(jd_tdb, JPL_EARTH, JPL_SUN);

    // Frame conversion (ICRS -> mean ecliptic of date -> ICRS)
    auto ecliptic = Ephemeris::icrs_to_ecliptic(state, jd_tt);
    auto icrs = Ephemeris::ecliptic_to_icrs(ecliptic, jd_tt);

    // Time conversion round-trip
    auto jd_tt_back = Ephemeris::tdb_to_tt(jd_tdb);

    // Light time
    auto lt = eph.get_state_lighttime(jd_tdb, JPL_EARTH, JPL_SUN);

    // Aberration (observer velocity in AU/day)
    std::array<double, 3> obs_vel = {0.0, 0.0172, 0.0};
    auto aberrated = Ephemeris::apply_aberration(state, obs_vel);

    // Clone handle for thread safety
    auto clone = eph.clone();
}
```

Key types:
- `Ephemeris` - RAII handle to ephemeris file
- `Header` - Parsed header with all metadata
- `JplError` - Exception with `jpl_status` code
- `std::array<double, 2>` for Julian dates
- `std::array<double, 6>` for state vectors
- `std::array<std::array<double, 3>, 3>` for 3x3 matrices