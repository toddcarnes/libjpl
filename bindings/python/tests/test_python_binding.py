#!/usr/bin/env python3
# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

"""Python cffi binding test for libjpl."""
import sys
import os

# Add the bindings/python/src directory to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

try:
    import cffi
except ImportError:
    print("SKIP: cffi not available")
    sys.exit(0)

from libjpl import (
    Ephemeris, JplError, JplStatus, JplTarget, JplByteOrder,
    JplReferenceFrame, open as jpl_open, open_mmap, status_string,
)
import gc
import math

def test_binding():
    fixture_path = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'tests', 'data', 'de441_2000_2010.eph')

    # Test successful open and get_state
    with Ephemeris(fixture_path) as eph:
        state = eph.get_state([2451545.0, 0.0], 3, 11)
        assert len(state) == 6, f"Expected 6 elements, got {len(state)}"
        for v in state:
            assert math.isfinite(v), f"Non-finite value in state: {v}"

    # Header metadata must come from the native struct, not defaults.
    with Ephemeris(fixture_path) as eph:
        header = eph.header
        assert header.ephemeris_number == 441, header.ephemeris_number
        assert header.constant_count > 400, header.constant_count
        names = header.constant_names
        assert len(names) == header.constant_count, len(names)
        for idx in (0, 399, 400, header.constant_count - 1):
            assert names[idx] == eph.constant_name(idx), (idx, names[idx], eph.constant_name(idx))
        assert status_string(JplStatus.OK) == "success"
        assert header.start_jd[0] < header.end_jd[0], header.start_jd
        assert len(header.titles) == 3, header.titles
        assert all(header.titles), header.titles
        assert 1.49597870e8 <= header.astronomical_unit_km <= 1.49597871e8, \
            header.astronomical_unit_km
        assert 81.30 <= header.earth_moon_mass_ratio <= 81.31, \
            header.earth_moon_mass_ratio
        assert header.pointers[3].subintervals > 0, header.pointers[3].subintervals

        constants = eph.constants()
        assert len(constants) == header.constant_count, len(constants)
        assert all(math.isfinite(v) for v in constants)

        # Apparent ecliptic position writes a result struct, so a misplaced
        # declaration shows up as garbage or a crash, not a wrong pointer.
        pos = eph.apparent_ecliptic_position([2451545.0, 0.0], 10)  # Moon
        assert 0.0 <= pos['longitude_deg'] < 360.0, pos['longitude_deg']
        assert abs(pos['latitude_deg']) <= 90.0, pos['latitude_deg']
        assert 0.002 < pos['range_au'] < 0.003, pos['range_au']
        assert math.isfinite(pos['longitude_rate_deg_per_day'])
        assert math.isfinite(pos['range_rate_km_per_s'])
        assert abs(pos['range_rate_km_per_s']) < 1.0, pos['range_rate_km_per_s']
        saved_header = header
    # Header must remain safe to access after the Ephemeris is closed/GC'd.
    gc.collect()
    assert saved_header.ephemeris_number == 441
    assert saved_header.constant_count > 400

    # Calling methods on a closed handle must raise JplError, not cffi TypeError.
    closed_eph = Ephemeris(fixture_path)
    closed_eph.close()
    try:
        closed_eph.get_state([2451545.0, 0.0], 3, 11)
        assert False, "Expected JplError on closed handle"
    except JplError as e:
        assert e.status == JplStatus.ERROR_ARGUMENT, e.status

    # Aberration helpers must work as static methods (no Ephemeris instance needed).
    ab = Ephemeris.apply_aberration([1.0, 0.0, 0.0, 0.0, 0.0, 0.0], [0.0, 0.0172, 0.0])
    assert len(ab) == 6 and all(math.isfinite(v) for v in ab)
    ab_full = Ephemeris.apply_aberration_full([1.0, 0.0, 0.0, 0.0, 0.0, 0.0], [0.0, 0.0172, 0.0], 1.0)
    assert len(ab_full) == 6 and all(math.isfinite(v) for v in ab_full)

    # Contract §4: omitting the solar term is identical to passing 0.0.
    ab_zero = Ephemeris.apply_aberration_full([1.0, 0.0, 0.0, 0.0, 0.0, 0.0], [0.0, 0.0172, 0.0], 0.0)
    assert ab == ab_zero, (ab, ab_zero)
    # Contract §5: ecliptic round-trip reproduces the input.
    icrs = Ephemeris.ecliptic_to_icrs([1.0, 0.0, 0.0, 0.0, 0.0, 0.0], [2451545.0, 0.0])
    back = Ephemeris.icrs_to_ecliptic(icrs, [2451545.0, 0.0])
    for got, want in zip(back, [1.0, 0.0, 0.0, 0.0, 0.0, 0.0]):
        assert abs(got - want) < 1e-9, (got, want)
    # rotate_vector matches the state entry's position half.
    rv = Ephemeris.rotate_vector([1.0, 0.0, 0.0], [2451545.0, 0.0])
    for got, want in zip(rv, icrs[:3]):
        assert abs(got - want) < 1e-12, (got, want)

    # Test missing path raises JplError and GC does not fail in __del__
    try:
        Ephemeris("nonexistent_file.eph")
        assert False, "Expected JplError for missing file"
    except JplError as e:
        assert e.status != 0, f"Expected non-zero status, got {e.status}"
    gc.collect()

    print("Python binding test passed")

if __name__ == "__main__":
    test_binding()
