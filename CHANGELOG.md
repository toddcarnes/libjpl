<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed

- Normalized-date discipline is now documented in one place (`src/jpl_private.h`): public entries normalize user input once via `jpl_date_normalize` at the trust boundary; internals consume normalized pairs, with `add_core()` asserting the contract in debug builds (`JPL_ASSERT_NORMALIZED`). Added a normalization-idempotence test. No behavior change (audit found no redundant or missing normalizations).
- Deleted `jpl_ecliptic_to_icrs` (a pure duplicate of the frame-context path); thinned `jpl_icrs_to_ecliptic`, `jpl_frame_rotation_matrix`, and context creation onto a single internal matrix computer. All in-repo and binding callers migrated to the context API; no behavior change.

- Interpolation internals now thread an `interp_work` bundle through a single `jpl_evaluate()` seam instead of seven helpers with five-plus parameters each; the `interp_select_record` passthrough was folded into its caller. No behavior change.
- New atomic `jpl_geometry` (`rel_state`, `light_time_days`, `observer_vel`, `sun_vec`, `emission`) built once by `jpl_compute_geometry()` in coverage-then-read order; both convenience call sites consume its named fields instead of threading three epochs by hand (replacing `observer_geometry()` and the inline duplicate).
- Layout offsets now have a single source of truth: `JPL_HDR_IPT_OFFSET`/`JPL_HDR_IPT_ENTRY_BYTES` name the IPT table, and `jpl_tail_offset()` centralizes the overflow-name tail computation shared by the layout validator, the overflow-name reader, and the test mock builders (which now use the same constants). `parse_header` asserts its cursor against the named offsets.
- Apparent-place pipeline now threads struct-typed stages: the internal `jpl_direction` (finite unit vector; `jpl_direction_from_raw` normalizes once) gates the solar-deflection stage, replacing both manual `/distance` renormalizations, and the longitude-rate policy (central/one-sided/adaptive-`h` finite differences) is extracted as `jpl_estimate_longitude_rate` over a sampler callback, unit-tested with a synthetic sampler and no ephemeris data. No behavior change (Horizons goldens byte-identical).

### Fixed

- Fixed solar gravitational light deflection in `jpl_apparent_ecliptic_position()`: `eraLd()` previously received `observer->Sun` and `Sun->observer` as `q` and `e` (making `e x q == 0`, a no-op); it now passes `Sun->target` as `q` and `Sun->observer` as `e`, with a near-solar-limb regression test.
- Fixed C17 undefined behavior in `jpl_apply_aberration()` (`(&state->x)[i]` replaced with `pos[i]`) and removed the `(double *)state` cast in `jpl_get_state()` for nutations/librations; added a portable byte-swap fallback in `read_f64()`.
- Made `jpl_frame_context *context` `const` in `jpl_frame_context_rotate_{position,velocity,state}` and documented the ecliptic-of-date-to-ICRS rotation direction.
- Fixed C# and VB.NET P/Invoke ABI for `const jpl_date *` (`in JplDate` / `ByRef JplDateNative`) and `jpl_status_string` (`IntPtr` + `Marshal.PtrToStringUTF8` so static C strings are never freed).
- Fixed C++ and Python `header.constant_names` out-of-bounds reads on ephemerides with >400 constants by reading overflow names via `jpl_get_constant_name()`; fixed Python `Ephemeris.__del__` on failed open, `JplHeader` lifetime after close/GC, and top-level `__init__.py` exports.
- Fixed compile/syntax/unit errors in all four language binding examples (`planet_positions.{cpp,py}` and `PlanetPositions.{cs,vb}`).
- Removed `#cmakedefine JPL_BUILDING_SHARED` from `cmake/jpl_export.h.in` and guarded SDK HTML doc copy on `DOXYGEN_FOUND`.
- Added v0.7.0 public symbols (`jpl_date`, `jpl_state`, `jpl_frame_context*`) to `docs/api.dox` and `tests/public_symbols.txt`, removed duplicate `docs/superpowers/` tree, and fixed stale/corrupted documentation entries.
- Removed accidentally committed `tests/test_convenience.c.bak` scratch copy.

## [0.7.0] - 2026-11-01

### Changed

- **BREAKING**: Type-safe public API: dates are now `jpl_date {hi, lo}` structs and states are `jpl_state {x, y, z, vx, vy, vz}` structs instead of `double[2]`/`double[6]` arrays. All public entry points (`jpl_get_state`, `jpl_get_state_lighttime`, `jpl_apply_aberration`, `jpl_tt_to_tdb`, `jpl_tdb_to_tt`, `jpl_get_tt_minus_tdb`, `jpl_ecliptic_to_icrs`, `jpl_icrs_to_ecliptic`, `jpl_frame_rotation_matrix`, `jpl_planet_position`, `jpl_moon_position`, `jpl_apparent_ecliptic_position`) take pointers to these structs. Internal `double[2]`/`double[6]` helpers remain in `jpl_private.h` (convert at the boundary with `jpl_date_to_pair`/`jpl_pair_to_date`/`jpl_state_to_array`/`jpl_array_to_state`).

