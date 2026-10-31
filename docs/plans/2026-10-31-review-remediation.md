<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# 2026-10-31 review remediation Implementation Plan

> **For agentic workers:** use checkboxes (`- [ ]`) for tracking. Work one
> workstream at a time, in the order given under "Merge sequence".

**Goal:** close every finding from the 2026-10-31 full-tree review: a
light-time solver that reports success without converging, three bindings
that corrupt memory on `jpl_apparent_ecliptic_position`, a license sweep
that breaks every build directory except `build/`, bindings that CI never
compiles, and documentation that describes behaviour the code does not have.

**Baseline:** `main` @ `00dfabb` (2026-09-29, v0.5.0) plus an uncommitted
working tree (`AGENTS.md`, `LICENSES.md`, `tests/cmake/check_licenses.cmake`,
`tests/cmake/test_install.cmake`, `tests/license_inventory.cmake` modified;
`docs/{plans,specs}` deleted; `docs/{plans,specs}` untracked).
Baseline suite: **44/46** in a fresh `build_review` (VS 18 / MSVC 19.51,
Debug); the two failures are entirely caused by finding B3.

**Spec:** the review report recorded as `docs/reviews/2026-10-31-review.md`
(Task 0). Findings are referenced by ID: B1-B3, H1-H3, M1-M5, L1-L5.

