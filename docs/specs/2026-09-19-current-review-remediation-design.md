<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Current review remediation design

Status: approved by the maintainer on 2026-09-19 for implementation planning. This document defines intended behavior only. It does not authorize implementation, branch creation, commits, merges, tags, or release publication.

Baseline: `b6d3ac1` on `main`. Source: the current-tree review performed on 2026-09-19 after the earlier `1327b20` remediation merged.

## 1. Approved execution model

- Implement on a dedicated `bugfix/current-review-remediation` branch created from an up-to-date `main` in an isolated worktree.
- Keep fixes in dependency order and split them into small Conventional Commits. Every behavior-changing commit includes a regression that fails before the fix and passes after it.
- Do not rewrite or squash implementation commits merely to make history look cleaner. The branch is intended to remain useful for bisection.
- Merge back to `main` only with an explicit merge commit after every task, platform gate, and real-data gate passes. Do not fast-forward, rebase, squash, or merge before maintainer approval.
- Change the package version to `0.3.0`. This is the first package identity for the already-breaking two-part-date API and the fixes in this remediation.
- Preserve C17, the dependency-free static library, three installed public headers, vendored ERFA v2.0.1, modern-AU outputs, two-part Julian dates, and the existing documented astronomy scope.
- Never modify files under `src/erfa/`.

## 2. Parser and I/O behavior

### 2.1 Advertised final endpoint

The public record count may continue to expose a supported physical trailing data record through `jpl_record_count()` and `jpl_read_record()`. Computational date selection must distinguish physical records from advertised intervals. A query exactly equal to `header.end_jd` selects advertised interval `N - 1`, never physical trailing record `N`.

The ephemeris handle therefore stores both the number of physically readable data records and the number of advertised covered intervals. Layout validation derives both values from the accepted candidate. Raw record APIs use the physical count. Interpolation and date selection use the advertised count.

### 2.2 Candidate validation

Remove value-equality heuristics from layout identity. A numeric constant equal to a record start date is valid data and cannot prove that a candidate offset is wrong. Candidate acceptance remains based on complete geometry, scheduled record endpoints, header metadata, pointer spans, and unique surviving layout and byte-order interpretation.

Do not allocate an entire candidate record or an overflow-name-sized contiguous header buffer. The parser reads the fixed classic header and, after candidate width is known, reads the 24-byte optional extension tail directly at its checked offset. Extra constant names beyond the public 400-name cap are skipped by offset, not copied. File-controlled record widths must not control heap allocation during open.

### 2.3 Endpoint rounding and zero-length reads

Record endpoint matching uses a magnitude-aware binary64 allowance. Calculate the allowance from the ULPs adjacent to the encoded endpoint and scheduled high part; do not use a fixed bound derived only from modern Julian dates. The allowance excuses encoding arithmetic only and never widens public coverage.

Zero-length raw reads return `JPL_OK` after ordinary pointer and handle validation and do not call `malloc(0)`, seek, read, or copy. This rule applies consistently to FILE and mmap backends and both byte orders.

## 3. API and numerical behavior

### 3.1 Validation precedence and statuses

Every state-taking API validates target and center codes before date coverage, dataset availability, record I/O, and body shortcuts. Invalid body combinations return `JPL_ERROR_ARGUMENT` regardless of date or file contents. Center zero remains the SSB alias; target zero remains invalid.

For finite two-part date inputs whose mathematical sum cannot be represented, preserve `JPL_ERROR_NUMERIC` from `jpl_date_normalize()`. Nonfinite parts remain `JPL_ERROR_ARGUMENT`. Apply this consistently to state, light-time, TT-TDB, convenience, frame, and time-conversion entry points. Computed outputs remain unchanged on every failure.

### 3.2 Aberration normalization

Both direction normalizations in `jpl_apply_aberration_full()` use the shared scale-safe norm helper. A finite intermediate vector with a representable direction must not fail merely because squaring its components overflows. Genuine nonfinite intermediate coordinates, zero direction, unrepresentable final coordinates, and an unrepresentable solar term continue to return `JPL_ERROR_NUMERIC` transactionally.

## 4. Validator trustworthiness

### 4.1 Parallel-safe synthetic corpus

Generate the validator corpus once per CTest run through a fixture setup test. Validator cases consume it but never regenerate or truncate it. Mark the generated corpus with `FIXTURES_REQUIRED`; use a resource lock only if a test still mutates shared data. A parallel repeated run must remain deterministic.

### 4.2 Reference diversity

The testpo runner keeps its minimum validated-row count but also checks semantic coverage. For every dataset present in the binary and represented by the supported validator contract, require each valid component at least once: ordinary Cartesian state components 1-6, nutation components 1-4, libration components 1-6, and TT-TDB value/rate components 1-2. Absent optional datasets do not create requirements. At least one available ordinary-body target must satisfy all six ordinary components. Duplicate rows cannot satisfy missing components.

Synthetic good data is expanded to meet this contract. Add a duplicate-only negative corpus that exceeds 100 rows but fails the diversity check.

### 4.3 Ephemeris sweep

The opt-in all-ephemerides sweep validates every physical data record, not only record zero. For each record, read it, require finite ordered encoded dates, and compare its start/end against the scheduled interval appropriate to covered or trailing records. This is intentionally a full sequential scan in an opt-in integration tier; detecting arbitrary interior corruption is more important than minimizing bytes read.

