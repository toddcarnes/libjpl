<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Review Follow-up Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Address all seven findings from the fresh v0.3.0 review, including removal of the unreachable unit-conversion branch.

**Architecture:** Keep the distributable `jpl` library free of test hooks by compiling a private testing variant for five tests. Replace Python-based fixture tooling and inconsistent text hashing with one binary-safe C17 helper. Make narrow core, public-documentation, and unit-conversion corrections, then validate on Windows and a native Linux filesystem.

**Tech Stack:** C17, CMake 3.20+, CTest, MSVC/GCC/Clang, Doxygen for optional docs verification.

**Spec:** `docs/superpowers/specs/2026-09-25-review-followup-fixes-design.md` (approved; F1–F7).

## Global Constraints

- `src/erfa/` is vendored and must not be edited; installed library has three public headers and no new runtime dependency.
- Dates remain two-part `double[2]`; preserve the standard DE on-disk format and current public semantics except the documented unreachable F7 error result.
- All new C targets use C17 with extensions off and the project's existing MSVC/GCC/Clang warning flags. Tests work without Python.
- Don't delete `tests/cmake/mutate_fixture.py` until every C mutation is compared byte-for-byte against its output on identical inputs.
- Use disposable build-tree directories for generated data. Don't overwrite committed fixtures or maintainer-local ephemerides.
- Before implementation, inspect `git status`; work in an isolated worktree following the repo's workflow. Commit only when requested by the maintainer; otherwise keep changes reviewable without committing.
- Run WSL Linux builds and **all** offline tests from an independent copy/clone inside the WSL native filesystem (e.g. `$HOME/libjpl-review`), including `jpl_install` and `jpl_licenses`. Do not execute builds or tests against `/mnt/d/...`.

## File map and boundaries

| File | Responsibility in this plan |
| --- | --- |
| `CMakeLists.txt` | Shared library sources; private testing variant and five hook consumers; fixture tool target and test wiring |
| `tests/cmake/test_install.cmake` | Assert the installed archive has no defined `jpl_test_*` symbols |
| `tools/fixture_tool.c` (new) | Standalone binary-safe `canon-copy`, `flip`, `dup`, `retarget`, `recoord`, `crlf` commands; no libjpl linkage |
| `tests/cmake/check_numerical_reference.cmake` | Canonicalize reference/generator with the tool, then hash with CMake |
| `tests/cmake/check_fixture.cmake` | Canonicalize fixture text/rows using the same tool; binary hash stays raw |
| `tests/cmake/test_fixture_integrity.cmake` | Invoke C mutation commands in disposable work paths; retain all positive/negative controls |
| `tests/cmake/mutate_fixture.py` | Old reference for byte-for-byte equivalence, then remove |
| `src/jpl.c`, `src/jpl_private.h`, `tests/test_api_errors.c` | Guard quotient conversions; direct private seam for record-selector regression |
| `src/jpl_convenience.c`, `include/jpl_convenience.h` | Remove F7 dead branch and correct Moon and km-to-AU documentation |
| `docs/validation.md`, `CHANGELOG.md` | Record fresh acceptance evidence and user-visible updates |

---

### Task 1: Isolate test-only library hooks (F1)

**Files:** Modify `CMakeLists.txt:14-61,156-241`.

**Interfaces:** Produces internal CMake target `jpl_testing` with `LIBJPL_TESTING`; production/exported `jpl` never defines it. The five existing hook-dependent tests link `jpl_testing`, with their existing private macro definitions retained.

- [ ] **Step 1: Establish a red test.** Configure and build the existing default `jpl` target (`cmake -S . -B build/followup -DLIBJPL_FETCH_EPHEMERIDES=OFF`; `cmake --build build/followup --config Debug --target jpl`). Inspect its compile command or MSVC project definitions and confirm `LIBJPL_TESTING` is present when tests are ON; record this as F1's baseline. Also configure `-DLIBJPL_BUILD_TESTS=OFF` to confirm the contrast.
- [ ] **Step 2: Share sources and add the private variant.** At the top of `CMakeLists.txt`, introduce:

  ```cmake
  set(JPL_CORE_SOURCES src/jpl.c src/jpl_frames.c src/jpl_convenience.c src/jpl_date.c)
  add_library(jpl STATIC ${JPL_CORE_SOURCES})
  ```

  Keep the existing ERFA glob/filter and `target_sources(jpl PRIVATE ${ERFA_SOURCES})`; remove only the now-redundant three `target_sources(jpl PRIVATE src/jpl_frames.c)`/convenience/date lines. Immediately inside `if(LIBJPL_BUILD_TESTS)` add `add_library(jpl_testing STATIC ${JPL_CORE_SOURCES} ${ERFA_SOURCES})`, replicate `jpl`'s C17/`C_EXTENSIONS OFF`, include paths, ERFA version defines, PIC option, warning/CRT definitions, and conditional `JPL_NEED_M_LIB` link setting. Define `LIBJPL_TESTING` **only** on `jpl_testing` and the five test executables. Never install or export `jpl_testing`.
