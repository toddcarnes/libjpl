/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/**
 * @file planet_positions.cpp
 * @brief Example: Print all planet positions at J2000.0 (2000-01-01 12:00 TT)
 *
 * Build:
 *   Compile against the installed or build-tree libjpl headers and jpl static library
 *   (C++17), e.g. with g++/clang++ or cl.exe including bindings/cpp/include and include/.
 *
 * Run:
 *   ./planet_positions /path/to/DE441.eph
 */

#include <jpl/jpl.hpp>
#include <iostream>
#include <iomanip>
#include <array>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ephemeris-file>\n";
        return 1;
    }

    try {
        // Open ephemeris file
        jpl::Ephemeris eph(argv[1]);

        // J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
        // Convert to TDB for ephemeris queries
        const std::array<double, 2> jd_tt = {2451545.0, 0.0};
        auto jd_tdb = jpl::Ephemeris::tt_to_tdb(jd_tt);

        std::cout << "Planet positions at J2000.0 (2000-01-01 12:00 TT)\n";
        std::cout << "TDB: " << jd_tdb[0] << " + " << jd_tdb[1] << "\n\n";

        // Planet target codes (from jpl.h)
        struct Planet { int code; const char* name; };
        const Planet planets[] = {
            {1, "Mercury"}, {2, "Venus"}, {3, "Earth"}, {4, "Mars"},
            {5, "Jupiter"}, {6, "Saturn"}, {7, "Uranus"}, {8, "Neptune"},
            {9, "Pluto"}, {10, "Moon"}, {11, "Sun"}, {12, "SSB"},
            {13, "EMB"}, {14, "Nutations"}, {15, "Librations"}, {17, "TT-TDB"}
        };

        std::cout << std::fixed << std::setprecision(6);
        std::cout << std::left << std::setw(12) << "Body"
                  << std::setw(14) << "X (AU)" << std::setw(14) << "Y (AU)"
                  << std::setw(14) << "Z (AU)" << std::setw(14) << "VX (AU/d)"
                  << std::setw(14) << "VY (AU/d)" << std::setw(14) << "VZ (AU/d)" << "\n";
        std::cout << std::string(98, '-') << "\n";

        for (const auto& p : planets) {
            if (p.code == 0) continue; // skip SSB alias
            if (p.code > 17) continue; // skip invalid

            try {
                // Get position relative to SSB (center=0) using TDB
                auto state = eph.get_state(
                    jd_tdb,  // J2000.0 TDB
                    p.code,            // target
                    0                  // center = SSB
                );

                std::cout << std::left << std::setw(12) << p.name
                          << std::setw(14) << std::fixed << std::setprecision(6) << state[0]
                          << std::setw(14) << state[1]
                          << std::setw(14) << state[2]
                          << std::setw(14) << state[3]
                          << std::setw(14) << state[4]
                          << std::setw(14) << state[5] << "\n";
            } catch (const jpl::JplError& ex) {
                std::cerr << "Error getting state for " << p.name
                          << ": " << ex.what() << " (status=" << ex.status() << ")\n";
            }
        }

        // Also show Earth relative to Sun
        std::cout << "\nEarth relative to Sun:\n";
        try {
            auto state = eph.get_state(jd_tdb, JPL_EARTH, JPL_SUN);
            std::cout << std::fixed << std::setprecision(6)
                      << "X: " << std::setw(14) << state[0]
                      << " Y: " << state[1] << " Z: " << state[2]
                      << " VX: " << state[3] << " VY: " << state[4] << " VZ: " << state[5] << "\n";
        } catch (const jpl::JplError& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
        }

    } catch (const jpl::JplError& ex) {
        std::cerr << "JplError: " << ex.what() << " (status=" << ex.status() << ")\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
