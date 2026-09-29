<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# C++ Binding Examples

## Building

```bash
# Configure with C++ binding enabled
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_CPP_BINDING=ON

# Build
cmake --build build --config Debug --parallel

# Run example
./build/bindings/cpp/examples/planet_positions /path/to/DE441.eph
```

## Example: Planet Positions at J2000.0

The `planet_positions.cpp` example demonstrates:
- Opening an ephemeris file
- Converting TT to TDB
- Getting planet positions relative to SSB
- Frame conversions (ecliptic ↔ ICRS)
- Time conversions (TT ↔ TDB)
- Light time calculation
- Aberration correction

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
    auto state = eph.get_state(jd_tdb, JplTarget::Earth, JplTarget::Sun);

    // Frame conversion
    auto icrs = Ephemeris::ecliptic_to_icrs(state, jd_tt);

    // Time conversion
    auto jd_tt = Ephemeris::tdb_to_tt(jd_tdb);

    // Light time
    auto lt = eph.get_state_lighttime(jd_tdb, 3, 11);

    // Aberration
    auto aberrated = Ephemeris::apply_aberration(geom_state, obs_vel);

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