- [ ] **Step 3: Switch exactly five links.** Replace `PRIVATE jpl` with `PRIVATE jpl_testing` for `jpl_lighttime_tests`, `jpl_parser_tests`, `jpl_api_errors_tests`, `jpl_largefile`, `jpl_numeric_tests`; remove `target_compile_definitions(jpl PRIVATE LIBJPL_TESTING)`. Keep their own test-source `LIBJPL_TESTING` definitions for `jpl_private.h` declarations. Leave all other links to `jpl` intact.
- [ ] **Step 4: Verify both variants.** Build Debug and run `ctest --test-dir build/followup -C Debug --output-on-failure -R '^(jpl_lighttime|jpl_parser|jpl_api_errors|jpl_largefile|jpl_numeric)$'`. Expected: 5/5 pass; `jpl`'s compile command lacks `LIBJPL_TESTING`, `jpl_testing`'s contains it; tests-OFF configuration has no `jpl_testing` target. Don't claim the install-symbol guard is covered until Task 2.
- [ ] **Step 5: Review the diff.** Confirm both libraries consume identical core and filtered ERFA source lists, PIC and math flags, while the installed target list still contains only `jpl`.

### Task 2: Assert installed archive has no testing symbols (F1)

**Files:** Modify `tests/cmake/test_install.cmake:92-155` and `CMakeLists.txt:720-770` (tool-path forwarding if required).

**Interfaces:** `jpl_install` inspects its primary tests-ON archive and the existing custom-destination tests-OFF child archive; it fails if either defines any `jpl_test_*` symbol.

- [ ] **Step 1: Write the failing assertion.** Before the Case 3 consumer configure, locate the unique installed archive in `${PREFIX}/${LIBDIR}` (`jpl.lib` on MSVC, `libjpl.a` on POSIX; fail if neither or both), obtain symbol-table output from `dumpbin /symbols` for MSVC or `nm` for POSIX, and require the tool itself to succeed. Filter **defined** external symbols, including a possible platform-leading underscore, for `jpl_test_`; do not use a raw archive-text search, which may match undefined references or unrelated strings. Emit the symbol name and archive path on failure. Pick `dumpbin` via `find_program` or forward a concrete tool path from CMake; on POSIX prefer `${CMAKE_NM}` passed from the parent (`-DNM_TOOL=${CMAKE_NM}`) to guessing PATH. Call the same assertion again after the Case 6 tests-OFF child install, on `${CUSTOM_PREFIX}/custom-lib`, and keep `jpl_install` runnable on clang-cl.
- [ ] **Step 2: Verify the assertion detects a leak.** Before switching production `jpl` in Task 1, or using a disposable copy of the old archive in an owned build-tree prefix, run the assertion and require it to fail specifically on a `jpl_test_*` definition. If Task 1 is already applied, use a disposable `jpl` build with `-DLIBJPL_BUILD_TESTS=ON` and a *temporary build-tree-only* macro injection; never modify or install over a real package prefix.
- [ ] **Step 3: Run `ctest --test-dir build/followup -C Debug --output-on-failure -R '^jpl_install$'`.** Expected: pass with the real production `jpl` archive; still passes all relocated/spaces, custom-destination, PIC and C/C++ consumer cases. Repeat in Release at acceptance.

### Task 3: Remove dead km-to-AU outcome (F7)

**Files:** Modify `src/jpl_convenience.c:29-37`, `include/jpl_convenience.h:96-104`.

**Interfaces:** `jpl_km_to_au(double km, double *au)` retains its signature; for finite input and non-NULL output returns `JPL_OK`; otherwise `JPL_ERROR_ARGUMENT`. Does not change sibling `jpl_au_per_day_to_km_per_s`.

