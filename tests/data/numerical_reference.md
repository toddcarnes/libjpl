<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

/* Independent numerical-reference provenance (test-only).
 * Generated values live in tests/data/numerical_reference.h; this note
 * records how they were produced, checked, and consumed. The upstream
 * primitive (eraDtdb/eraEcm06/eraAb from vendored ERFA 2.0.1) is
 * independent of the libjpl wrapper, not a second physical model. Real
 * geometric DE/testpo comparisons remain the separate data check; no
 * unsupported Horizons/SPICE convention is compared here.
 *
 * Generation:
 *   cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF
 *   cmake --build build/remediation --config Debug --target jpl_generate_reference
 *   build/remediation/Debug/jpl_generate_reference --output <new-header-path>
 * The generator compiles upstream ERFA sources directly and never links
 * libjpl; it refuses to overwrite an existing output path. The checked-in
 * header was added through the file workflow after review, not by silent
 * overwrite. Reproducibility: two runs to distinct build-tree paths gave
 * identical canonical-LF SHA-256
 * 0a0f75214fcdc507ad15fa6b7874d7b9caab3c05874658934038085d2f1d3b26
 * (verified 2026-09-24; byte-identical outputs, values match the checked-in
 * header). Canonicalization: CRLF replaced with LF, no other bytes changed,
 * bare CR rejected. Generator source canonical-LF SHA-256
 * dfa98d6d97a066fa80e733ef8766bbb35cba0dd7c931e1d95955aa7b3f589955.
 * Both hashes are pinned in tests/data/MANIFEST.txt
 * (numerical.reference.sha256 / numerical.generator.sha256) and enforced
 * by CTest jpl_numerical_reference_integrity.
 *
 * Upstream anchor (ERFA t_erfa_c.c, t_dtdb):
 *   eraDtdb(2448939.5, 0.123, 0.76543, 5.0123, 5525.242, 3190.0)
 *   = -0.1280368005936998991e-2 (tolerance 1e-15).
 * The generator aborts when this anchor fails, so a misconfigured
 * upstream cannot silently seed the header. ERFA version/source:
 * vendored ERFA v2.0.1 (SOFA 20231011) under src/erfa/, unmodified.
 *
 * Contents: 5 time references (TT pair, eraDtdb geocentric correction in
 * seconds, direct eraEcm06 ICRS-to-ecliptic matrix) at
 * {2451545.0,0.0}, {2451620.5,0.0}, {2451800.5,0.0},
 * {-3100000.5,0x1p-40}, {8000000.5,0x1p-40}; 2 apparent references (TT
 * pair, analytic linear-mock state, delay days) with the mock documented
 * in the header comment. Distant-epoch vectors verify
 * arithmetic/convention, not claimed physical model accuracy there.
 * Beyond-legacy epochs (the two distant rows) carry no supported
 * physical-model claim: they pin pair arithmetic and matrix convention
 * only, and no Horizons/SPICE convention is compared here.
 *
 * Consumption: ordinary tests include the checked-in header and compare
 * API results to the pinned arrays; they never regenerate them. Date
 * differences are compared as pairs: the recovered correction seconds
 * through (output[0]-input[0]) + (output[1]-input[1]), not through
 * subtraction of two pre-summed full JDs. The analytic light-time
 * residual |tau - range/c| <= 1e-12 d was cross-checked at generation
 * before the values were accepted.
 */
