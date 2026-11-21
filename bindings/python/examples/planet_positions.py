#!/usr/bin/env python3
# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
"""
Example: Print all planet positions at J2000.0 (2000-01-01 12:00 TT)

Usage:
    python planet_positions.py /path/to/DE441.eph

Requires:
    pip install cffi
"""

import sys
import os

# Add the libjpl package to path (adjust as needed for your environment)
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from libjpl import Ephemeris, JplTarget, JplStatus

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <ephemeris-file>")
        return 1

    eph_path = sys.argv[1]

    try:
        # Open ephemeris using context manager (auto-closes)
        with Ephemeris(eph_path) as eph:
            # J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
            jd_tt = [2451545.0, 0.0]

            # Convert TT to TDB for ephemeris queries
            jd_tdb = Ephemeris.tt_to_tdb([2451545.0, 0.0])

            print("Planet positions at J2000.0 (2000-01-01 12:00 TT)")
            print()

            # Planet target codes (matching jpl.h JplTarget enum)
            planets = {
                1: "Mercury", 2: "Venus", 3: "Earth", 4: "Mars",
                5: "Jupiter", 6: "Saturn", 7: "Uranus", 8: "Neptune",
                9: "Pluto", 10: "Moon", 11: "Sun", 12: "SSB",
                13: "EMB", 14: "Nutations", 15: "Librations"
            }

            print(f"{'Body':<12} {'X (AU)':>14} {'Y (AU)':>14} {'Z (AU)':>14} "
                  f"{'VX (AU/d)':>14} {'VY (AU/d)':>14} {'VZ (AU/d)':>14}")
            print("-" * 98)

            for code, name in sorted(planets.items()):
                if code == 0 or code > 17:
                    continue
                try:
                    # Use TDB for ephemeris queries
                    state = eph.get_state(jd_tdb, code, 0)
                    print(f"{name:<12} {state[0]:>14.6f} {state[1]:>14.6f} {state[2]:>14.6f} "
                          f"{state[3]:>14.6f} {state[4]:>14.6f} {state[5]:>14.6f}")
                except Exception as e:
                    print(f"{name:<12} Error: {e}")

            # Earth relative to Sun
            print("\nEarth relative to Sun:")
            state = eph.get_state(jd_tdb, JplTarget.EARTH, JplTarget.SUN)
            print(f"  X: {state[0]:>14.6f} Y: {state[1]:>14.6f} Z: {state[2]:>14.6f}")
            print(f"  VX: {state[3]:>14.6f} VY: {state[4]:>14.6f} VZ: {state[5]:>14.6f}")

            # Moon position
            print("\nMoon position (Earth-centered, ICRS):")
            moon = eph.moon_position(jd_tt)
            print(f"  X: {moon[0]:>14.6f} Y: {moon[1]:>14.6f} Z: {moon[2]:>14.6f}")
            print(f"  VX: {moon[3]:>14.6f} VY: {moon[4]:>14.6f} VZ: {moon[5]:>14.6f}")

            # Frame conversions (ICRS -> mean ecliptic of date)
            print("\nFrame conversion (ICRS -> Ecliptic):")
            state_ecl = Ephemeris.icrs_to_ecliptic(state, jd_tt)
            print(f"  Ecliptic: X={state_ecl[0]:.6f} Y={state_ecl[1]:.6f} Z={state_ecl[2]:.6f}")

            # Light time
            lt_result = eph.get_state_lighttime(jd_tdb, JplTarget.EARTH, JplTarget.SUN)
            print(f"\nLight time (Earth-Sun): {lt_result[1]:.6f} days")

            # Time conversions
            print("\nTime conversions:")
            jd_tdb = Ephemeris.tt_to_tdb([2451545.0, 0.0])
            jd_tt = Ephemeris.tdb_to_tt(jd_tdb)
            print(f"  TT->TDB: {jd_tdb[0]} + {jd_tdb[1]}")
            print(f"  TDB->TT: {jd_tt[0]} + {jd_tt[1]}")

    except Exception as e:
        print(f"Error: {e}")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())