## 5. Distribution, CI, and provenance

### 5.1 Fetch cache publication

A verified download replaces a stale cache entry transactionally. Move the stale destination to a sibling backup, attempt to publish the verified temporary file, and restore the backup if publication fails. Never overwrite an existing backup silently. Remove the backup only after the new destination exists and the temporary file is gone. A test-only forced-publication-failure switch exercises rollback without relying on platform races.

### 5.2 Workflow truthfulness and security

The maintainer-local workflow requires a dedicated runner label plus nonempty `JPLEPH` and `JPL_TESTPO` repository variables. Configuration fails before downloading or testing when either variable is absent. The workflow checks that `jpl_all_ephemerides_local` and at least one local non-440 testpo suite are registered before running them.

Set workflow token permissions explicitly to `contents: read`. Pin third-party actions to reviewed full commit SHAs while retaining comments with their human-readable release lines.

### 5.3 Package version

Set `project(libjpl VERSION 0.3.0 LANGUAGES C)`. Keep exact-version package compatibility. Add a dated `0.3.0` changelog section containing the existing breaking Unreleased work plus this remediation, and leave a new empty `Unreleased` heading for future work. Update installation tests to require exactly `0.3.0` and reject `0.2.0` and `0.3.1` requests.

### 5.4 Docs test registration

CTest is enabled independently of `LIBJPL_BUILD_TESTS`. With docs ON and ordinary tests OFF, `jpl_docs_symbols` and `jpl_docs_paths` are registered and pass. Ordinary library test executables remain controlled by `LIBJPL_BUILD_TESTS`.

### 5.5 Numerical provenance

Pin canonical-LF SHA-256 values for `tests/data/numerical_reference.h` and `tools/generate_numerical_reference.c` in `tests/data/MANIFEST.txt`. Add an always-on CMake test that canonicalizes CRLF to LF only and verifies both files. Update the prose provenance hashes to the same values. Any future generator or reference change must update code, reviewed output, manifest, and provenance together.

## 6. Documentation corrections

- `jpl_get_state_lighttime()` accepts targets 1-13 and centers 0-13. Every parameter, return, and note statement uses those exact ranges.
- `LICENSES.md` links to the root `LICENSE` with `LICENSE`, not `../LICENSE`.
- README states that both downloaded binaries and testpo files receive exact size and SHA-256 validation.
- Live comments and validation notes describe the new validator fixture, full sweep, cache rollback, local-workflow requirements, action pins, docs-only tests, and `0.3.0` package identity accurately.

## 7. Acceptance gates

- Fresh Windows MSVC Debug and Release builds with zero warnings; all offline tests pass serially and with parallel CTest.
- Validator stress: at least five repeated parallel runs with no failures.
- Docs ON/tests OFF configuration registers and passes exactly the two docs checks.
- Installed C and C++ consumers pass default, relocated, custom install-directory, exact-version, and mismatch cases for `0.3.0`.
- Linux GCC and Clang Debug and Release pass strict C17 builds with no warnings.
- Windows clang-cl and macOS Clang pass build, tests, and installed consumers.
- ASan/UBSan pass the offline suite; TSan passes concurrency suites.
- The workflow policy test confirms explicit permissions, immutable action pins, local-runner labels, and variable guards.
- Opt-in DE440/DE440t references and the maintainer-local legacy/long-span sweep pass. Every available binary is fully scanned.
- Doxygen runs with `WARN_AS_ERROR=YES`; license, fixture, numerical provenance, examples, fetch-cache, and package tests pass.
- `git diff --check` is clean, the worktree contains no unintended generated files, `src/erfa/` is unchanged, and every review finding maps to a completed task and regression.

## 8. Finding coverage

| Review finding | Required disposition |
| --- | --- |
| F01 validator race | One-time CTest fixture generation plus parallel stress |
| F02 trailing final endpoint | Separate physical and advertised counts; endpoint regression |
| F03 constants collision | Remove value discriminator; FILE/mmap regression |
| F04 body validation precedence | Validate codes before date/data access |
| F05 aberration norm overflow | Scale-safe second norm plus tiny-Sun reference |
| F06 parser allocations | Direct bounded reads; 1 MiB record plus read-limit regression |
| F07 cache replacement loss | Backup, publish, rollback, forced-failure test |
| F08 local sweep false pass | Required variables, dedicated label, registration proof |
| F09 duplicate 0.2.0 identity | Version 0.3.0 and exact package tests |
| F10 weak testpo diversity | Per-dataset component matrix and duplicate-only negative |
| F11 interior sweep omission | Full record scan and interior-corruption negative |
| F12 date overflow status | Preserve NUMERIC consistently |
| F13 fixed endpoint tolerance | ULP-derived magnitude-aware allowance |
| F14 malloc(0) | Explicit zero-count success path |
| F15 docs checks disappear | Unconditional CTest enable plus docs-only configure test |
| F16 stale reference hashes | Manifest-backed canonical hashes and CTest |
| F17 mutable workflow actions | SHA pins and least-privilege permissions |
| F18 live documentation drift | Correct all identified statements and add link/policy checks |
