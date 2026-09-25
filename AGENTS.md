<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

Small, dependency-free C17 library for reading JPL Development Ephemeris
(DE) binaries. Static lib `jpl` + three public headers.

## Protocol deviations

If this guide's prerequisites aren't met (no git repo, missing
toolchain, conflicting instructions), stop and ask before working
around them — do not silently deviate. This applies to workflow
and infrastructure only; ordinary implementation judgment calls
stay with the implementer (record them in the commit message or
task ledger, don't stall on them).

## Layout

- `include/` — public API: `jpl.h` (core), `jpl_frames.h` (ICRS frames),
  `jpl_convenience.h` (planet/moon helpers). All public decls need Doxygen
  (`@brief/@param/@return/@note`); keep `WARN_AS_ERROR=YES` clean.
- `src/` — `jpl.c`, `jpl_frames.c`, `jpl_convenience.c`, internal
  `src/jpl_private.h` (shared constants/helpers, NOT installed),
  `src/erfa/` (vendored ERFA v2.0.1, BSD-3 — never modify sources here).
- `tests/` — one executable per `test_*.c`; shared mock builders live in
  `tests/jpl_test_util.h` (put_u32/put_f64/put_ptr, mock header, norm3).
- `docs/superpowers/{specs,plans}/` — design specs and implementation plans.

## Review and required follow-up

- [Project review at 1327b20](docs/reviews/2026-09-17-project-review.md)
  records open findings, reproductions, verification limits, and a resolution
  log. Consult it before follow-up work; the report itself fixes no code.
- [Remediation design](docs/superpowers/specs/2026-09-17-review-remediation-design.md)
  records the agreed breaking-change, two-part-date, modern-AU, legacy-frame,
  and MIT licensing policies plus complete issue coverage. The maintainer
  has approved the written specification.
- [Implementation plan](docs/superpowers/plans/2026-09-17-review-remediation.md)
  is the execution entry point. It maps every issue to ordered tasks and links
  detailed core, validation, and distribution plans. Planning does not resolve
  findings; record actual fix and verification evidence in the review log.
- Maintainer requirement: all Julian dates must use arrays of two doubles,
  preserving both parts through date arithmetic and ERFA calls. The current
  scalar-date APIs violate this requirement; see R00 in the review. Splitting
  an already-rounded scalar at an ERFA call does not restore precision.
  Preserve the standard DE on-disk format when migrating in-memory dates.

## Build & test

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- CMake 3.20+, `c_std_17`, `C_EXTENSIONS OFF`.
- MSVC: `/W4 /permissive-`, zero warnings. GCC/Clang:
  `-Wall -Wextra -Wpedantic`.
- MSVC is multi-config: always pass `-C Debug` (or `-C Release`) to ctest.
- Single-config generators default to Release (set in CMakeLists).
- `LIBJPL_BUILD_DOCS=OFF` by default; `ON` requires Doxygen (1.9+ known good).
- `LIBJPL_FETCH_EPHEMERIDES=OFF` by default (fully offline). `ON` registers
  the opt-in download/reference tier (`jpl_real_ephemeris`,
  `jpl_all_ephemerides` when the resolved download dir is non-empty,
  `jpl_all_ephemerides_local` only when `JPLEPH` is set, `jpl_testpo_440[t]`
  plus the local `JPLEPH`/`JPL_TESTPO` sweep). `JPLEPH`/`JPL_TESTPO` env dirs
  only redirect per-file sources inside that tier; they do not enable it on
  their own. Default offline suites: unit/validator/fixture/license/fetch-cache/install/workflow-policy/numerical-reference/docs-only/docs/examples tiers (see `ctest -N`).
- Generated docs: `jpl_docs_symbols` (public-symbol XML inventory) and
  `jpl_docs_paths` (source/build paths with spaces) exist when docs are
  enabled, even with ordinary tests OFF (`jpl_docs_only` proves the
  docs-ON/tests-OFF matrix); `jpl_examples` compiles/runs the README patterns.

## Conventions

- No outside dependencies: ERFA is vendored under `src/erfa/`, used via
  `file(GLOB)` + test-driver exclusion + version defines in CMakeLists.
- Always build: new features are unconditional; only docs/tests are options.
- Append, don't rewrite: public headers accumulate Doxygen + decls; use
  surgical edits so earlier comments survive.
- `jpl_apply_aberration` and frame helpers are pure math (no handle/I/O);
  I/O lives in `read_doubles` (native-order fast path) and the open/clone
  paths. Same `jpl_ephemeris*` must not be shared across threads on the
  FILE path — use `jpl_ephemeris_clone()` per thread or `jpl_open_mmap()`.
- Physics constants come from `src/jpl_private.h`, never literals.
- Shell: agent-run commands execute under PowerShell 5.1, so keep them
  5.1-compatible — chain with `;` (no `&&`/`||`), quote paths with
  spaces, use `workdir` instead of `cd`. (The user shell is pwsh 7.6.6;
  7-only syntax must not appear in commands agents run.)
