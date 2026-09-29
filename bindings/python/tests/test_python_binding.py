# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

#!/usr/bin/env python3
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

from libjpl import Ephemeris, JplError
import math

def test_binding():
    fixture_path = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'tests', 'data', 'de441_2000_2010.eph')
    
    # Test successful open and get_state
    with Ephemeris(fixture_path) as eph:
        state = eph.get_state([2451545.0, 0.0], 3, 11)
        assert len(state) == 6, f"Expected 6 elements, got {len(state)}"
        for v in state:
            assert math.isfinite(v), f"Non-finite value in state: {v}"
    
    # Test missing path raises JplError
    try:
        Ephemeris("nonexistent_file.eph")
        assert False, "Expected JplError for missing file"
    except JplError as e:
        assert e.status != 0, f"Expected non-zero status, got {e.status}"
    
    print("Python binding test passed")

if __name__ == "__main__":
    test_binding()