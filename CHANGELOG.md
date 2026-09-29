<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.5.0] - 2026-09-29

### Added

- Shared library target `jpl_shared` with versioned SONAME (`VERSION`/`SOVERSION`) and Windows `VERSIONINFO` resource.
- Export header `jpl_export.h` with `JPL_API` macro for cross-platform DLL import/export.
- C++ header-only RAII wrapper (`bindings/cpp/`): `jpl::Ephemeris`, `jpl::JplError`, convenience helpers.
- Python cffi binding (`bindings/python/`): `libjpl.Ephemeris` context manager, `libjpl.JplError`.
- C# P/Invoke binding (`bindings/csharp/`): `LibJpl.JplEphemeris : SafeHandle`, `LibJplApi.Open/OpenMmap`, enums `JplStatus`/`JplTarget`/`JplByteOrder`/`JplReferenceFrame`.
- VB.NET P/Invoke binding (`bindings/vbnet/`): mirrors C# API, mirrors `SafeHandle` wrapper.
- CMake options `LIBJPL_BUILD_SHARED`, `LIBJPL_BUILD_CPP_BINDING`, `LIBJPL_BUILD_PYTHON_BINDING`, `LIBJPL_BUILD_CSHARP_BINDING`, `LIBJPL_BUILD_VBNET_BINDING` (all default `OFF` except shared).
- License header enforcement for new files via `jpl_licenses` test.
- Usage examples for all bindings: C (`bindings/c/examples/`), C++ (`bindings/cpp/examples/`), Python (`bindings/python/examples/`), C# (`bindings/csharp/examples/`), VB.NET (`bindings/vbnet/examples/`).

### Changed

- Public headers `jpl.h`, `jpl_frames.h`, `jpl_convenience.h` now include `jpl_export.h` and prefix all public declarations with `JPL_API`.
- Library install layout: static archive to `${CMAKE_INSTALL_LIBDIR}/static`, shared library to `${CMAKE_INSTALL_LIBDIR}/shared` and `${CMAKE_INSTALL_BINDIR}/shared`.
- C++ binding installs headers to `${CMAKE_INSTALL_INCLUDEDIR}/jpl/`.
- `jpl_install` test updated for split static/shared layout.

### Fixed

- `jpl_install` test: POSIX shared-link check now references `${LIBDIR}/static/libjpl.a`.
- License checker now exempts `build_debug/`, `build_release/`, `obj/`, `bin/` build directories.
- C#/VB.NET wrapper scripts copy `libjpl.so`/`jpl.dll` to test output directory for Linux/macOS.

## [0.4.0] - 2026-09-26

### Added

- Added `jpl_get_constant_name()` to retrieve every DE constant name,
  including names beyond the first 400 stored in `jpl_header`.
- Added `jpl_apparent_ecliptic_position()` for geocentric apparent
  true-of-date longitude, latitude, range, signed longitude speed, and
  Horizons-style radial range-rate.
- Pinned 220 DE441 Horizons observer rows for offline and optional local
  full-file comparisons of apparent positions and motion.
- Added a grouped Doxygen API landing page.

### Fixed

- Kept private parser and light-time test hooks out of the installed static
  library. Install tests now inspect defined archive symbols.
- Excluded ignored `.worktrees/` checkouts from the license inventory sweep
  so installed-package verification works from the main checkout too.
- Replaced Python-dependent fixture mutation and integrity checks with a
  C17 test tool that canonicalizes CRLF byte-for-byte and rejects bare CR.
- Guarded record/subinterval quotient-to-integer casts before conversion.
- Clarified the single-leg Moon apparent-position contract and the valid
  TT-TDB record-width boundary; removed an unreachable km-to-AU error path.

## [0.3.0] - 2026-09-25

### Changed

- Two-part Julian dates are now required across the public API
  (**BREAKING CHANGE**, no scalar wrapper): every date parameter is
  `const double jd[2]`, coverage fields are `double start_jd[2]` /
  `double end_jd[2]`, and TT/TDB/frame/aberration/unit helpers return
  `jpl_status` with pair outputs. Build pairs directly (day plus
  fraction); splitting an already-rounded scalar is not supported. See
  `docs/migration-two-part-dates.md`.
- Public body states and unit helpers use the exact modern AU
  (`149597870.7 km`); `header.astronomical_unit_km` stays original
  metadata. Light speed is derived from exact SI values.
- ICRS convenience (`jpl_planet_position`, `jpl_moon_position`) requires
  `JPL_FRAME_ICRF` (`jpl_header.reference_frame`); legacy/unknown frames
  return `JPL_ERROR_UNSUPPORTED` with native core reads unaffected.
- Computed outputs are transactional (unchanged on failure); raw
  record/constant buffers may be partially written after I/O errors.
- Generated Doxygen HTML+XML with a checked public-symbol inventory
  (`jpl_docs_symbols`) and source/build paths-with-spaces coverage
  (`jpl_docs_paths`); README examples compile and run as `jpl_examples`.

### Fixed

- F01: validator corpus generated once per run via a CTest fixture setup;
  repeated parallel execution no longer truncates shared files.
- F02: trailing records kept out of date selection (advertised vs physical
  counts); coverage-end queries select the final covered interval.
- F03: layout validation without payload-value heuristics.
- F04/F12: bodies validated before dates/data access; NUMERIC preserved
  for finite unrepresentable date pairs.
- F05: scale-safe aberration output normalization.
- F06: bounded header reads for wide records (no file-controlled open-time
  allocations).
- F07: transactional fetch-cache publication with rollback.
- F08: maintainer-local workflow gated on dedicated runner data and local
  suite registration.
- F09: package identity 0.3.0 (exact-version compatibility).
- F10: reference component-coverage requirements for testpo files.
- F11: full-record ephemeris sweep scan.
- F13/F14: magnitude-aware endpoint tolerance; allocator-independent
  zero-length reads.
- F15: docs checks registered with docs ON even when tests are OFF.
- F16: canonical-LF numerical-reference provenance hashes enforced.
- F17: immutable action pins and read-only workflow permissions.
- F18: live documentation aligned (ranges, links, fetch/workflow/sweep/
  package/test guidance).

## [0.2.0] - 2026-09-19

### Added
- TT-TDB dataset support: parse the header pointer, report dataset presence, and expose the dataset from time-segmented ephemerides with record-geometry span validation.
- Committed DE441 2000-2010 fixture with manifest, validated with zero network.
- Opt-in JPL download tier: network test data gated behind a fetch option, with integration suites wired to fetched data.

### Changed
- Body codes Earth=3 / EMB=13 per JPL PLEPH convention (**BREAKING CHANGE**: raw-integer callers swap 3/13).
- Test harness: single testpo runner with the manifest as single source, fixture provenance checks and SRS regression coverage; testpo suites registered for time-segmented DE variants.

### Fixed
- Light-time iteration uses the center epoch; observer velocity uses the full aberration term at the reception epoch.
- SRS relativistic constant corrected.
- Overflow and record-offset guard wraps closed, including the TT-TDB tail parse.
- Mmap reads lock-free and shareable (FILE path cached per handle); POSIX same-fd size recheck.
- Explicit test checks, off-J2000 golden vector, scoped libration envelope, Release-safe mocks, ASCII-only build comments.
