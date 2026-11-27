<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Light-time solution: `jpl_get_lighttime_solution()` returns the solver's relative state, observer barycentric state at reception, delay, and exact emission epoch atomically; `jpl_get_state_lighttime()` is now its wrapper. The convenience geometry path consumes the solution instead of re-reading the observer and reconstructing emission from the delay. Exposed in all four bindings.

### Changed

- Tests: added a domain-level DE fixture builder (`tests/jpl_fixture.h` / `.c`) and migrated the threads, mmap identity probe, light-time linear, and numeric fixture setup off hand-built byte layouts. Parser, core open/format, and large-file tests stay byte-level by design; the remaining behavior-test migrations are follow-ups.
- Core: query entry points now issue one `jpl_verified_epoch` token per query (normalized data epoch plus coverage bounds); evaluation, body composition, the light-time solver, and the convenience pipeline consume the token instead of re-preparing dates or re-checking coverage per leg. The convenience path verifies the TDB epoch after TT→TDB conversion and passes it to verified internal entry points.

## [0.11.0] - 2026-11-27

### Changed

- **Bindings contract**: `record_count()` and `has_tt_tdb()` now raise on closed handles (previously returned 0/False), matching the documented use-after-close contract.
- **Numerical correction**: TT→TDB file refinement sign fix in `jpl_tt_to_tdb_policy()` now computes `TDB = TT − (TT − TDB)`; the previous code added the correction and shifted apparent convenience results on DE430t/DE432t/DE436t/DE438t/DE440t.
- **Windows I/O**: File paths are converted from UTF-8 to UTF-16 for `jpl_open`, `jpl_open_mmap`, and FILE clone reopen, so non-ASCII paths work with the bindings' UTF-8 marshaling.

### Removed

- Documentation claims that Graphviz is required for docs checks or call graphs.
- Six broken relative links in historical documentation.
- Unused `JPL_DJM0` macro from `src/jpl_private.h`.
- "Silently absent" wording for missing binding toolchains in `BUILDING.md`.
- "Retarded-epoch velocity" wording in `include/jpl_convenience.h`.

### Fixed

- `jpl_horizons_de441_local` now requires the documented full DE441 span.
- `test_all_ephemerides` scans `DE*.eph` instead of using a fixed filename list; `DE404.eph` is explicitly excluded.
- New test `jpl_generate_reference_run` reproduces the numerical-reference header.
- Tautological aberration test fixed; `jpl_tdb_to_tt` failure-preserves-output and exact-aliasing test added; fixture-negative cases now assert failure reason.

## [0.10.0] - 2026-11-24

### Changed

- Bindings: conversion contract recorded in `bindings/CONTRACT.md` (header snapshot incl. the 400-name overflow rule, date/state bridging, context wrapping, the 0.0-omits-solar-term rule, round-trip rule, per-language error behavior); each language pins overflow-name agreement, aberration equivalence, and the ecliptic round trip. Audit found no behavioral divergences (one stale comment fixed). No behavior change.
- Cleanup: removed the `JPL_MAX_CONSTANTS` alias (use `JPL_MAX_CONSTANT_NAMES`) and the `jpl::jpl` CMake target aliases (link `jpl::jpl_static`); deleted unused C#/VB `Clone(out)` overloads and the VB-only `GetTtMinusTdbResult` wrapper; deduplicated test byte helpers and record-width spellings; scoped the 0.3.0 migration table as history. New `rotate_vector` binding in all four languages with cross-binding equivalence pins.
- Infra: each configure writes a suite data manifest (`<build-dir>/suite_provenance.txt`, echoed to the configure log) recording fetch-tier resolution, maintainer redirects, and committed fixtures — one answer per build tree to "which data did this run validate". Read-only addition; no test logic changes.

## [0.9.0] - 2026-11-24

### Changed

