/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include <jpl/jpl.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ephemeris-file>\n";
        return 1;
    }

    try {
        jpl::Ephemeris eph(argv[1]);

        // Check ephemeris number (DE441 = 441) and overflow constant names (>400)
        auto hdr = eph.header();
        if (hdr.ephemeris_number != 441) {
            std::cerr << "Expected ephemeris number 441, got " << hdr.ephemeris_number << "\n";
            return 1;
        }
        if (hdr.constant_count <= 400 || hdr.constant_names.size() != static_cast<size_t>(hdr.constant_count)) {
            std::cerr << "Unexpected constant_names size: " << hdr.constant_names.size()
                      << " vs constant_count " << hdr.constant_count << "\n";
            return 1;
        }
        for (size_t idx : {size_t{0}, size_t{399}, size_t{400}, hdr.constant_names.size() - 1}) {
            if (hdr.constant_names[idx] != eph.constant_name(idx)) {
                std::cerr << "Constant name mismatch at " << idx << ": '"
                          << hdr.constant_names[idx] << "' vs '" << eph.constant_name(idx) << "'\n";
                return 1;
            }
        }

        // Read Earth-from-Sun at J2000.0
        const std::array<double, 2> jd_tdb = {2451545.0, 0.0};
        auto state = eph.get_state(jd_tdb, 3, 11);

        // Verify all six slots are finite
        for (int i = 0; i < 6; ++i) {
            if (!std::isfinite(state[i])) {
                std::cerr << "State[" << i << "] is not finite: " << state[i] << "\n";
                return 1;
            }
        }

        // Test frame conversions
        auto icrs_state = jpl::Ephemeris::ecliptic_to_icrs(state, {2451545.0, 0.0});
        for (int i = 0; i < 6; ++i) {
            if (!std::isfinite(icrs_state[i])) {
                std::cerr << "ICRS State[" << i << "] is not finite: " << icrs_state[i] << "\n";
                return 1;
            }
        }

        // Test time conversion
        auto jd_tdb_tt = jpl::Ephemeris::tt_to_tdb({2451545.0, 0.0});
        if (!std::isfinite(jd_tdb_tt[0]) || !std::isfinite(jd_tdb_tt[1])) {
            std::cerr << "TT->TDB conversion failed\n";
            return 1;
        }

        // Test convenience functions
        auto planet_state = eph.planet_position({2451545.0, 0.0}, 3, 11);
        for (int i = 0; i < 6; ++i) {
            if (!std::isfinite(planet_state[i])) {
                std::cerr << "Planet state[" << i << "] is not finite: " << planet_state[i] << "\n";
                return 1;
            }
        }

        auto moon_state = eph.moon_position({2451545.0, 0.0});
        for (int i = 0; i < 6; ++i) {
            if (!std::isfinite(moon_state[i])) {
                std::cerr << "Moon state[" << i << "] is not finite: " << moon_state[i] << "\n";
                return 1;
            }
        }

        // Apparent ecliptic position returns a five-field result struct.
        auto ecliptic = eph.apparent_ecliptic_position({2451545.0, 0.0}, 10); // Moon
        if (!(ecliptic.longitude_deg >= 0.0 && ecliptic.longitude_deg < 360.0)) {
            std::cerr << "Bad ecliptic longitude: " << ecliptic.longitude_deg << "\n";
            return 1;
        }
        if (!(std::abs(ecliptic.latitude_deg) <= 90.0)) {
            std::cerr << "Bad ecliptic latitude: " << ecliptic.latitude_deg << "\n";
            return 1;
        }
        if (!(ecliptic.range_au > 0.002 && ecliptic.range_au < 0.003)) {
            std::cerr << "Bad Moon range: " << ecliptic.range_au << "\n";
            return 1;
        }
        if (!std::isfinite(ecliptic.longitude_rate_deg_per_day) ||
            !std::isfinite(ecliptic.range_rate_km_per_s) ||
            std::abs(ecliptic.range_rate_km_per_s) >= 1.0) {
            std::cerr << "Bad ecliptic rates: " << ecliptic.longitude_rate_deg_per_day
                      << ", " << ecliptic.range_rate_km_per_s << "\n";
            return 1;
        }

        // Test aberration (observer velocity in AU/day; Earth orbital speed ~0.0172 AU/day)
        std::array<double, 6> geom_state = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        std::array<double, 3> obs_vel = {0.0, 0.0172, 0.0};
        auto aberrated = jpl::Ephemeris::apply_aberration(geom_state, obs_vel);
        for (int i = 0; i < 6; ++i) {
            if (!std::isfinite(aberrated[i])) {
                std::cerr << "Aberrated state[" << i << "] is not finite: " << aberrated[i] << "\n";
                return 1;
            }
        }

        // Contract §4: omitting the solar term equals an explicit 0.0.
        auto aberrated_zero = jpl::Ephemeris::apply_aberration_full(geom_state, obs_vel, 0.0);
        for (int i = 0; i < 6; ++i) {
            if (aberrated[i] != aberrated_zero[i]) {
                std::cerr << "Aberration 0.0 mismatch at " << i << "\n";
                return 1;
            }
        }

        // Contract §5: ecliptic round-trip reproduces the input.
        auto back_to_ecliptic = jpl::Ephemeris::icrs_to_ecliptic(icrs_state, {2451545.0, 0.0});
        for (int i = 0; i < 6; ++i) {
            if (std::fabs(back_to_ecliptic[i] - state[i]) > 1e-9) {
                std::cerr << "Ecliptic round-trip mismatch at " << i << "\n";
                return 1;
            }
        }

        // Test light time
        auto lt_result = eph.get_state_lighttime({2451545.0, 0.0}, 3, 11);
        if (!std::isfinite(lt_result.light_time_days) || lt_result.light_time_days <= 0) {
            std::cerr << "Light time computation failed\n";
            return 1;
        }

        // Test frame rotation matrix
        auto R = jpl::Ephemeris::frame_rotation_matrix({2451545.0, 0.0});
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (!std::isfinite(R[i][j])) {
                    std::cerr << "Rotation matrix[" << i << "][" << j << "] is not finite: " << R[i][j] << "\n";
                    return 1;
                }
            }
        }

        // Test time conversion round-trip
        auto jd_tt = jpl::Ephemeris::tdb_to_tt(jd_tdb);
        if (!std::isfinite(jd_tt[0]) || !std::isfinite(jd_tt[1])) {
            std::cerr << "TDB->TT conversion failed\n";
            return 1;
        }

        // Check that opening a missing path throws
        try {
            jpl::Ephemeris bad_eph("nonexistent_file.eph");
            std::cerr << "Expected exception for missing file\n";
            return 1;
        } catch (const jpl::JplError& ex) {
            if (ex.status() != JPL_ERROR_IO && ex.status() != JPL_ERROR_ARGUMENT) {
                std::cerr << "Wrong error status for missing file: " << ex.status() << "\n";
                return 1;
            }
        }

        std::cout << "C++ binding test passed\n";
        return 0;

    } catch (const jpl::JplError& ex) {
        std::cerr << "JplError: " << ex.what() << " (status=" << ex.status() << ")\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }
}