- [ ] **Step 1: Check baseline behavior.** Build and run `ctest --test-dir build/followup -C Debug --output-on-failure -R '^jpl_convenience$'`; review existing `tests/test_convenience.c:20-46` finite/NaN/NULL assertions.
- [ ] **Step 2: Remove only the unreachable branch.** After the two argument checks, set `*au = km / JPL_AU_KM; return JPL_OK;`; remove unused `tmp` and `if (!isfinite(tmp)) return JPL_ERROR_NUMERIC;`. In the function's Doxygen `@return`, remove only the NUMERIC outcome; preserve notes about output unchanged on failure and modern AU.
- [ ] **Step 3: Rebuild and run `jpl_convenience` and `jpl_examples`.** Expected: both pass, compiler warning-free. Inspect the header to ensure sibling conversion still documents/returns `JPL_ERROR_NUMERIC` on overflow.

### Task 4: Correct Moon documentation (F3)

**Files:** Modify `include/jpl_convenience.h:65-87`.

- [ ] **Step 1: Check the call path.** Confirm `src/jpl_convenience.c:117-119` delegates once to `jpl_planet_position(ephem, jd_tt, JPL_MOON, JPL_EARTH, state)` and that this path computes one light-time leg and one aberration correction.
- [ ] **Step 2: Replace the misleading note.** Use this scoped wording (retain existing Doxygen `@brief`, `@param`, `@return`, frame-gating note):

  ```c
  * @note This is one Earth-observer-to-Moon light-time leg with one
  *       aberration correction, using jpl_planet_position(). The velocity
  *       state[3..5] is retarded-epoch and unaberrated.
  * @note If a caller adds this result to a separate apparent leg to
  *       construct a third-body direction, the independently
  *       corrected legs have different emission epochs; that sum is only
  *       an approximation. For consistent geometry, start with geometric
  *       jpl_get_state() legs and apply the desired corrections explicitly.
  ```

- [ ] **Step 3: Review the rendered text.** Ensure it no longer implies the function itself adds two vectors. If Doxygen is installed, build `docs` and run `jpl_docs_symbols`; otherwise use the optional docs gate in Task 9. No new C test for a documentation-only edit.

### Task 5: Correct TT-TDB boundary comment (F4)

**Files:** Modify only `src/jpl.c:605-610`.

- [ ] **Step 1: Replace the stale explanation** with:

  ```c
  /* Span end (start-1)+coeff*sub is the exclusive upper index in
   * zero-based record doubles. A span ending at width is valid: its
   * last coefficient occupies slot width-1 and is interpolated. */
  ```

- [ ] **Step 2: Cross-check with `src/jpl.c:1463-1467`**, whose bounds condition uses `>` rather than `>=`, and run `ctest --test-dir build/followup -C Debug --output-on-failure -R '^jpl_tt_tdb$'`. Real DE440t testpo is optional if local files are available; record its result separately.

### Task 6: Bound floating-point quotients before integer casts (F5)

**Files:** Modify `src/jpl.c:1198-1237,1427-1436`, `src/jpl_private.h:79-97`, `tests/test_api_errors.c:685-712`.

**Interfaces:** Internal, `LIBJPL_TESTING`-guarded `jpl_test_select_record(const jpl_ephemeris *e, const double jd[2], size_t *index, double start[2], double end[2])` forwards to `select_record` for the regression. No installed header or public API changes.

- [ ] **Step 1: Write the failing test.** In `test_api_errors.c`, using its existing valid handle `e`, call the new testing seam with `jd={DBL_MAX,0.0}` and sentinel index/start/end outputs, expecting `JPL_ERROR_RANGE` and unchanged sentinels; add a normal in-coverage call that selects the known mock record index and scheduled boundaries, and confirm a public `jpl_get_state` at that epoch succeeds. Declare the seam in the `#ifdef LIBJPL_TESTING` block of `src/jpl_private.h`. First build should fail because the seam isn't defined; do not run the pre-fix out-of-range float-to-size_t conversion under UBSan until the guard is in place.
- [ ] **Step 2: Implement the selector guard and seam.** Before `(size_t)quotient_d`, reject nonfinite/negative values as before, compare the normalized query pair against header start/end directly and return `JPL_ERROR_RANGE` when outside. For `quotient_d >= (double)e->covered_record_count`, **clamp before the cast** to `covered_record_count - 1` if the two-part query is within coverage (retains endpoint/rounding behavior); otherwise return `JPL_ERROR_RANGE`. Only cast values strictly less than the covered count. Add the tiny `#ifdef LIBJPL_TESTING` wrapper after `select_record`.
- [ ] **Step 3: Guard the subinterval cast in `interpolate`.** After its existing `isfinite(qd)`/nonnegative check and before `(size_t)qd`, if `qd >= (double)nsub` set `subinterval = nsub - 1` (the verified `norm <= file_end` already bounds the real date); otherwise cast. Keep all existing two-part boundary verification and `free(record)` paths. Do **not** reject a valid final endpoint just because the scalar quotient rounded slightly above `nsub`.
- [ ] **Step 4: Run `ctest --test-dir build/followup -C Debug --output-on-failure -R '^(jpl_api_errors|jpl_parser|jpl_numeric|jpl_dates|jpl_tt_tdb)$'`.** Expected: all pass with unchanged ordinary endpoint behavior and direct out-of-range selector rejection. In Linux sanitizer build include `-fsanitize=undefined,float-cast-overflow` and run `jpl_api_errors` to check no floating-to-integer overflow.

