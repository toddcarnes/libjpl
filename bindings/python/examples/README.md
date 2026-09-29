<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Python Binding Examples

## Building

```bash
# Configure with Python binding enabled
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_PYTHON_BINDING=ON

# Build
cmake --build build --config Debug

# Run example
python bindings/python/examples/planet_positions.py /path/to/DE441.eph
```

## Requirements

- Python 3.8+
- `cffi` (`pip install cffi`)

## API Overview

The Python binding provides a Pythonic, context-manager based interface:

```python
from libjpl import Ephemeris, JplTarget, JplStatus

# Open ephemeris (context manager auto-closes)
with Ephemeris("DE441.eph") as eph:
    # Two-part Julian dates [day, fraction]
    jd_tt = [2451545.0, 0.0]  # J2000.0 TT

    # Time conversions
    jd_tdb = Ephemeris.tt_to_tdb([2451545.0, 0.0])
    jd_tt = Ephemeris.tdb_to_tt(jd_tdb)

    # Get state vector (position + velocity)
    state = eph.get_state(jd_tdb, 3, 11)  # Earth relative to Sun

    # Planet positions
    state = eph.planet_position(jd_tt, 3, 11)  # Earth relative to Sun
    state = eph.moon_position(jd_tt)           # Moon relative to Earth

    # Light time
    state, lt = eph.get_state_lighttime(jd_tdb, 3, 11)

    # Frame conversions
    icrs = Ephemeris.ecliptic_to_icrs(state, jd_tt)
    ecliptic = Ephemeris.icrs_to_ecliptic(state, jd_tt)
    R = Ephemeris.frame_rotation_matrix(jd_tt)

    # Time conversions
    jd_tdb = Ephemeris.tt_to_tdb(jd_tt)
    jd_tt = Ephemeris.tdb_to_tt(jd_tdb)

    # Aberration
    aberrated = Ephemeris.apply_aberration(geom_state, obs_vel)
    aberrated = Ephemeris.apply_aberration_full(geom_state, obs_vel, sun_dist_au)

    # Light time
    state, lt = eph.get_state_lighttime(jd_tdb, 3, 11)

    # Clone for thread safety
    clone = eph.clone()
```

## Error Handling

```python
from libjpl import Ephemeris, JplError, JplStatus

try:
    with Ephemeris("DE441.eph") as eph:
        state = eph.get_state([2451545.0, 0.0], 3, 11)
except JplError as e:
    print(f"JplError: {e.message} (status={e.status})")
except Exception as e:
    print(f"Error: {e}")

# Check specific status
try:
    with Ephemeris("nonexistent.eph") as eph:
        pass
except JplError as e:
    if e.status == JplStatus.ERROR_IO:
        print("File not found")
    elif e.status == JplStatus.ERROR_RANGE:
        print("Date out of range")
```

## Context Manager

The `Ephemeris` class is a context manager - it automatically closes on exit:

```python
# Automatic cleanup
with Ephemeris("DE441.eph") as eph:
    state = eph.get_state(...)

# Or manual control
eph = Ephemeris("DE441.eph")
try:
    state = eph.get_state(...)
finally:
    eph.close()
```

## Constants and Enums

```python
from libjpl import JplTarget, JplStatus, JplByteOrder, JplReferenceFrame

# Target codes
JplTarget.MERCURY          # 1
JplTarget.EARTH            # 3
JplTarget.MOON             # 10
JplTarget.SUN              # 11
JplTarget.SOLAR_SYSTEM_BARYCENTER  # 12
JplTarget.EARTH_MOON_BARYCENTER    # 13

# Status codes
JplStatus.OK               # 0
JplStatus.ERROR_ARGUMENT   # 1
JplStatus.ERROR_IO         # 2
# ...

# Reference frames
JplReferenceFrame.UNKNOWN       # 0
JplReferenceFrame.DYNAMICAL_1950 # 1
JplReferenceFrame.DYNAMICAL_2000 # 2
JplReferenceFrame.ICRF          # 3
```

## Running the Example

```bash
# Build
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_PYTHON_BINDING=ON
cmake --build build --config Debug

# Run
python bindings/python/examples/planet_positions.py /path/to/DE441.eph
```