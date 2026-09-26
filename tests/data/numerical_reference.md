<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Independent numerical references

The pinned values live in `tests/data/numerical_reference.h`. The generator
calls `eraDtdb`, `eraEcm06` and `eraAb` from vendored ERFA v2.0.1 directly;
it does not link libjpl. These checks are independent of libjpl's wrappers,
but ERFA is not a second physical model. Real geometric DE/testpo comparisons
are a separate data check. No Horizons/SPICE convention is compared here.

## Generation and integrity

```sh
cmake -S . -B build/reference -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/reference --config Debug --target jpl_generate_reference
build/reference/Debug/jpl_generate_reference --output <new-header-path>
```

The executable is under `build/reference/Debug/` with multi-config generators
and directly under `build/reference/` with single-config generators. It
refuses to overwrite an existing output. Review a new result before replacing
the checked-in header.

Two runs to distinct build-tree paths gave byte-identical output matching
the checked-in header (verified 2026-09-24). Canonical-LF SHA-256 is
`0a0f75214fcdc507ad15fa6b7874d7b9caab3c05874658934038085d2f1d3b26`
for the header and
`dfa98d6d97a066fa80e733ef8766bbb35cba0dd7c931e1d95955aa7b3f589955`
for `tools/generate_numerical_reference.c`. Canonicalization replaces CRLF
with LF only and rejects bare CR. `tests/data/MANIFEST.txt` pins both hashes
as `numerical.reference.sha256` and `numerical.generator.sha256`;
`jpl_numerical_reference_integrity` checks them in the offline suite.

The generator checks the anchor from ERFA's `t_erfa_c.c` (`t_dtdb`):
`eraDtdb(2448939.5, 0.123, 0.76543, 5.0123, 5525.242, 3190.0)` equals
`-0.1280368005936998991e-2` within `1e-15`. It aborts if the anchor fails.
The bundled source is ERFA v2.0.1 (SOFA 20231011) under `src/erfa/`.

## What the references check

The header contains five TT-pair references with geocentric `eraDtdb`
corrections in seconds and direct `eraEcm06` ICRS-to-ecliptic matrices, at
`{2451545.0,0.0}`, `{2451620.5,0.0}`, `{2451800.5,0.0}`,
`{-3100000.5,0x1p-40}` and `{8000000.5,0x1p-40}`. Two apparent-place
references contain a TT pair, analytic linear-mock state and light-time delay
in days; the mock is documented in the header. The distant epochs check pair
arithmetic and matrix convention, **not** physical-model accuracy there.

Ordinary tests compare API results with the checked-in arrays, without
regenerating them. They recover date corrections from the pairs using
`(output[0]-input[0]) + (output[1]-input[1])`, not subtraction of pre-summed
Julian dates. The analytic light-time residual
`|tau - range/c| <= 1e-12 d` was checked before accepting the references.