### Task 7: Add byte-safe standalone fixture tool (F2/F6)

**Files:** Create `tools/fixture_tool.c`; modify `CMakeLists.txt:156-160,705-718`.

**Interfaces:** `jpl_fixture_tool canon-copy SRC DST`, `flip PATH`, `dup TEXT_IN TEXT_OUT ROWS_IN ROWS_OUT`, `retarget TEXT_IN TEXT_OUT ROWS_IN ROWS_OUT`, `recoord TEXT_IN TEXT_OUT`, `crlf TEXT_IN TEXT_OUT ROWS_IN ROWS_OUT`. All file I/O is binary; `canon-copy` rejects bare CR and returns nonzero with a diagnostic. No link to `jpl`.

- [ ] **Step 1: Add the executable with the existing test-only pattern.** Inside `if(LIBJPL_BUILD_TESTS)` add `add_executable(jpl_fixture_tool tools/fixture_tool.c)`, `target_compile_features(jpl_fixture_tool PRIVATE c_std_17)`, `set_target_properties(jpl_fixture_tool PROPERTIES C_EXTENSIONS OFF)`, and put it in the test target warning-flags loop. No install/export entry.
- [ ] **Step 2: Specify and test `canon-copy` before implementation.** Add a CTest driver `tests/cmake/test_fixture_tool.cmake` registered as `jpl_fixture_tool` with `-DTOOL=$<TARGET_FILE:jpl_fixture_tool>` and a unique `-DWORK` build-tree directory. Use CMake `file(WRITE)` to create `"A\r\nB\n"`, run `canon-copy`, assert `file(SHA256)` matches a separately written `"A\nB\n"` control. Run a bare-CR input `"A\rB"` and require nonzero status and a diagnostic containing `bare CR`. Add missing-input/invalid-mode failures and spaces in a work path. Run `ctest --test-dir build/followup -C Debug -R '^jpl_fixture_tool$' --output-on-failure`; expected initial failure (missing executable/subcommand).
- [ ] **Step 3: Implement binary I/O and `canon-copy`.** Use `fopen(...,"rb")`/`fopen(...,"wb")`, checked `fread`/`fwrite`/`fclose`, bounded allocation or buffered streaming, and binary input bytes; for each `\r` require the following byte `\n` and emit one `\n`, otherwise report `bare CR` and return nonzero. Ensure all diagnostics name the input/output path. Write only under `WORK` in the test driver. Run focused CTest; expected pass.
- [ ] **Step 4: Add mutation tests and implementations incrementally.** Preserve `tests/cmake/mutate_fixture.py` for comparison until Task 8. Match its exact semantics: `flip` XORs byte at offset 18855 with `0xff`; `dup` requires exactly 121 canonicalized lines and writes line 0 repeated at positions 0..119 followed by original line 120, LF-joined with final LF; `retarget` requires row index 60 containing `2453371.5  1  9  6` and replaces exactly that substring with `2453371.5  2  9  6`; `recoord` requires row index 89 containing `-0.02293917668434295246` and replaces it with `-0.02293927668434295246`; `crlf` canonicalizes existing CRLF to LF then expands each LF to CRLF. For `dup`/`retarget`, derive the rows output from whitespace tokens 2..5 of each resulting text line, joined by one space and a final LF (the Python `rows_in` parameter is accepted but unused for those modes). Require expected row counts/substrings rather than silently succeeding if the fixture changes. Use exact CLI arity checks and fail loudly for malformed input.
- [ ] **Step 5: Compare generated outputs byte-for-byte against old Python.** In a disposable build-tree directory while Python is available, invoke old `mutate_fixture.py` and the new tool for all five modes using the same source fixtures; compare results with `cmake -E compare_files` (all output files, including both rows files). Also compare `canon-copy` to the inline Python canonicalizer for the numerical header/generator and CRLF input. Expected: all equal; keep this development comparison out of the permanent Python-free default suite. Run `jpl_fixture_tool` CTest Debug and Release.