**Tech stack:** C17, CMake 3.20+/CTest, C++17, Python 3 + cffi, .NET 8
(C#/VB.NET), PowerShell 5.1 for Windows commands.

## Global constraints

- Do not modify `src/erfa/`; use ERFA only through internal library code.
- Library targets keep `c_std_17` with `C_EXTENSIONS OFF`.
- MSVC `/W4 /permissive-`; GCC/Clang `-Wall -Wextra -Wpedantic`.
- Public headers accumulate Doxygen; surgical edits only.
- Two-part Julian dates stay two-part; never split a rounded scalar.
- Physics constants come from `src/jpl_private.h`, never literals.
- Every new source file carries the MIT header with
  `Copyright (c) 2026 Todd Carnes` / `SPDX-License-Identifier: MIT`, and is
  added to `tests/license_inventory.cmake` unless exempt.
- Commands run under PowerShell 5.1: no `&&`/`||`, use `; if ($?) { ... }`,
  quote paths with spaces, use `workdir` instead of `cd`.
- MSVC is multi-config: always pass `-C Debug`/`-C Release` to ctest.
- Do not merge into `main` without explicit maintainer approval; use
  `git merge --no-ff <branch>`.
- Do not stage or commit files unless explicitly asked.

---

## Decisions required before implementation

| ID | Question | Recommendation |
|---|---|---|
| D1 | Is Graphviz `dot` actually required? | Determine empirically (Task 0.2). Current evidence says **no, on any platform**; see Appendix A. |
| D2 | C#/VB `JplHeader`: implement `FromNative` or remove the properties? | Implement. The header API is advertised in `README.md`, `CHANGELOG.md` 0.5.0, and `docs/validation.md`; removing it is a breaking API change for a released feature. |
| D3 | Warning policy: doc says "zero tolerance" but no `/WX` or `-Werror`. | Enforce in CI only (`-Werror` via `CMAKE_C_FLAGS`, `/WX` via `CMAKE_C_FLAGS`), leave local builds forgiving. Then `BUILDING.md:140` is true for CI and the "zero warnings" claim becomes testable. |
| D4 | `CHANGELOG.md:39` (0.5.0) claims the checker exempts `build_debug/`/`build_release/`; it never did. | Correct 0.5.0 in place (one factual line, not a feature change) and note the correction under `[Unreleased]`. Rewriting a released entry is a maintainer call; the wrong line is actively misleading otherwise. |
| D5 | `cmake/libjplConfig.cmake.in` exports `jpl::jpl_shared_debug` as an alias for the generic shared target. | Remove it; `jpl::jpl_shared` already exists and the `_debug` name is wrong for Release consumers. |

---

## Task 0 — capture and settle

### 0.1 Record the review

- [ ] Write `docs/reviews/2026-10-31-review.md` from the review findings
  (B1-B3, H1-H3, M1-M5, L1-L5) with `file:line` citations and the probe
  evidence. `docs/reviews/` is license-exempt and already exists.
- [ ] Reference it from this plan and from `docs/validation.md`.
- [ ] Verify: file present, no build or test change.

### 0.2 Settle the Graphviz question (D1)

**Question:** `BUILDING.md:102-103,115-116` and `AGENTS.md:65` say Linux
docs checks need `dot`; `BUILDING.md:113-114` says Windows treats it as
optional. What does the code actually require?

**Current evidence (all checked this session):**

| Artifact | Finding |
|---|---|
| `Doxyfile.in` (entire file, 12 lines) | No `HAVE_DOT`, no `DOT_*`, no `CALL_GRAPH`; Doxygen defaults `HAVE_DOT = NO`. |
| `docs/api.dox`, `include/*.h` | No `\dot`, `\graph`, or `@dot` blocks. |
| `tests/cmake/check_docs.cmake` | Reads generated XML only; never invokes `dot`. |
| `tests/cmake/test_docs_paths.cmake` | Configures and builds the same `docs` target; never invokes `dot`. |
| `.github/workflows/ci.yml` docs job | Installs **only** `doxygen`, then runs `jpl_docs_symbols` and `jpl_docs_paths`. |
| `docs/validation.md:38` | Records a real Ubuntu run that stopped at `jpl_docs_only` until Graphviz 14.1.2 was installed. |

The first four rows say dot is never invoked. The last row says a real run
needed it. One of the two is stale; do not guess.

**Procedure** (on a Linux machine or a throwaway CI branch):

```sh
# 1. Prove the baseline is green with Graphviz present.
cmake -S . -B build/nodot -DCMAKE_BUILD_TYPE=Release \
  -DLIBJPL_BUILD_DOCS=ON -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/nodot --target docs
ctest --test-dir build/nodot -R 'jpl_docs_symbols|jpl_docs_paths' --output-on-failure

# 2. Remove Graphviz and repeat from a clean tree.
sudo apt-get remove -y graphviz
rm -rf build/nodot
cmake -S . -B build/nodot -DCMAKE_BUILD_TYPE=Release \
  -DLIBJPL_BUILD_DOCS=ON -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/nodot --target docs
ctest --test-dir build/nodot -R 'jpl_docs_symbols|jpl_docs_paths' --output-on-failure
```

- [ ] If step 2 is **green**: Graphviz is optional on every platform.
  Delete the requirement from `BUILDING.md:102-103,115-116` and
  `AGENTS.md:65`; annotate `docs/validation.md:38` as historical
  (superseded by this experiment); leave the CI docs job installing only
  Doxygen. Reword `BUILDING.md:113-114` to say graphs are not generated by
  this project at all.
- [ ] If step 2 is **red**: keep the requirement, and make it coherent —
  add `HAVE_DOT = YES` (or the specific `DOT_*` option) to `Doxyfile.in`,
  add `graphviz` to the CI docs job install line, and keep the
  `BUILDING.md`/`AGENTS.md` text. Capture the exact failing output in the
  commit message.
- [ ] Either way, add a comment to `Doxyfile.in` stating whether `dot` is
  required, so the next reader does not have to re-derive it.
- [ ] Verify: docs job green on Linux, `jpl_docs_symbols` and
  `jpl_docs_paths` pass in both the parent and nested builds.

---

## Workstream 1 — library correctness

### 1.1 Fix silent light-time success (B1)

**Files:** `src/jpl.c`, `tests/test_lighttime.c`, `include/jpl_testing.h`

- [ ] In `lt_fast_iterate` (`src/jpl.c:1807-1822`), after the loop and
  before `*delay = lt;`, add:

  ```c
  if (st == JPL_OK) st = JPL_ERROR_CONVERGENCE;
  ```

  Rationale: exhausting the loop after a successful evaluation leaves
  `st == JPL_OK` while the settle test never fired. `lighttime_solve`
  (`src/jpl.c:1887-1891`) then returns `JPL_OK` with `*delay` written and
  `state` untouched, violating both `jpl.h`'s "unchanged on failure" and the
  solver comment at `src/jpl.c:1833-1834`. The RANGE and date-error breaks
  already carry the right status and must not be overwritten.
- [ ] Confirm the local `lt` still reaches the bracket path: `*delay = lt;`
  stays at `src/jpl.c:1820`, and `lighttime_solve` owns `lt`, so the
  caller's `*out_light_time` stays unwritten on failure.
- [ ] Add a regression test in `tests/test_lighttime.c` next to the existing
  budget case (`:501-505`), using the same `write_linear_mock` fixture:
  - `fast=1, safe=0` on `JPL_MARS` from `{2451545.5, 0.0}` must return
    `JPL_ERROR_CONVERGENCE`, with `state` byte-identical to the caller's
    buffer and `tau` unchanged (the probe confirmed this case previously
    returned `JPL_OK` with `tau = 0.005804...` and an untouched `state`).
  - `fast=1, safe=JPL_LIGHTTIME_SAFE_MAX_ITER` must return `JPL_OK` and
    satisfy `fabs(tau - norm(state)/C) <= 1e-12` (the existing near-end
    assertions at `tests/test_lighttime.c:480-484` show the form).
  - `fast=0, safe=0` keeps returning `JPL_ERROR_CONVERGENCE`.
  - Assert the cheap invariant on the success path: the returned `state`
    is the emission-epoch state, not the reception-epoch one.
- [ ] Re-run the probe from
  `C:\Users\mm1ca\AppData\Local\Temp\opencode\lt_probe.c` against the fixed
  library and confirm it now reports `JPL_ERROR_CONVERGENCE` with unchanged
  outputs.
- [ ] Verify: `ctest -R jpl_lighttime` green in Debug and Release.

### 1.2 Remove the library stderr write (M1)

**Files:** `src/jpl.c`

- [ ] Delete `fprintf(stderr, ...)` at `src/jpl.c:1621` and return the
  status directly.
- [ ] Verify: `Select-String -Path src\*.c -Pattern 'fprintf|printf|puts'`
  returns nothing (the library is otherwise silent), and the full suite is
  unchanged.

---

## Workstream 2 — license sweep, install, packaging

### 2.1 Make the sweep build-directory-safe (B3, H3, L2)

**Files:** `tests/cmake/check_licenses.cmake`, `tests/cmake/test_licenses.cmake`,
`tests/license_inventory.cmake`, `CHANGELOG.md`

- [ ] Replace `build_*/` at `tests/cmake/check_licenses.cmake:84` with a
  pattern that matches real names, e.g.
  `^(src/erfa/|docs/(specs|plans|reviews)/|docs/(html|latex)/|out/|\.worktrees/|\.git/|\.superpowers/|build[^/]*/|cmake-build-[^/]*/)`.
  Note `build_*/` currently means `build` + zero or more `_` characters
  (POSIX ERE), so it exempts nothing useful.
- [ ] Add a marker-based predicate so arbitrary names work too: for each
  candidate, take the leading path component and skip if
  `${SOURCE}/${component}/CMakeCache.txt` exists. Name-matching alone will
  keep breaking on the next directory name.
- [ ] Extend `tests/cmake/test_licenses.cmake` with three cases built the
  same way as the existing worktree case:
  - `source-with-build/build_review/unlisted.c` (no cache file) → **pass**
    (name-based exemption).
  - `source-with-build/mybuild/CMakeCache.txt` +
    `source-with-build/mybuild/unlisted.c` → **pass** (marker-based).
  - `source-with-build/plain/unlisted.c` → **fail** (proves the sweep still
    catches real source).
- [ ] Deduplicate `tests/license_inventory.cmake:131,138`
  (`bindings/cpp/examples/README.md`) and re-indent `:129`
  (`bindings/vbnet/tests/Program.vb`).
- [ ] Correct `CHANGELOG.md:39` per D4.
- [ ] Verify: `ctest -R 'jpl_licenses|jpl_install'` green in a build tree
  named `build_review`, `mybuild`, and `build`, with `LIBJPL_BUILD_DOCS=OFF`
  and `ON`.

### 2.2 Land the pending install-path fix (H3)

**Files:** `tests/cmake/test_install.cmake` (already modified in the tree)

- [ ] Fold the uncommitted `test_install.cmake` changes into the same commit
  as 2.1 so the install matrix runs before it is trusted.
- [ ] Re-run the manual invocation used during the review (real CTest
  arguments + patched checker) and confirm `libjpl: install verification OK`.
- [ ] Add a case asserting the shared runtime lands in `${BINDIR}` (not
  `${BINDIR}/shared`), matching `CMakeLists.txt:183-189`.
- [ ] Verify: `ctest -R '^jpl_install$'` green in Debug and Release, and
  the test fails if the shared runtime is pointed at a wrong prefix (mutate
  one line locally to prove the assertion bites, then revert).

### 2.3 Repair SDK staging (M5)

**Files:** `CMakeLists.txt`, new `cmake/verify_sdk.cmake`

- [ ] Replace the self-comparison at `CMakeLists.txt:1046` with a real
  post-condition: `compare_files "${JPL_SDK_PREFIX}/include/jpl.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/jpl.h"`.
- [ ] Replace the unconditional
  `remove_directory "${JPL_SDK_PREFIX}/share"` (`:1019-1022`) with removal
  of only `${JPL_SDK_PREFIX}/share/licenses` (already done on the previous
  line); do not delete a whole tree that future install rules may populate.
- [ ] Add `cmake/verify_sdk.cmake` (MIT header) checking that the staged
  tree contains the four public headers, `LICENSE`, `ERFA-LICENSE`,
  `doc/README.md`, and the expected library artifacts; invoke it after
  staging.
- [ ] Update `tests/license_inventory.cmake` with the new file.
- [ ] Verify: `cmake --build build --target package-sdk` with docs ON;
  inspect the zip; then delete one notice inside a scratch staging copy and
  confirm the verifier fails.

---

## Workstream 3 — language bindings

### 3.1 Fix the apparent-ecliptic ABI in three bindings (B2)

**Files:** `bindings/python/src/libjpl/_libjpl.py`,
`bindings/python/src/libjpl/ephemeris.py`,
`bindings/csharp/src/LibJpl/Native.cs`,
`bindings/csharp/src/LibJpl/LibJpl.cs`,
`bindings/vbnet/src/LibJpl/Native.vb`,
`bindings/vbnet/src/LibJpl/LibJpl.vb`

The C signature is four parameters, the last a
`jpl_apparent_ecliptic_result *`. Python (`_libjpl.py:84-88`), C#
(`Native.cs:187-195`), and VB (`Native.vb:163-172`) all declare `target`
plus five out-doubles, so the callee writes a 40-byte struct through the
first out-pointer slot. That is memory corruption, not a wrong value.

- [ ] C#: add `[StructLayout(LayoutKind.Sequential)] struct
  JplApparentEclipticResult` matching
  `jpl_apparent_ecliptic_result` field order, declare
  `out JplApparentEclipticResult result`, and update the wrapper method and
  its return type.
- [ ] VB: same shape with `ByRef result As JplApparentEclipticResult`.
- [ ] Python: declare the struct in the cdef, allocate
  `_ffi.new("jpl_apparent_ecliptic_result *")`, read fields in
  `ephemeris.py:322-337`.
- [ ] Audit every remaining declaration for the same class of error
  (Appendix B): functions taking a struct pointer, `size_t`, or a 2-D
  array. Record the audit result in the commit message.
- [ ] Add value-level tests to each binding: call the function for
  `JPL_MOON` at `{2451545.0, 0.0}`, assert longitude/latitude in valid
  ranges, `range_au` within `1e-9` of `MOON_KM / AU_KM` (the check
  `tests/test_convenience.c:130` already uses), and both rates finite.
- [ ] Verify: run each binding test against the fixed library; preferably
  also under ASan on Linux.

### 3.2 Implement `JplHeader.FromNative` in C# and VB (H2)

**Files:** `bindings/csharp/src/LibJpl/LibJpl.cs`,
`bindings/csharp/src/LibJpl/Native.cs`,
`bindings/vbnet/src/LibJpl/LibJpl.vb`,
`bindings/vbnet/src/LibJpl/Native.vb`,
binding test files

Currently only `EphemerisNumber` is populated (`LibJpl.cs:85-115`,
`LibJpl.vb:68-77`), so titles, constants, coverage, step, AU, EMRAT,
pointers, frame, and TT/TDB metadata all return defaults, and
`ReadConstants`/`ReadRecord` size buffers from zeros.

- [ ] Complete the native struct: `char titles[3][85]` as a flattened
  `ByValArray SizeConst=255` byte array (decode 3×85), `char
  constant_names[400][7]` as `ByValArray SizeConst=2800` (slice 400×7),
  `double[2] start/end`, `pointers[12]`, plus `record_double_count`
  (`UIntPtr`/`ulong`) and `byte_order`.
- [ ] Use `Marshal.PtrToStructure<JplHeaderNative>` and map every field;
  do not hard-code offsets.
- [ ] Add a layout guard in the binding tests: assert
  `Marshal.SizeOf<JplHeaderNative>()` equals the value reported by the C
  side (add a tiny `jpl_test_header_size` helper to `include/jpl_testing.h`
  and `src/jpl.c`, or a `sizeof` print in an existing test binary), and
  assert `Marshal.OffsetOf` for `ephemeris_number` is 3264.
- [ ] Test against `tests/data/de441_2000_2010.eph`: number 441,
  `constant_count >= 400`, `start_jd < end_jd`, AU in
  `[1.49597870e8, 1.49597871e8]`, EMRAT in `[81.30, 81.31]`, non-empty
  titles, `pointers[JPL_EARTH].subintervals > 0`, and `ReadConstants`
  returning exactly `constant_count` finite values.
- [ ] Verify: C# and VB tests fail if any mapped field is reverted to a
  default (prove by temporarily removing one assignment).

### 3.3 Fix DLL staging and configuration handling (M2, M3)

**Files:** `bindings/python/CMakeLists.txt`,
`bindings/csharp/CMakeLists.txt`, `bindings/vbnet/CMakeLists.txt`,
`bindings/csharp/run_csharp_test.bat.in`,
`bindings/vbnet/run_vbnet_test.bat.in`,
`bindings/csharp/run_csharp_test.sh.in`,
`bindings/python/src/libjpl/_libjpl.py`

- [ ] Stop copying from `${CMAKE_BINARY_DIR}/../../Debug/jpl.dll`
  (`bindings/python/CMakeLists.txt:36`); the artifact is
  `$<TARGET_FILE:jpl_shared>`.
- [ ] Prefer loader search paths over copies: wrap the test command in
  `${CMAKE_COMMAND} -E env` setting `PATH=$<TARGET_FILE_DIR:jpl_shared>;…`
  on Windows and `LD_LIBRARY_PATH=…` on Linux/macOS, and make the test
  depend on `jpl_shared`.
- [ ] If a copy is kept, make it `$<CONFIG>`-aware and fail loudly (no
  `>nul 2>nul`, no `|| true`).
- [ ] Delete the stale untracked `bindings/python/tests/jpl.dll` and
  `bindings/csharp/tests/jpl.dll`, and remove the hard-coded fallback at
  `_libjpl.py:100-115` (or keep it documented as a manual override).
- [ ] Verify: configure/build in Debug and Release with bindings ON from a
  clean tree; run each test with the stale DLLs removed.

### 3.4 Broaden C++ coverage (M4)

- [ ] Extend `bindings/cpp/tests/test_cpp_binding.cpp` to call
  `planet_position`, `moon_position`, and `apparent_ecliptic_position`,
  asserting finite values and a physical range check, mirroring 3.1.
- [ ] Verify: `jpl_cpp_binding` green in Debug and Release.

---

## Workstream 4 — CI

### 4.1 Compile and run the bindings (H1)

**Files:** `.github/workflows/ci.yml`, `tests/cmake/check_workflows.cmake`

No CI job enables any binding option, so none of the four tests ever
registers. This is the root cause that let B2 and H2 ship.

- [ ] Add `bindings-windows` (windows-2022, VS generator): setup Python
  3.12 + `pip install cffi`, setup .NET 8, configure with shared and all
  four binding options `ON`, build, run
  `ctest -R 'jpl_(cpp|python|csharp|vbnet)_binding' --output-on-failure`.
- [ ] Add `bindings-linux` (ubuntu-24.04): `apt-get install -y libffi-dev`
  + Python cffi, .NET 8, same configure/test.
- [ ] Assert registration, not just success: after configure, require
  `ctest -N` to list all four names, so a silently-absent test fails the
  job.
- [ ] Keep the `check_workflows.cmake` policy checks satisfied: pinned
  actions, explicit least-privilege `permissions`, dedicated self-hosted
  labels only where used.
- [ ] Apply D3 here: `-DCMAKE_C_FLAGS="-Werror"` for GCC/Clang jobs and
  `/WX` for MSVC jobs.
- [ ] Verify: both new jobs green; the full suite reports **50/50**
  (46 baseline + 4 binding tests).

---

## Workstream 5 — documentation and metadata

### 5.1 Correct binding documentation (L1)

**Files:** `docs/validation.md`, `README.md`, `BUILDING.md`, `CHANGELOG.md`

- [ ] Rewrite `docs/validation.md:168-201`: say which CI job executes each
  binding, remove "All binding tests pass ..." unless the new jobs are
  green, and state that C++ links the static library
  (`bindings/cpp/CMakeLists.txt:8`) while Python/C#/VB use the shared one.
- [ ] Fix `docs/validation.md:177-178`: `planet_position`, `moon_position`,
  and `tt_to_tdb` are members of `jpl::Ephemeris`, not free functions
  (`bindings/cpp/include/jpl/ephemeris.hpp:245,253,240`).
- [ ] Delete the duplicate empty `## Platform support` at
  `docs/validation.md:203`.
- [ ] Document the apparent-ecliptic longitude-rate step (`h = 0.01` day
  central difference, `src/jpl_convenience.c:226`) and its
  one-sided/adaptive fallback near coverage, in
  `include/jpl_convenience.h` as a `@note` (L5).
- [ ] Update `BUILDING.md:43` for the static C++ link, and
  `BUILDING.md:140` per D3.
- [ ] Add `[Unreleased]` entries for B1, B2, B3, H1, H2, M1-M5, L1-L5.

### 5.2 Repair the plan/spec rename fallout (L1)

**Files:** 14 Markdown files under `docs/specs/`, `docs/plans/`, plus
`docs/validation.md`, `BUILDING.md`, `AGENTS.md`

23 links still point at `docs/{plans,specs}/...`.

- [ ] Mechanical rewrite: `docs/plans/` → `docs/plans/`,
  `docs/specs/` → `docs/specs/`.
- [ ] Verify: `Select-String -Path *.md,docs\**\*.md -Pattern
  'docs/superpowers'` returns nothing, and every rewritten target exists
  (script the existence check; report missing targets).
- [ ] Optionally add a `tests/cmake/check_md_links.cmake` that verifies
  relative Markdown links resolve, so this cannot silently rot again.
  Register it as `jpl_md_links` only if it passes on the current tree.
- [ ] Verify: docs build and `jpl_docs_symbols`/`jpl_docs_paths` unchanged.

### 5.3 Remove Python import instrumentation (L4)

- [ ] Delete `traceback.print_exc()` and the four `DEBUG:` prints at
  `bindings/python/src/libjpl/_libjpl.py:108-114`; keep the raised
  `ImportError` with the searched paths.
- [ ] Verify: importing without the shared library prints one clear error
  and no traceback.

### 5.4 Package config cleanup (L3)

- [ ] Remove `jpl::jpl_shared_debug` from `cmake/libjplConfig.cmake.in`
  per D5.
- [ ] Verify: `jpl_install` consumer cases still find
  `jpl::jpl`/`jpl::jpl_shared` and build.

---

## Merge sequence

Each row is one branch, one review, one `--no-ff` merge after approval.

| # | Branch | Contents | Gate |
|---|---|---|---|
| 1 | `fix/lighttime-contract` | 1.1, 1.2 | `jpl_lighttime` Debug+Release; probe fixed |
| 2 | `fix/license-sweep` | 2.1, 2.2, 5.4 (L2 part) | `jpl_licenses`/`jpl_install` in `build_review`, `mybuild`, `build` |
| 3 | `fix/binding-abi` | 3.1, 3.3, 3.4, 5.3 | binding tests value-level; ASan clean |
| 4 | `feat/binding-header` | 3.2 | header assertions against DE441 fixture |
| 5 | `ci/bindings` | 4.1 | 50/50 on Windows and Linux |
| 6 | `docs/review-2026-10-31` | 0.1, 0.2, 5.1, 5.2, 5.4 | links resolve; docs job green |
| 7 | `build/sdk-staging` | 2.3 | `package-sdk` + negative check |

Branch 2 should be reviewed first if the maintainer wants a green local
suite immediately; branch 1 is the only library-behaviour change.

## Verification matrix

Run on every branch; record results in the PR body.

| Configuration | Expected |
|---|---|
| Windows MSVC Debug, bindings OFF | 46/46 |
| Windows MSVC Release, bindings OFF | 46/46 |
| Windows MSVC Debug+Release, bindings ON | 50/50 |
| Linux GCC Debug+Release | 46/46 (50/50 with bindings) |
| Linux Clang Debug+Release | 46/46 |
| Linux ASan/UBSan | existing `-E 'jpl_install|jpl_docs_paths'` set green |
| Linux TSan | `jpl_threads`, `jpl_mmap` green |
| Docs ON (Linux) | `jpl_docs_symbols`, `jpl_docs_paths`, `jpl_docs_only`, `jpl_examples` |
| Arbitrary build dir (`mybuild/`) | 46/46 |
| Path with spaces | `jpl_docs_paths`, `jpl_install` green |

## Risk register

| Risk | Mitigation |
|---|---|
| B1 fix changes the test hook and surfaces previously hidden failures | Only the exhaustion path changes; add the budget matrix in the same commit. |
| Binding signature change breaks source compatibility | Only affects calls that currently corrupt memory; note it in `[Unreleased]` as a bug fix. |
| `FromNative` layout drift between C and managed structs | `Marshal.SizeOf`/`OffsetOf` guards plus a C-reported size; fail the test loudly. |
| Marker-based skip hides a real source directory | Require `CMakeCache.txt` and keep the negative test for a plain directory. |
| CI .NET jobs need network for restore | Pin SDK 8.0.x, cache NuGet, keep tests offline. |
| Markdown rewrite touches many files | Scripted replacement; verify only link targets changed and every target exists. |
| Graphviz decision flips the docs job | Task 0.2 settles it before any doc edit. |

## Acceptance criteria

- `jpl_get_state_lighttime` never returns `JPL_OK` unless `state` and the
  light-time satisfy `JPL_LIGHTTIME_TOL_DAYS`, and never returns success
  with `state` untouched.
- No binding can write past an out-parameter for
  `jpl_apparent_ecliptic_position`; all four binding tests call it and
  assert values.
- C# and VB `GetHeader()` return real coverage, constants metadata, and
  pointers for DE441.
- `jpl_licenses` and `jpl_install` pass in a build directory named
  anything, including one with no `CMakeCache.txt` yet.
- CI builds and runs all four bindings on Windows and Linux.
- No documentation statement contradicts the code: Graphviz, warning
  policy, binding library linkage, free helpers, duplicate heading, and
  all `docs/superpowers` links.
- Baseline 44/46 → 46/46 with bindings OFF, 50/50 with bindings ON.

---

## Appendix A — Graphviz evidence

Current `Doxyfile.in` in full:

```
PROJECT_NAME = libjpl
OUTPUT_DIRECTORY = "@CMAKE_CURRENT_BINARY_DIR@/docs"
INPUT = "@CMAKE_CURRENT_SOURCE_DIR@/include" "@CMAKE_CURRENT_SOURCE_DIR@/docs/api.dox"
RECURSIVE = NO
GENERATE_HTML = YES
GENERATE_XML = YES
GENERATE_LATEX = NO
WARN_AS_ERROR = YES
```

Doxygen's default for `HAVE_DOT` is `NO`, so `dot` is not consulted. The
CI docs job installing only `doxygen` on `ubuntu-24.04` and running both
docs tests is consistent with that. The contradictory data point is
`docs/validation.md:38`; Task 0.2 resolves it rather than choosing a side.

Windows is unaffected either way: the current file never requests call
graphs, so `BUILDING.md:113-114` describes behaviour the project does not
enable.

## Appendix B — binding ABI audit checklist

Compare every declaration in `Native.cs`, `Native.vb`, and `_libjpl.py`
against the header, in this order:

| C parameter shape | Marshalling that is correct | Known status |
|---|---|---|
| `const double x[2]` | `double[]` / `double *` with `SizeConst=2` | OK today |
| `double out[6]` | `[Out] double[]` with `SizeConst=6` | OK today |
| `const double R[3][3]` | `double[,]` or flattened `double[9]` | check |
| `jpl_apparent_ecliptic_result *` | `out`/`ByRef` struct, or struct pointer | **broken** (B2) |
| `jpl_header *` (returned) | `PtrToStructure<JplHeaderNative>` | **stub** (H2) |
| `size_t` | `UIntPtr`/`ulong` (x64) | check |
| `int *` out | `out int` | check |
| `const char *` | `LPUTF8Str` | OK today |