### Fixed

- Gravitational light-deflection direction in `jpl_apparent_ecliptic_position()`: the source unit vector was built from the x-component alone, collapsing every apparent direction onto the (1,1,1) diagonal (up to ~138 deg error vs Horizons); the full state vector is now normalized.
- Added missing trailing newlines flagged by Clang `-Wnewline-eof` (whitespace only, no behavior change).

## [0.6.0] - 2026-10-31

### Changed

- **BREAKING**: Unified aberration API: removed `jpl_apply_aberration_full()`; `jpl_apply_aberration()` now takes `sun_distance_au` parameter (>0 includes solar term, ≤0 omits it). All callers updated.
- **BREAKING**: `jpl_apply_aberration()` signature changed from 3 to 4 arguments; `jpl_apply_aberration_full()` removed from public API and bindings.
- Internal test helpers (`jpl_date_*`, `jpl_icrs_to_true_ecliptic_position`, `jpl_vec_norm3`) moved from `jpl_private.h` to `jpl_testing.h` (guarded by `LIBJPL_TESTING`).
- Windows path resolution in `jpl_ephemeris_clone()` now uses dynamic `GetFullPathNameA` allocation (no `_MAX_PATH` limit).

### Fixed

- Light-time solver: budget exhaustion in `lt_fast_iterate` correctly sets `JPL_ERROR_CONVERGENCE`.
- Added `JPL_TEST_ASSERT` macro for programming errors in test helpers (distinct from `JPL_TEST_CHECK`).
- Added debug assertion to `jpl_date_compare()` for normalized input precondition.
- `jpl_private.h` no longer exposes internal declarations to downstream consumers.

### Added

- Windows extended-length path support (>260 chars) for `jpl_ephemeris_clone()` reopen path.

### Dependencies

- ERFA version still 2.0.1 (SOFA_VERSION 20231011).

## [0.5.0] - 2026-09-29

### Added

- Shared library target `jpl_shared` with versioned SONAME (`VERSION`/`SOVERSION`) and Windows `VERSIONINFO` resource.
- Export header `jpl_export.h` with `JPL_API` macro for cross-platform DLL import/export.
- C++ header-only RAII wrapper (`bindings/cpp/`): `jpl::Ephemeris`, `jpl::JplError`, members of `jpl::Ephemeris`: `planet_position`, `moon_position`, `tt_to_tdb`).
- Python cffi binding (`bindings/python/`): `libjpl.Ephemeris` context manager, `libjpl.JplError`.
- C# P/Invoke binding (`bindings/csharp/`): `LibJpl.JplEphemeris : SafeHandle`, `LibJplApi.Open/OpenMmap`, enums `JplStatus`/`JplTarget`/`JplByteOrder`/`JplReferenceFrame`.
- VB.NET P/Invoke binding (`bindings/vbnet/`): mirrors C# API, mirrors `SafeHandle` wrapper.
- CMake options `LIBJPL_BUILD_SHARED`, `LIBJPL_BUILD_CPP_BINDING`, `LIBJPL_BUILD_PYTHON_BINDING`, `LIBJPL_BUILD_CSHARP_BINDING`, `LIBJPL_BUILD_VBNET_BINDING` (all default `OFF` except shared).
- License header enforcement for new files via `jpl_licenses` test.
- Usage examples for all bindings: C (`bindings/c/examples/`), C++ (`bindings/cpp/examples/`), Python (`bindings/python/examples/`), C# (`bindings/csharp/examples/`), VB.NET (`bindings/vbnet/examples/`).

### Changed

- Public headers `jpl.h`, `jpl_frames.h`, `jpl_convenience.h` now include `jpl_export.h` and prefix all public declarations with `JPL_API`.
- Library install layout: static archive to `${CMAKE_INSTALL_LIBDIR}/static`, shared library to `${CMAKE_INSTALL_LIBDIR}/shared` and `${CMAKE_INSTALL_BINDIR}`.
- C++ binding installs headers to `${CMAKE_INSTALL_INCLUDEDIR}/jpl/`.
- `jpl_install` test updated for split static/shared layout.

### Fixed

- License sweep: build directories are exempt by name (`build*`, `cmake-build-*`, `out/`) and by `CMakeCache.txt` marker, so `jpl_licenses` and `jpl_install` pass in a build tree of any name.

- `jpl_install` test: POSIX shared-link check now references `${LIBDIR}/static/libjpl.a`.
- License checker now exempts `build_debug/`, `build_release/`, `obj/`, `bin/` build directories.
- C#/VB.NET/Python: loader path (`$<TARGET_FILE_DIR:jpl_shared>`) replaces broken wrapper scripts and DLL copies. (M2, M3, L4)

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