### Task 8: Migrate checkers and mutation driver; remove Python (F2/F6)

**Files:** Modify `CMakeLists.txt:274-318`, `tests/cmake/check_fixture.cmake`, `tests/cmake/check_numerical_reference.cmake`, `tests/cmake/test_fixture_integrity.cmake`; delete `tests/cmake/mutate_fixture.py` after comparison from Task 7.

**Interfaces:** `check_fixture.cmake` requires `FIXTURE_TOOL`, `WORK` in addition to its existing paths; `check_numerical_reference.cmake` requires `FIXTURE_TOOL`; negative fixture driver requires `FIXTURE_TOOL`, forwarding it and an owned per-case `WORK` to every nested checker call. CMake retains SHA-256 comparison and existing expected/actual diagnostics.

- [ ] **Step 1: Wire the target file path into registrations.** In `CMakeLists.txt`, pass `"-DFIXTURE_TOOL=$<TARGET_FILE:jpl_fixture_tool>"` to `jpl_fixture_integrity`, `jpl_fixture_integrity_negative`, and `jpl_numerical_reference_integrity`; give the fixture positive check an owned work dir `"-DWORK=${CMAKE_CURRENT_BINARY_DIR}/fixture_integrity"`. Existing negative/numerical `WORK` dirs remain distinct.
- [ ] **Step 2: Migrate `check_numerical_reference.cmake`.** Require `FIXTURE_TOOL` to exist, remove Python discovery and inline `-c`, and replace the `execute_process` command in `jpl_canonical_sha256()` with `"${FIXTURE_TOOL}" canon-copy "${src_path}" "${work_path}"`. Keep the existing `RESULT_VARIABLE` check and subsequent `file(SHA256)`/manifest comparisons. Focused `jpl_numerical_reference_integrity` must pass; a build-tree mutated header copy must still fail with expected and actual hashes.
- [ ] **Step 3: Migrate `check_fixture.cmake`.** Require `FIXTURE_TOOL`/`WORK`, create `WORK`, run `canon-copy` into `${WORK}/text.canonical` and `${WORK}/rows.canonical`, check each exit code, and `file(SHA256)` both outputs. Leave raw binary fixture hash unchanged. Replace the old `file(READ)`/`string(REPLACE)` text hashing only. Focused `jpl_fixture_integrity` must pass for LF and the negative driver's CRLF control.
- [ ] **Step 4: Migrate the negative driver.** Remove `PYTHON`/`find_program` block; require the passed tool to exist. Replace all five `${PYTHON} .../mutate_fixture.py` invocations with `${FIXTURE_TOOL}` plus identical mode and path args. For every nested `-P "${CHECKER}"`, include `"-DFIXTURE_TOOL=${FIXTURE_TOOL}"` and a unique owned `-DWORK` path such as `${WORK}/canonical_intact`, `${WORK}/canonical_flip`, `${WORK}/canonical_dup_text`, `${WORK}/canonical_dup_rows`, `${WORK}/canonical_retarget_text`, `${WORK}/canonical_retarget_rows`, `${WORK}/canonical_recoord`, `${WORK}/canonical_crlf` (never share the positive checker's canonical output with concurrently running CTest cases). Keep all existing hash and numerical-runner assertions; after Task 7 byte-equivalence evidence, delete `mutate_fixture.py`.
- [ ] **Step 5: Test false-pass resistance.** Run `ctest --test-dir build/followup -C Debug --output-on-failure -R '^(jpl_fixture_tool|jpl_fixture_integrity|jpl_fixture_integrity_negative|jpl_numerical_reference_integrity)$'`. Expected: all four pass; the flipped/duplicate/retarget/recoord negatives fail for their intended checker/runner category inside the driver, intact and CRLF controls pass. Feed a bare-CR fixture text copy to `check_fixture.cmake`; expected: explicit canonicalization failure, not a hash false pass. Search live default-suite scripts and `CMakeLists.txt` for `python`, `Python3`, `mutate_fixture.py`; expected: none (historical documentation is not a runtime dependency).

### Task 9: Evidence and full cross-platform acceptance

**Files:** Modify `docs/validation.md`, `CHANGELOG.md`; append a short F1–F7 outcome/evidence section to `docs/reviews/2026-09-19-current-review-resolution.md` or create `docs/reviews/2026-09-25-review-followup-resolution.md` (prefer the new file to preserve older checkpoints).

- [ ] **Step 1: Run fresh Windows gates** from the implementation worktree: `cmake -S . -B build/followup-accept -DLIBJPL_FETCH_EPHEMERIDES=OFF`, `cmake --build build/followup-accept --config Debug --parallel`, `ctest --test-dir build/followup-accept -C Debug --output-on-failure`, then repeat build and full CTest with `--config Release` / `-C Release`. Count and report all tests; do not silently exclude `jpl_install`, `jpl_licenses`, or fixture integrity. If Doxygen is installed, configure a docs-ON build, build `docs`, run `jpl_docs_symbols`, `jpl_docs_paths`, and `jpl_docs_only`.
- [ ] **Step 2: Run full native Linux gates.** Transfer the current reviewed sources to a **new owned directory in the WSL native filesystem** (a git clone of the current implementation branch into `$HOME/libjpl-review` is suitable once changes are committed; otherwise copy the exact tracked-plus-edited tree into a new path, excluding `build/` and `.worktrees/`). Check that the destination is absent before creating it; copying **from** `/mnt/d` once is fine, but do not run builds/tests there. Record the source revision/diff so Windows and Linux test the same bytes. Execute all subsequent commands from that native directory, **not** `/mnt/d`. For GCC Debug and Release in separate native `build/` dirs: `cmake -S . -B build/followup-linux-debug -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF -DCMAKE_C_FLAGS=-Werror=implicit-function-declaration`, `cmake --build build/followup-linux-debug --parallel`, `ctest --test-dir build/followup-linux-debug -C Debug --output-on-failure`; repeat with `-DCMAKE_BUILD_TYPE=Release` and `-C Release`. Run the **entire** offline suite, including `jpl_install`, `jpl_licenses`, `jpl_largefile`, `jpl_threads`, `jpl_mmap`; if an actual infrastructure limit remains, identify the test and evidence rather than labeling a subset full.
- [ ] **Step 3: Prove Python independence.** On the native Linux copy, execute the relevant CTest selection (`jpl_fixture_tool|jpl_fixture_integrity|jpl_fixture_integrity_negative|jpl_numerical_reference_integrity`) with a controlled PATH containing CMake and essential system tools but no `python`/`python3`, and verify `command -v python3`/`command -v python` return absent in that environment. Do not alter global PATH or system packages. Repeat a full suite in that environment if feasible; otherwise explain which tests need non-Python executables on PATH and keep the default-suite static search in Task 8 as additional evidence.
- [ ] **Step 4: Run sanitizer and packaging spot checks** where available: GCC ASan/UBSan/float-cast-overflow native build and its applicable offline suite; TSan on `jpl_threads|jpl_mmap`; `LIBJPL_BUILD_TESTS=OFF` build/install/consumer to ensure no test target or helper leaks. Optional locally-present real tier `jpl_testpo_440t` confirms the unchanged TT-TDB boundary comment is correct; never silently download large data.
- [ ] **Step 5: Update documentation with actual results.** In `CHANGELOG.md` under `[Unreleased]`, summarize installed-library isolation, removal of Python from default tests, corrected Moon documentation, quotient guards, and unit-helper cleanup without claiming a new release number. In `docs/validation.md`, record the `jpl_install` symbol check and the native-WSL full-suite procedure/results with exact pass counts and toolchains, making prior `/mnt/d` subset notes explicitly historical. In the new resolution log, map F1–F7 to changed files, regression tests and fresh Windows/Linux evidence; mark any unrun platform or real tier explicitly pending.
- [ ] **Step 6: Final review.** Run `git diff --check`, inspect `git diff --stat` and `git status --short`, verify no `src/erfa/*` edits, no generated work or local ephemeris files are staged, and that default/docs-only/tests-OFF CMake matrices still behave as documented. Present results and remaining platform limits to the maintainer.

## Execution handoff

Execute tasks in order: Task 1 provides the private test target for Task 6; Task 7 provides the tool consumed by Task 8; Task 9 records actual acceptance, including the user's native-WSL full-suite requirement. Keep the positive/negative tests for each delivered task and investigate any failure before moving on.