- Core: query entry points now funnel validate → normalize → coverage through a single owned entry gate (`jpl_entry_prepare` + `jpl_entry_coverage`); dataset and frame gating still report UNSUPPORTED after RANGE. No behavior change (precedence matrix preserved, goldens byte-identical).
- Core: TT→TDB conversion in the convenience pipeline now goes through a file-vs-model policy (`jpl_tt_to_tdb_policy`): file-interpolated refinement when TT-TDB data is present and in coverage, else the model. Any file-path failure keeps the model result. No behavior change where no file data exists (all committed goldens); new file-TT × apparent-place golden pins the previously uncovered combination.
- Dates: trust-boundary audit recorded in `src/jpl_private.h` (every normalization site names its boundary); no redundant normalizations found — `add`/`subtract`/`partition`/`select_record` keep theirs as directly-callable funnels (see `test_dates` split cases and the selector far-date case); `jpl_date_compare` stays lexicographic with a new pinning test. No behavior change.
- I/O: `read_doubles` now serves every record read through the handle's byte-source adapter (mmap `memcpy` vs FILE `seek`+`fread` chosen at construction); `jpl_open`/`jpl_open_mmap` share one layout validator and handle fill, with platform mapping isolated in `jpl_map_handle`/`jpl_unmap_handle` (no `#if` in constructors or close). No behavior change (goldens byte-identical; thread/clone contracts unchanged).
- Frames: **BREAKING**: deleted the pure-duplicate `jpl_frame_context_rotate_position`/`jpl_frame_context_rotate_velocity` (identical bodies); one `jpl_frame_context_rotate_vector` plus `jpl_frame_context_rotate_state` (now two vector rotations) cover all uses. In-repo callers migrated; no behavior change.

### Removed

- Frames: `jpl_frame_context_rotate_position`, `jpl_frame_context_rotate_velocity` (use `jpl_frame_context_rotate_vector`).

## [0.8.1] - 2026-11-20

### Fixed

- Core: `jpl_frame_context_create()` now sets `*out_context = NULL` on all failure paths; `validate_layout()` propagates `JPL_ERROR_IO`/`JPL_ERROR_MEMORY` from record-endpoint probes; `resolve_reopen_path()` guards Win32 `GetFullPathNameA` buffer growth; `lt_fast_iterate()` checks `fabs(*residual) <= JPL_LIGHTTIME_TOL_DAYS`; `jpl_date_partition()` normalizes endpoint indices; removed unused `identity` struct fields and redundant `select_record()` recomputation.
- CMake & tests: `jpl_horizons_offline` and `jpl_horizons_reference_mutation` now run unconditionally against the committed DE441 slice (`tests/data/de441_2000_2010.eph`); `jpl_shared` sets `C_VISIBILITY_PRESET hidden` so Unix shared libraries export only `JPL_API` symbols; `jpl_install` supports `-DLIBJPL_BUILD_SHARED=OFF`; expanded `jpl_licenses` sweep to cover C++, C#, VB.NET, and Doxygen files and added missing inventory entries; added direct unit tests for `jpl_frame_context_*`.
- Bindings: hardened C# and VB.NET P/Invoke calls with array-length validation and `SafeHandle` `DangerousAddRef`/`DangerousRelease`, populated overflow `ConstantNames` (>400), delegated `JplException` messages to `jpl_status_string()`, and added `ApplyAberrationFull`; made Python `apply_aberration*` `@staticmethod`, guarded closed handles with `JplError`, and fixed `pyproject.toml`; avoided redundant overflow-name reads in C++/Python `constants()`/`read_record()`; fixed all four binding examples.
- Documentation: aligned `include/jpl.h`, `include/jpl_frames.h`, `README.md`, `BUILDING.md`, `CHANGELOG.md`, `docs/validation.md`, `docs/migration-two-part-dates.md`, `.github/workflows/ci.yml`, and all five `bindings/*/examples/README.md` files with the v0.8.x API.

### Changed

- Documentation: Doxygen project name now includes library version (`libjpl 0.8.1`).
- Build: ignore `build-docs/` directory in `.gitignore`.

## [0.8.0] - 2026-11-20

### Changed

- Normalized-date discipline is now documented in one place (`src/jpl_private.h`): public entries normalize user input once via `jpl_date_normalize` at the trust boundary; internals consume normalized pairs, with `add_core()` asserting the contract in debug builds (`JPL_ASSERT_NORMALIZED`). Added a normalization-idempotence test. No behavior change (audit found no redundant or missing normalizations).
- **BREAKING**: Deleted `jpl_ecliptic_to_icrs` (a pure duplicate of the frame-context path); thinned `jpl_icrs_to_ecliptic`, `jpl_frame_rotation_matrix`, and context creation onto a single internal matrix computer. All in-repo and binding callers migrated to the context API; no behavior change.

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
