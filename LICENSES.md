<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Licenses and third-party notices

## Project license

`libjpl` first-party code and documentation are licensed under the MIT
License, copyright Todd Carnes (2026). The complete grant lives in
[`LICENSE`](LICENSE); inventoried first-party source, header, test, script,
build, and documentation files carry a short copyright notice plus
`SPDX-License-Identifier: MIT` in their native comment syntax. The
enumerated file list is `tests/license_inventory.cmake` (the list
itself is the single source of truth for the count; no duplicated
count lives here), enforced by
`tests/cmake/check_licenses.cmake` (CTest `jpl_licenses`).

Git history verifies 2026 as the first-party creation year. First-party
notices use that year; no earlier year is claimed.

## Third-party: ERFA

Frame/time routines link vendored ERFA v2.0.1 (derived from the IAU SOFA
library) under `src/erfa/`, copyright NumFOCUS Foundation, BSD-3-Clause.
Its complete terms are in `src/erfa/LICENSE`, which is inspected but
never modified. Both texts install unconditionally to
`${CMAKE_INSTALL_DATADIR}/licenses/libjpl/` as `LICENSE` (project MIT)
and `ERFA-LICENSE` (ERFA BSD-3). `jpl_install` passes the configured
data directory to the license checker, so custom layouts are checked
without assuming `share`.

## Test data provenance

- The committed fixture (`tests/data/de441_2000_2010.eph`), reference
  slice (`tests/data/testpo_441_slice.txt`), and identity list
  (`tests/data/fixture_rows.txt`) derive from JPL DE441 / `testpo.441`
  material. They are third-party reference data: they carry no MIT
  header (comments would break byte hashes and parsers) and are pinned
  by hash in `tests/data/MANIFEST.txt`. Full source/tool provenance is
  recorded in `tests/data/PROVENANCE.md`.
- Opt-in downloads (DE440/DE440t binaries, `testpo.440[t]`) come from
  JPL under the fetch tier; upstream terms stay with the upstream
  files. Download URLs, sizes, and hashes live only in
  `tests/data/MANIFEST.txt`.
- Generated build trees carry no notices. Generated first-party source
  artifacts (currently `tests/data/numerical_reference.h`) carry the
  generator-emitted MIT notice; the generator
  (`tools/generate_numerical_reference.c`) refuses to overwrite tracked
  truth.

## Exemptions

Full license texts (`LICENSE`, `src/erfa/LICENSE`), non-commentable
artifacts (binary fixture, authoritative reference data, hashed
identity list), vendored `src/erfa/**`, and verbatim historical records
(`docs/superpowers/`, `docs/reviews/`) are exempt from per-file
notices for the stated reasons. The exemption list is explicit in
`tests/license_inventory.cmake`, not a blanket exclusion.
