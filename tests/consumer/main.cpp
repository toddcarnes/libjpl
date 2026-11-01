// Copyright (c) 2026 Todd Carnes
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
// Installed-package C++ consumer (test-only, owned by T19). Mirrors main.c
// with <cmath>/std::isfinite and ordinary brace initialization. Includes
// all three public headers; never includes private headers.
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>
#include <cmath>
int main() {
    jpl_date tt{2451545.0, 0.125};
    jpl_date tdb{0.0, 0.0};
    double rotation[3][3]{};
    double au{0.0};
    if (jpl_tt_to_tdb(&tt, &tdb) != JPL_OK) return 1;
    if (jpl_frame_rotation_matrix(&tt, rotation) != JPL_OK) return 1;
    if (jpl_km_to_au(149597870.7, &au) != JPL_OK || au != 1.0) return 1;
    return !std::isfinite(tdb.hi) || !std::isfinite(rotation[0][0]);
}
