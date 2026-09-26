/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
/* Installed-package C consumer (test-only). Includes all
 * three public headers and references symbols that pull math/ERFA objects
 * from the installed static archive. Never includes private headers. */
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>
#include <math.h>
int main(void) {
    const double tt[2] = {2451545.0, 0.125};
    double tdb[2], rotation[3][3], au;
    if (jpl_tt_to_tdb(tt, tdb) != JPL_OK) return 1;
    if (jpl_frame_rotation_matrix(tt, rotation) != JPL_OK) return 1;
    if (jpl_km_to_au(149597870.7, &au) != JPL_OK || au != 1.0) return 1;
    return !isfinite(tdb[0]) || !isfinite(rotation[0][0]);
}
