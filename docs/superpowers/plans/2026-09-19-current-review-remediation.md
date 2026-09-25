<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Current Review Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve all 18 findings from the 2026-09-19 current-tree review with regression-backed, bisectable commits and release the corrected package identity as version 0.3.0.

**Architecture:** Work in dependency order on one dedicated branch. Establish deterministic tests first, then repair parser geometry and bounded reads, public status behavior, numerical stability, validator coverage, distribution and CI, package identity, provenance, and live documentation. Each task ends in one focused commit and leaves the full focused suite green.

**Tech Stack:** C17 with `C_EXTENSIONS OFF`, CMake 3.20+, CTest, MSVC/GCC/Clang/clang-cl, Doxygen 1.9+, GitHub Actions, vendored ERFA v2.0.1, native Windows/POSIX file and threading APIs.

**Spec:** `docs/superpowers/specs/2026-09-19-current-review-remediation-design.md`

## Global Constraints

- Do not begin execution until the maintainer explicitly authorizes it.
- At execution time, invoke `using-git-worktrees` before creating the isolated worktree. Create branch `bugfix/current-review-remediation` from an up-to-date `main`.
- Never modify, stage, or revert unrelated user changes. Never modify any file under `src/erfa/`.
- Use test-driven development for every behavior change: add the regression, run it and observe the expected failure, implement the smallest correct fix, then rerun focused and full tests.
- Tests use `JPL_TEST_CHECK`, explicit exit checks, or CMake `message(FATAL_ERROR)`. Do not use C `assert()` in executable test paths.
- Keep FILE and mmap parser behavior identical through shared validation logic.
- Preserve the standard DE on-disk format, public two-part-date API, modern-AU output convention, transactional computed outputs, and exactly three installed public headers.
- Public declarations retain complete Doxygen. Keep `WARN_AS_ERROR=YES` and the public-symbol inventory green.
- CMake minimum remains 3.20. Do not use newer commands or options without a version-compatible fallback.
- Agent shell commands must be PowerShell 5.1 compatible. Use `workdir`, quote paths, and use `cmd1; if ($?) { cmd2 }` for dependent commands.
- Before creating each build or worktree directory, verify its parent with `Test-Path -LiteralPath`.
- Every commit uses Conventional Commits with a concise subject and a body explaining behavior, regression evidence, and review finding IDs. Stage only files named by that task.
- Do not merge, tag, push, publish, or amend. Final merge is a maintainer-authorized `--no-ff` merge after every gate passes.

---

## File Map

- `src/jpl.c` stores layout counts, validates candidates, selects records, reads raw doubles, validates body arguments, preserves date statuses, and applies aberration.
- `src/jpl_private.h` holds private constants and the scale-safe norm helper.
- `include/jpl.h` documents target/center ranges and status behavior.
- `src/jpl_convenience.c` propagates date-normalization status and validates convenience arguments.
- `tests/test_parser.c` owns FILE/mmap layout, byte-order, endpoint-rounding, and bounded-open regressions.
- `tests/test_api_errors.c` owns public precedence, transactional output, date-status, and final-endpoint regressions.
- `tests/test_aberration.c` owns ERFA comparisons, extreme-scale behavior, aliasing, and transactionality.
- `tests/make_validator_data.c` creates the synthetic validator corpus and corruption cases.
- `tests/cmake/test_validators.cmake` runs one validator case against a pre-generated corpus.
- `tests/test_testpo.c` enforces reference grammar, numerical comparisons, and semantic coverage.
- `tests/test_all_ephemerides.c` performs the opt-in complete binary sweep.
- `cmake/LibjplTestData.cmake` resolves local/cache/download test data and publishes verified downloads.
- `tests/cmake/test_fetch_cache.cmake` exercises resolver reuse, failure, replacement, and rollback.
- `CMakeLists.txt` registers fixtures, docs-only tests, provenance checks, package tests, and real-data tiers.
- `.github/workflows/ci.yml` and `.github/workflows/ephemerides.yml` define platform and real-data gates.
- `tests/cmake/check_workflows.cmake` will enforce workflow permissions, action pins, labels, and local guards without a YAML dependency.
- `tests/cmake/check_numerical_reference.cmake` will verify canonical-LF generator/reference hashes.
- `tests/data/MANIFEST.txt` remains the machine-readable digest source.
- `tests/data/numerical_reference.md`, `README.md`, `LICENSES.md`, `CHANGELOG.md`, `docs/validation.md`, and `AGENTS.md` carry live contracts and execution guidance.
- `docs/reviews/2026-09-19-current-review-resolution.md` will record per-finding implementation revisions and fresh evidence.

---

### Task 00: Create the isolated branch and baseline ledger

**Files:**
- Create at execution time: isolated worktree outside the current checkout
- Create: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Consumes: approved design and this plan.
- Produces: branch `bugfix/current-review-remediation`, clean baseline evidence, and the permanent F01-F18 closure ledger used by every later task.

- [ ] **Step 1: Verify repository state without changing it.** Run `git status --short --branch`, `git log --oneline -10`, and `git worktree list`. Record the exact starting revision. If `main` is dirty, preserve those changes and create the worktree from committed `main`; do not stash or reset them.
- [ ] **Step 2: Invoke the `using-git-worktrees` skill and create the worktree.** Use branch name `bugfix/current-review-remediation`. Verify the worktree parent before creation. Do not create the branch in the current checkout.
- [ ] **Step 3: Materialize the approved planning documents in the branch.** Add byte-identical copies of `docs/superpowers/specs/2026-09-19-current-review-remediation-design.md` and `docs/superpowers/plans/2026-09-19-current-review-remediation.md` from the reviewed workspace. Compare them before staging; do not revise the approved scope during transfer. Commit:
```text
docs(plan): record current review remediation

Record the maintainer-approved F01-F18 design, dependency order,
regression requirements, commit boundaries, and merge gates before any
implementation changes.
```
- [ ] **Step 4: Configure a fresh offline baseline.** Verify the worktree's `build` parent, then run:
```powershell
cmake -S . -B build/baseline -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/baseline --config Debug --parallel
ctest --test-dir build/baseline -C Debug --output-on-failure
cmake --build build/baseline --config Release --parallel
ctest --test-dir build/baseline -C Release --output-on-failure
```
Expected: ordinary serial tests pass. Record the actual count and duration. Do not conceal skips.
- [ ] **Step 5: Reproduce F01 before any fix.** Run:
```powershell
ctest --test-dir build/baseline -C Debug --output-on-failure -R "^jpl_validator_" -j 16 --repeat until-fail:5
```
Expected: at least one validator fails because the shared corpus is regenerated concurrently. Save the failing test names and diagnostics in the ledger. If it does not reproduce, the source race still exists; record `SOURCE-CONFIRMED, NOT REPRODUCED` and continue.
- [ ] **Step 6: Create the ledger with one row per finding.** Use columns `ID`, `Task`, `State`, `Commit`, `Regression`, `Verification`, and `Remaining limits`. Initialize F01-F18 as `Open`; copy the finding-to-task map from the end of this plan.
- [ ] **Step 7: Commit only the ledger.** Stage the new resolution file and commit:
```text
docs(review): record current remediation baseline

Record the starting revision, clean offline Debug and Release results,
the parallel validator-race reproduction, and open F01-F18 ledger.
No implementation behavior changes in this commit.
```

### Task 01: Make validator fixtures parallel-safe

**Files:**
- Modify: `CMakeLists.txt:322-469`
- Modify: `tests/cmake/test_validators.cmake:18-36`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Consumes: executable `jpl_validator_data` and shared path `JPL_VALIDATOR_WORK`.
- Produces: CTest fixture `jpl_validator_corpus`, fixture setup test `jpl_validator_data_setup`, and read-only validator cases requiring that fixture.

- [ ] **Step 1: Add a CTest setup test before the validator cases.** Register:
```cmake
add_test(NAME jpl_validator_data_setup
    COMMAND jpl_validator_data "${JPL_VALIDATOR_WORK}")
set_tests_properties(jpl_validator_data_setup PROPERTIES
    FIXTURES_SETUP jpl_validator_corpus)
```
Set every `jpl_validator_*` case, including `jpl_fixture_read_error`, to `FIXTURES_REQUIRED jpl_validator_corpus`. Use one `set_tests_properties()` list after all registrations so no case is omitted.
- [ ] **Step 2: Remove generation from each case driver.** Delete the `GENERATOR` execution block at `tests/cmake/test_validators.cmake:25-35`. Keep `WORK` only for paths. Make the script fail immediately if the expected `EPH` or `TESTPO` file does not exist, except the fixture read-error case whose paths are source fixtures.
- [ ] **Step 3: Reconfigure and prove the old race is gone.** Run the setup plus validators in parallel:
```powershell
cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/remediation --config Debug --parallel
ctest --test-dir build/remediation -C Debug --output-on-failure -R "^jpl_validator_" -j 16 --repeat until-fail:10
```
Expected: every repetition passes. Confirm CTest automatically runs the setup fixture when a required validator is selected alone.
- [ ] **Step 4: Run all offline tests in parallel.** Run `ctest --test-dir build/remediation -C Debug --output-on-failure -j 16`. Expected: all tests pass without shared-file failures.
- [ ] **Step 5: Update F01 evidence and commit.** Commit:
```text
fix(tests): generate validator corpus once per run

Use a CTest fixture setup to create synthetic validator data once and
make every case consume it read-only. Repeated parallel execution no
longer truncates shared ephemerides or reference files. Resolves F01.
```

### Task 02: Separate covered and physical record counts

**Files:**
- Modify: `src/jpl.c` internal `jpl_ephemeris`, `validate_layout()`, open paths, clone path, and `select_record()`
- Modify: `tests/test_parser.c:339-400`
- Modify: `tests/test_api_errors.c:700-748`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Produces internal fields:
```c
size_t record_count;            /* physically readable data records */
size_t covered_record_count;    /* advertised intervals only */
```
- `jpl_record_count()` and `jpl_read_record()` continue using `record_count`.
- `select_record()` uses `covered_record_count` for all date indexing.

- [ ] **Step 1: Write the failing trailing-endpoint regression.** In the existing one-interval trailing mock, set the covered Mars coefficient to `5.0` and the trailing coefficient to `99.0`. Query `{2451546.0, 0.0}` through FILE and mmap. Assert `JPL_OK` and `state[0] == 5.0 / JPL_TEST_AU_KM`. Run `jpl_parser`; expected failure shows `99.0 / AU`.
- [ ] **Step 2: Extend layout output.** Change `validate_layout()` and `select_layout()` to return both physical and covered counts through separate output pointers. For accepted candidates set:
```c
*covered_record_count = expected_records;
*record_count = total_records - 2;
```
Reject zero covered intervals. Thread both outputs through FILE and mmap opens.
- [ ] **Step 3: Preserve counts through clones.** Copy `covered_record_count` alongside `record_count` for FILE and mmap clones. Do not change the public header or raw-record contract.
- [ ] **Step 4: Fix date selection.** In `select_record()`, require `covered_record_count > 0`. At the final advertised endpoint select `covered_record_count - 1`. Use that count for candidate clamping and binary-search upper bounds. Never use the physical trailing count in a date quotient.
- [ ] **Step 5: Run focused boundary suites.** Run Debug and Release `jpl_parser`, `jpl_api_errors`, `jpl_numeric`, `jpl_fixture`, and `jpl_testpo_parser`. Expected: all pass, and the new distinct trailing coefficient proves the fix.
- [ ] **Step 6: Update F02 and commit.** Commit:
```text
fix(core): keep trailing records out of date selection

Track advertised intervals separately from physically readable data
records. Exact coverage-end queries now select the final covered
interval while raw record APIs retain access to a trailing record.
Resolves F02.
```

### Task 03: Remove the constants-value layout discriminator

**Files:**
- Modify: `src/jpl.c:478-512`
- Modify: `tests/test_parser.c`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Consumes: complete endpoint and unique-candidate checks already in `validate_layout()`.
- Produces: candidate validity independent of numeric constant values.

- [ ] **Step 1: Add a valid collision mock.** Clone the existing one-record little-endian Mars mock. Set `constant_count = 2`, `constants[0] = start_jd`, and retain valid distinct record endpoints and coefficients. Assert both `jpl_open()` and `jpl_open_mmap()` succeed, constants preserve the equal value, and state interpolation succeeds. Run `jpl_parser`; expected: both opens return `JPL_ERROR_FORMAT`.
- [ ] **Step 2: Remove only the value heuristic.** Delete the read and comparison of constants slot zero and data slot zero. Keep `header_record_endpoints()` as the first data-record validation. Do not replace the heuristic with another comparison against coefficient or padding values.
- [ ] **Step 3: Verify ambiguity remains rejected.** Run `jpl_parser` and confirm the deliberately ambiguous-width fixture still returns `JPL_ERROR_FORMAT` because both complete split candidates survive, not because payload values differ.
- [ ] **Step 4: Run FILE/mmap and real-fixture suites.** Run Debug and Release `jpl_parser`, `jpl_mmap`, `jpl_fixture`, and `jpl_real_ephemeris` when registered.
- [ ] **Step 5: Update F03 and commit.** Commit:
```text
fix(parser): validate layouts without payload-value heuristics

Stop treating equality between a numeric constant and the first record
date as evidence of a phantom layout. Geometry and scheduled endpoints
remain the sole layout discriminators. Resolves F03.
```

### Task 04: Bound parser reads independently of record width

**Files:**
- Modify: `src/jpl.c:345-615`, `parse_header()` support code
- Modify: `tests/test_parser.c`
- Modify: `tests/test_largefile.c`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Add private helper:
```c
static void parse_optional_tt_tdb(jpl_header *header,
                                  const unsigned char tail[24],
                                  jpl_byte_order order);
```
- Add test-only parser read limiter under `LIBJPL_TESTING`:
```c
void jpl_test_set_parser_read_limit(size_t max_bytes);
```
- `parse_header()` parses only the classic 2856-byte fields and leaves TT-TDB tentative and absent.
- `validate_layout()` reads 24 optional bytes directly after candidate geometry establishes the tail offset.

- [ ] **Step 1: Add bounded-read instrumentation under `LIBJPL_TESTING`.** Route every parser `source->read()` call through one private wrapper. The wrapper returns `JPL_ERROR_IO` when a nonzero test limit is set and `count` exceeds it; production builds compile to the direct read. Declare `jpl_test_set_parser_read_limit()` only in `src/jpl_private.h` under `LIBJPL_TESTING`. Setting zero disables the limit. Keep this hook out of installed headers.
- [ ] **Step 2: Add a safe wide-record regression.** Create a valid mock with a 1 MiB record width, one advertised interval, and ordinary fixed header/endpoints. Set the parser read limit to 2880 bytes, open through FILE and mmap, then reset the limit even on failure. Before the fix, full-record parsing requests 1 MiB and both opens fail; after the fix, no parser read exceeds 2880 bytes and both opens succeed. This proves bounded behavior without attempting a dangerous multi-gigabyte allocation.
- [ ] **Step 3: Split classic parsing from optional-tail parsing.** Keep `parse_header()` bounded to `CLASSIC_HEADER_BYTES`. Move the T14/T15 24-byte decoding at the current tail into `parse_optional_tt_tdb()`. The helper receives exactly 24 bytes, reads T15 from offset 12, checks integer bounds, and leaves `has_tt_tdb = 0` until span validation.
- [ ] **Step 4: Remove overflow-window allocation.** Delete the `candidate.constant_count > JPL_MAX_CONSTANT_NAMES` `malloc(need)` preparse. Compute the tail offset with checked arithmetic but do not read skipped overflow names.
- [ ] **Step 5: Remove full-record allocation.** Delete `malloc(width)` and the full candidate-header read. Reuse the classic parsed header. If `tail + 24 <= width`, call the bounded source wrapper for `raw_tail[24]` and parse it. If the tail is outside the header record, treat TT-TDB as absent under the existing policy.
- [ ] **Step 6: Preserve all geometry checks.** Set `record_double_count = width / 8` only after classic pointer extents fit. Apply `tt_tdb_span_end()` to the tentative tail exactly as today, accepting `end <= width_in_doubles`. Do not allow optional metadata to enlarge width.
- [ ] **Step 7: Run parser matrix.** Run `jpl_parser`, `jpl_largefile`, `jpl_tt_tdb`, `jpl_fixture`, and big-endian tests in Debug and Release. Expected: overflow-name and DE440t boundary cases remain green; the read-limit hook reports no request above 2880 bytes.
- [ ] **Step 8: Update F06 and commit.** Commit:
```text
fix(parser): bound header reads for wide records

Parse the fixed classic header and optional 24-byte tail directly
instead of allocating overflow-name windows or whole candidate records.
Hostile sparse files can no longer drive multi-gigabyte open-time
allocations. Resolves F06.
```

### Task 05: Scale endpoint tolerance and handle zero-length reads

**Files:**
- Modify: `src/jpl.c:307-322`, `src/jpl.c:899-917`
- Modify: `tests/test_parser.c`
- Modify: `tests/test_jpl.c` or `tests/test_api_errors.c`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Add private helper:
```c
static double endpoint_ulp_tolerance(const double scheduled[2], double encoded);
```
- `read_doubles(..., count == 0)` returns `JPL_OK` without backend access.

- [ ] **Step 1: Add a large-epoch rounding regression.** Build a valid mock near JD `80000000.0` with a non-binary step, independently encode scheduled data endpoints as binary64, and assert FILE/mmap opens succeed. Perturb one endpoint by at least eight local ULPs and assert both opens fail. Before the fix, the valid mock must fail under the fixed modern-epoch tolerance.
- [ ] **Step 2: Implement local-ULP tolerance.** Use `nextafter()` in both directions around `encoded` and `scheduled[0]`. Select the maximum finite adjacent spacing and multiply by four. If multiplication is nonfinite, return `JPL_ERROR_FORMAT`. Compare the compensated endpoint error against that tolerance. Remove `JPL_T05_ENDPOINT_TOL_DAYS` and its modern-JD-only comment.
- [ ] **Step 3: Add zero-constant foreign-endian coverage.** Create a valid big-endian zero-constant mock. Pass a non-NULL one-element destination to `jpl_read_constants()` and assert `JPL_OK` through FILE and mmap, with the sentinel unchanged.
- [ ] **Step 4: Short-circuit zero reads.** In `read_doubles()`, after validated function arguments and before backend branching, add `if (count == 0) return JPL_OK;`. Do not seek or evaluate mapping base for a zero count.
- [ ] **Step 5: Run focused tests in Debug and Release.** Run `jpl_parser`, `jpl_tests`, `jpl_api_errors`, and `jpl_largefile`.
- [ ] **Step 6: Update F13/F14 and commit.** Commit:
```text
fix(parser): scale endpoint checks and zero-length reads

Derive encoded endpoint tolerance from local binary64 spacing across the
supported epoch range and make zero-count raw reads allocator-independent.
Resolves F13 and F14.
```

### Task 06: Enforce public argument precedence and numeric date statuses

**Files:**
- Modify: `src/jpl.c:1312-1820`
- Modify: `src/jpl_convenience.c`
- Modify: `include/jpl.h:256-337`
- Modify: relevant Doxygen in `include/jpl_convenience.h`
- Modify: `tests/test_api_errors.c`
- Modify: `tests/test_convenience.c`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Add private body validation helpers in `src/jpl.c`:
```c
static int valid_state_target(int target);
static int valid_state_center(int target, int center);
static int valid_lighttime_body(int target, int center);
```
- Preserve `JPL_ERROR_NUMERIC` from finite `jpl_date_normalize()` failures.

- [ ] **Step 1: Add precedence tests.** For invalid target `99`, invalid center `99`, target zero, angular target with nonzero center, and TT-TDB target through `jpl_get_state()`, combine each with an out-of-range date and a missing dataset. Assert `JPL_ERROR_ARGUMENT` and unchanged output. Before fix, at least invalid target/out-of-range and valid target/invalid center/missing data return RANGE or UNSUPPORTED.
- [ ] **Step 2: Add finite-overflow date tests.** Pass `{DBL_MAX, DBL_MAX}` through state, light-time, TT-TDB, planet, Moon, frame, and TT/TDB conversion APIs. Assert `JPL_ERROR_NUMERIC` and unchanged outputs. Keep `{NAN, 0}` and `{INFINITY, 0}` expectations as `JPL_ERROR_ARGUMENT`.
- [ ] **Step 3: Centralize body validation.** Validate target/center combinations immediately after required pointer checks and before date normalization or data access. Keep angular rules explicit. Do not call `barycentric_state()` with an invalid code.
- [ ] **Step 4: Preserve normalization status.** Replace unconditional mappings such as `if (status != JPL_OK) return JPL_ERROR_ARGUMENT;` with:
```c
if (status != JPL_OK) return status;
```
after explicit nonfinite-part checks. Apply the same rule in convenience functions. File-header normalization failures remain `JPL_ERROR_FORMAT`.
- [ ] **Step 5: Correct public light-time ranges.** In `include/jpl.h`, state exactly: target 1-13, center 0-13. Remove the note claiming bodies 0-13 are valid targets. Document finite-overflow pairs as NUMERIC.
- [ ] **Step 6: Run API, convenience, numeric, light-time, frames, TT-TDB, docs-symbol, and examples tests in Debug and Release.** Expected: all pass and outputs remain transactional.
- [ ] **Step 7: Update F04/F12 and the target-zero part of F18, then commit.** Commit:
```text
fix(api): validate bodies before dates and data access

Reject invalid target and center combinations before coverage or dataset
checks, preserve NUMERIC for finite unrepresentable date pairs, and make
the light-time target range unambiguous. Resolves F04, F12, and part of
F18.
```

### Task 07: Make aberration's final normalization scale-safe

**Files:**
- Modify: `src/jpl.c:1916-1935`
- Modify: `tests/test_aberration.c`
- Modify: `tests/test_numeric.c`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Consumes: `jpl_vec_norm3()` from `src/jpl_private.h`.
- Produces: finite normalized output whenever `p[3]` and the final scaled coordinates are representable.

- [ ] **Step 1: Add the failing tiny-Sun regression.** Use a unit-scale nonradial state, finite subluminal observer velocity, and `sun_distance_au = 1e-200`. Compute the expected direction using ERFA `eraAb()` or an independent scale-safe normalization of the same published expression. Assert `JPL_OK`, finite output, matching direction within `1e-12`, preserved distance, unchanged velocity, and exact in-place aliasing. Before fix, direct squaring returns `JPL_ERROR_NUMERIC`.
- [ ] **Step 2: Keep the genuine overflow regression.** Retain `sun_distance_au = 1e-320` expecting NUMERIC because `SRS / distance` itself overflows. This distinguishes avoidable norm overflow from an unrepresentable solar term.
- [ ] **Step 3: Replace direct sum of squares.** Build all three finite `p[i]` values first, then call:
```c
rn = jpl_vec_norm3(p);
if (!isfinite(rn) || !(rn > 0.0)) return JPL_ERROR_NUMERIC;
```
Do not accumulate `p[i] * p[i]`.
- [ ] **Step 4: Run `jpl_aberration`, `jpl_numeric`, `jpl_convenience`, and examples in Debug and Release.** Run ASan/UBSan focused binaries when available.
- [ ] **Step 5: Update F05 and commit.** Commit:
```text
fix(numeric): normalize aberration output without squaring overflow

Use the shared scale-safe norm for the final aberration direction so
finite large intermediates remain representable, while preserving
NUMERIC for a genuinely overflowing solar term. Resolves F05.
```

### Task 08: Require diverse testpo coverage

**Files:**
- Modify: `tests/test_testpo.c`
- Modify: `tests/make_validator_data.c`
- Modify: `CMakeLists.txt` validator registrations
- Modify: `tests/cmake/test_validators.cmake` only if a new expected diagnostic category is needed
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Add test-only coverage matrix:
```c
size_t body_component[18][7];
```
- Add generated `duplicate_only.testpo` and CTest `jpl_validator_duplicate_only` expecting `insufficient reference coverage`.

- [ ] **Step 1: Generate the duplicate-only negative.** Write a framed file with 120 identical valid Mars X1 rows. Register a negative validator case. Before runner changes it must exit zero; configure the CMake driver to expect failure and observe the test fail.
- [ ] **Step 2: Expand good synthetic rows.** Ensure the good corpus has Mars X1-X6, nutation X1-X4, libration X1-X6, and TT-TDB X1-X2. Use the actual mock coefficients and zero rates as exact expected values. Keep all three supported row grammars represented.
- [ ] **Step 3: Track coverage by target and component.** Increment `body_component[target][component]` only after a numerical comparison succeeds. Do not let angular rows satisfy ordinary-body component requirements.
- [ ] **Step 4: Validate requirements after parsing.** Require at least one available ordinary body target in 1-13 to have all six components. If nutation metadata is present, require T14 X1-X4. If libration metadata is present, require T15 X1-X6. If TT-TDB is present, require T17 X1-X2. Keep the 100-row minimum as an independent truncation guard. Emit `insufficient reference coverage: ...` naming every missing target/component.
- [ ] **Step 5: Run all synthetic validator tests serially and with `-j 16 --repeat until-fail:5`.** Expected: good passes, duplicate-only fails for the intended category, all prior negatives retain their category.
- [ ] **Step 6: Run DE440, DE440t, and available local testpo files.** If an authoritative file lacks one required component, inspect its documented format before changing policy. Do not weaken coverage merely to make it green; record the exact upstream omission and seek maintainer approval.
- [ ] **Step 7: Update F10 and commit.** Commit:
```text
fix(validators): require reference component coverage

Track successful rows by target and component and reject duplicate-heavy
files that meet the row threshold without exercising complete present
datasets. Resolves F10.
```

### Task 09: Scan every record in the ephemeris sweep

**Files:**
- Modify: `tests/test_all_ephemerides.c:147-197`
- Modify: `tests/make_validator_data.c` sweep fixtures
- Modify: `tests/cmake/test_validators.cmake` sweep expectations
- Modify: `docs/validation.md`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- The sweep iterates `index = 0 .. jpl_record_count(ephemeris)-1` and validates every decoded record.

- [ ] **Step 1: Create an interior-corruption sweep fixture.** Generate a named file with at least three covered records, clean first and last endpoints, and a NaN or reversed date in record one. Add it to a separate sweep directory so open succeeds but the sweep must fail. Before the fix the sweep exits zero.
- [ ] **Step 2: Replace the first-record-only check with a loop.** Reuse one allocated record buffer. For every physical record call `jpl_read_record()`, reject nonfinite start/end with `isfinite()`, reject `end <= start`, and verify continuity with the previous end. Require record zero to start at coverage start. Require the final covered record to end at coverage end; allow one trailing record to start at coverage end and remain ordered without extending public coverage.
- [ ] **Step 3: Improve diagnostics.** Include filename and zero-based record index in read, finiteness, ordering, discontinuity, and coverage-anchor errors. Remove the manual NaN bit inspection because ordinary `isfinite()` is clearer and also rejects infinities.
- [ ] **Step 4: Update the success text and validation docs.** State that every physical record was decoded and checked. Note that this opt-in tier intentionally performs a full sequential scan and can read several gigabytes.
- [ ] **Step 5: Run synthetic sweep negative/control, committed fixture, DE440/DE440t, and available local binaries.** Expected: corruption fails at record one; all authoritative files pass.
- [ ] **Step 6: Update F11 and commit.** Commit:
```text
fix(validators): scan every ephemeris data record

Decode and validate all physical records in the opt-in sweep, including
interior date geometry and trailing-record continuity. Arbitrary middle
record corruption can no longer pass the sweep. Resolves F11.
```

### Task 10: Publish fetched cache entries transactionally

**Files:**
- Modify: `cmake/LibjplTestData.cmake:110-166`
- Modify: `tests/cmake/test_fetch_cache.cmake`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Test-only CMake variable: `LIBJPL_TEST_FORCE_PUBLISH_FAILURE`, default false.
- Sibling backup path: `${dest}.replace-backup`.

- [ ] **Step 1: Add the forced-failure regression.** Create a stale destination with known bytes and a valid downloadable replacement. Run the resolver with `-DLIBJPL_TEST_FORCE_PUBLISH_FAILURE=ON`. Assert configure fails with `forced publish failure`, destination retains the original hash and size, and neither `.fetch-tmp` nor `.replace-backup` remains. Before fix, the switch is ignored; the test must fail.
- [ ] **Step 2: Guard stale backup state.** If `${dest}.replace-backup` exists, remove the fetched temporary file and fail without touching destination or backup. This avoids guessing which file is authoritative after an interrupted earlier replacement.
- [ ] **Step 3: Move the old destination using a checked subprocess.** Use `${CMAKE_COMMAND} -E rename` through `execute_process(RESULT_VARIABLE ...)` so CMake 3.20 can inspect failure. If moving destination to backup fails, remove only the temp and fail.
- [ ] **Step 4: Publish or roll back.** Under the test switch, skip the publish command and synthesize a nonzero result. Otherwise rename temp to destination through `execute_process`. On failure, rename backup back to destination, remove temp, verify restoration, then issue `FATAL_ERROR`. If rollback itself fails, name both paths and preserve the backup for manual recovery.
- [ ] **Step 5: Complete successful replacement.** Verify destination exists and temp does not, then remove backup. Recompute destination size/hash once after publication as a defensive assertion.
- [ ] **Step 6: Run `jpl_fetch_cache` repeatedly and perform an offline second configure against a populated cache.** Expected: successful replacement, forced rollback, invalid download, and cache reuse all preserve documented files.
- [ ] **Step 7: Update F07 and commit.** Commit:
```text
fix(fetch): restore stale cache when publication fails

Back up an existing destination, publish the verified temporary file
through checked rename operations, and roll back on failure. A forced
failure regression proves the prior cache survives. Resolves F07.
```

### Task 11: Make the maintainer-local workflow prove local coverage

**Files:**
- Modify: `.github/workflows/ephemerides.yml:49-66`
- Create: `tests/cmake/check_workflows.cmake`
- Modify: `CMakeLists.txt` to register `jpl_workflow_policy`
- Modify: `tests/license_inventory.cmake`
- Modify: `docs/validation.md`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Dedicated runner labels: `[self-hosted, libjpl-ephemerides]`.
- Required repository variables: `JPLEPH`, `JPL_TESTPO`.

- [ ] **Step 1: Add a failing workflow policy test.** The CMake script reads both workflow files and requires the dedicated local label, nonempty-variable guard commands, explicit test registration inspection, and no bare `[self-hosted]` local job. Register it in the offline suite and run it; expected failure on current YAML.
- [ ] **Step 2: Require the dedicated runner.** Change local `runs-on` to `[self-hosted, libjpl-ephemerides]`.
- [ ] **Step 3: Add a preflight step.** Use the runner's PowerShell shell explicitly and fail when either variable is empty or either directory does not exist:
```powershell
if ([string]::IsNullOrWhiteSpace($env:JPLEPH)) { throw 'JPLEPH repository variable is required' }
if ([string]::IsNullOrWhiteSpace($env:JPL_TESTPO)) { throw 'JPL_TESTPO repository variable is required' }
if (-not (Test-Path -LiteralPath $env:JPLEPH -PathType Container)) { throw "JPLEPH directory not found: $env:JPLEPH" }
if (-not (Test-Path -LiteralPath $env:JPL_TESTPO -PathType Container)) { throw "JPL_TESTPO directory not found: $env:JPL_TESTPO" }
```
- [ ] **Step 4: Prove registration before execution.** Run `ctest -N` into a file, require `jpl_all_ephemerides_local`, and require at least one `jpl_testpo_` name other than parser/440/440t. Then run only `jpl_all_ephemerides_local` and `jpl_testpo_[0-9]` local registrations. Do not use a regex that can pass on parser-only tests.
- [ ] **Step 5: Update validation guidance and license inventory.** Document the required label and variables. Add the new CMake checker to first-party inventory.
- [ ] **Step 6: Run the workflow policy test locally.** If the configured maintainer runner is available, dispatch the workflow and retain its URL; otherwise mark hosted execution pending without claiming a pass.
- [ ] **Step 7: Update F08 and commit.** Commit:
```text
fix(ci): require evidence from the local ephemeris sweep

Gate the maintainer job on dedicated runner data, verify local suites are
registered, and prevent downloaded or parser-only tests from satisfying
the local sweep. Resolves F08.
```

### Task 12: Enable docs checks without ordinary tests

**Files:**
- Modify: `CMakeLists.txt:121-155`
- Create: `tests/cmake/test_docs_only.cmake`
- Modify: `tests/license_inventory.cmake`
- Modify: `AGENTS.md`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- CTest is enabled unconditionally.
- `LIBJPL_BUILD_TESTS=OFF` still suppresses all ordinary test executables.
- `LIBJPL_BUILD_DOCS=ON` registers exactly `jpl_docs_symbols` and `jpl_docs_paths` from this project.

- [ ] **Step 1: Add a configure-matrix test.** The new CMake script creates a disposable nested build with docs ON/tests OFF, builds `docs`, runs `ctest -N`, and requires both docs test names while rejecting `jpl_tests` and `jpl_parser`. Register it only in tests-enabled parent builds to avoid recursive self-registration.
- [ ] **Step 2: Observe failure.** Run `jpl_docs_only`; expected: nested `ctest -N` reports zero tests.
- [ ] **Step 3: Enable CTest before docs registration.** Move `enable_testing()` outside `if(LIBJPL_BUILD_TESTS)` and place it before the docs block. Keep every ordinary executable and ordinary `add_test()` inside the option block.
- [ ] **Step 4: Run docs-only, docs symbols, docs paths, licenses, and install tests.** Verify paths with spaces remain supported.
- [ ] **Step 5: Update guidance and inventory.** State that docs checks exist with docs ON even when ordinary tests are OFF. Add the new script to the inventory.
- [ ] **Step 6: Update F15 and commit.** Commit:
```text
fix(cmake): register docs checks independently of unit tests

Enable CTest before the docs block and add a configuration-matrix
regression proving docs ON plus tests OFF still runs symbol and path
checks. Resolves F15.
```

### Task 13: Pin numerical-reference provenance

**Files:**
- Modify: `tests/data/MANIFEST.txt`
- Modify: `tests/data/numerical_reference.md:15-24`
- Create: `tests/cmake/check_numerical_reference.cmake`
- Modify: `CMakeLists.txt`
- Modify: `tests/license_inventory.cmake`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Manifest keys:
```text
numerical.reference.sha256=0a0f75d9335f94fcaafbdc2680c032747222c02269c29697472a031382e544f8
numerical.generator.sha256=dfa98d5517dfe966984e621932d856c435e23e4bbd3d1ff9ba33ef48ed4e6a42
```
- Hashes are over canonical LF content, where CRLF is replaced with LF and no other bytes are changed.

- [ ] **Step 1: Independently regenerate the reference to a new build-tree path.** Build `jpl_generate_reference`, run it twice to two new files, canonicalize LF, and require byte-identical outputs. Compare generated values to the checked-in header before accepting hashes. If values differ, stop and investigate rather than updating hashes mechanically.
- [ ] **Step 2: Add the manifest keys and update prose.** Use the exact canonical hashes above only after Step 1 confirms them. Explain canonicalization in `numerical_reference.md`.
- [ ] **Step 3: Add the checker.** Read the two keys from the manifest, read each source file, replace `\r\n` with `\n`, reject remaining bare `\r`, write canonical copies under a supplied build `WORK` path, and compare `file(SHA256)` to the manifest. Emit expected and actual values on failure.
- [ ] **Step 4: Register `jpl_numerical_reference_integrity`.** Pass manifest, header, generator, and a build-tree work path. Add a negative mode in the script or a companion invocation that mutates a build-tree copy and must fail.
- [ ] **Step 5: Run integrity, numerical, frame, aberration, and light-time tests.** Expected: positive and negative integrity controls pass.
- [ ] **Step 6: Update inventory and F16, then commit.** Commit:
```text
test(provenance): enforce numerical reference hashes

Pin canonical-LF hashes for the reviewed numerical header and its
independent generator and verify both in the offline suite. Resolves F16.
```

### Task 14: Assign package version 0.3.0

**Files:**
- Modify: `CMakeLists.txt:6`, package-test registration near line 695
- Modify: `tests/cmake/test_install.cmake`
- Modify: `CHANGELOG.md`
- Modify: `README.md` only if it names the package version
- Modify: `docs/migration-two-part-dates.md` only if it names 0.2.0 as the new API version
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- `PROJECT_VERSION` becomes exactly `0.3.0`.
- Installed `libjplConfigVersion.cmake` retains `ExactVersion` compatibility.

- [ ] **Step 1: Strengthen install version tests before changing CMake.** Require installed version content to mention `0.3.0`. Configure consumers with exact `0.3.0` and require success. Configure exact `0.2.0` and `0.3.1` consumers and require failure. Run `jpl_install`; expected failure because current project reports 0.2.0.
- [ ] **Step 2: Change project version.** Set `project(libjpl VERSION 0.3.0 LANGUAGES C)`. Do not change ERFA's `PACKAGE_VERSION="2.0.1"` definitions; they belong to vendored ERFA and are unrelated.
- [ ] **Step 3: Update changelog structure.** Leave `## [Unreleased]` at the top for future changes. Add `## [0.3.0] - 2026-09-19` containing the breaking two-part-date and modern-AU changes plus concise entries for F01-F18. Preserve the historical `0.2.0` section.
- [ ] **Step 4: Run install tests in Debug and Release.** Verify C and C++ consumers, relocation, custom include/lib paths, spaces, exact match, and both mismatch cases.
- [ ] **Step 5: Search live files for stale package claims.** Use a content search for `0.2.0`; leave historical review/spec references unchanged and update only live package or migration claims.
- [ ] **Step 6: Update F09 and commit.** Commit:
```text
chore(release): assign version 0.3.0 to breaking API

Give the two-part-date API and current remediation a package identity
distinct from tagged 0.2.0, retain exact-version compatibility, and test
both older and newer mismatch requests. Resolves F09.
```

### Task 15: Pin workflow actions and permissions

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `.github/workflows/ephemerides.yml`
- Modify: `tests/cmake/check_workflows.cmake`
- Modify: `docs/validation.md`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Approved pins as resolved on 2026-09-19:
```yaml
actions/checkout@11d5960a326750d5838078e36cf38b85af677262 # v4
actions/cache@0057852bfaa89a56745cba8c7296529d2fc39830 # v4
actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4
ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756 # v1
```

- [ ] **Step 1: Extend the workflow policy test.** Require top-level `permissions` with `contents: read`, reject `uses:` values ending in `@v1` or `@v4`, require every external `uses:` reference to end in exactly 40 lowercase hex characters before an optional comment, and require the four approved owner/repository pairs.
- [ ] **Step 2: Observe failure against mutable tags and missing permissions.** Run `jpl_workflow_policy`; expected failure.
- [ ] **Step 3: Set least privilege.** Add:
```yaml
permissions:
  contents: read
```
to both workflows before `jobs:`. Do not add write permissions for cache or artifact operations.
- [ ] **Step 4: Replace every action tag with the approved SHA and retain the release comment.** Ensure no mutable external action remains.
- [ ] **Step 5: Run workflow policy and parse both YAML files through GitHub Actions validation when available.** Dispatch is not required for syntax inspection, but actual hosted jobs remain part of Task 18.
- [ ] **Step 6: Update docs and F17, then commit.** Commit:
```text
fix(ci): pin actions and restrict workflow permissions

Use reviewed immutable action commits and explicit read-only repository
permissions in offline and real-data workflows. Resolves F17.
```

### Task 16: Correct remaining live documentation

**Files:**
- Modify: `LICENSES.md:11-19`
- Modify: `README.md:122-137`
- Modify: `AGENTS.md` where test-tier wording needs alignment
- Modify: `docs/validation.md`
- Modify: `tests/cmake/check_licenses.cmake` or add a small Markdown-link check in `tests/cmake/check_docs.cmake`
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`

**Interfaces:**
- Root license link is `LICENSE`.
- Fetch documentation states exact size and SHA-256 checks for all four downloaded files.

- [ ] **Step 1: Add a local-link regression.** Extend a docs/license checker to extract the `LICENSES.md` project-license link and require that it resolves inside the source root to an existing regular file. Run it before editing; expected failure because `../LICENSE` resolves outside the root.
- [ ] **Step 2: Fix the license link.** Change `[`LICENSE`](../LICENSE)` to `[`LICENSE`](LICENSE)` and leave installed notice paths unchanged.
- [ ] **Step 3: Correct fetch integrity wording.** State that DE440, DE440t, testpo.440, and testpo.440t all receive exact-size and SHA-256 verification from `tests/data/MANIFEST.txt` when downloaded. Preserve the explicit policy that maintainer-local alternate spans are used as-is and validated by reference suites.
- [ ] **Step 4: Audit all live docs against completed tasks.** Check target ranges, docs-only tests, validator fixture setup, complete sweep cost, cache rollback, local workflow prerequisites, workflow pins, numerical hashes, and version 0.3.0. Historical specs/plans/reviews remain unchanged.
- [ ] **Step 5: Run `jpl_licenses`, docs symbol/path checks, examples, fetch-cache, workflow policy, and a Markdown link scan.** Expected: all pass.
- [ ] **Step 6: Update the remaining F18 evidence and commit.** Commit:
```text
docs(project): align notices and validation guidance

Repair the root license link and align fetch, workflow, sweep, package,
and test guidance with implemented behavior. Completes F18.
```

### Task 17: Full cross-platform and real-data acceptance

**Files:**
- Modify: `docs/reviews/2026-09-19-current-review-resolution.md`
- Modify: `docs/validation.md`
- Modify implementation/test files only if a gate exposes a new defect; each such fix requires a separate regression and commit before resuming this task.

**Interfaces:**
- Consumes: Tasks 01-16.
- Produces: fresh acceptance evidence and no unresolved F01-F18 rows.

- [ ] **Step 1: Inspect branch history and finding map.** Run `git log --oneline main..HEAD`, `git diff --stat main...HEAD`, and `git diff --check`. Confirm one focused implementation commit per task and no `src/erfa/` changes.
- [ ] **Step 2: Run clean Windows MSVC builds.** From fresh Debug/Release-capable build directories run configure, build, serial CTest, and `ctest -j 16`. Require zero warnings and zero failures in both configurations.
- [ ] **Step 3: Run validator stress.** Run `ctest -R "^jpl_validator_" -j 16 --repeat until-fail:20` in Debug and Release. Require zero failures.
- [ ] **Step 4: Run docs-only and package matrices.** Configure docs ON/tests OFF and pass both docs tests. Run installed C/C++ consumers in default, relocated, custom include/lib, and spaces prefixes for Debug and Release. Verify exact 0.3.0 succeeds and 0.2.0/0.3.1 fail.
- [ ] **Step 5: Run Linux GCC and Clang.** Use fresh native-filesystem Debug and Release builds with `-Wall -Wextra -Wpedantic -Werror=implicit-function-declaration`. Run all offline tests and install consumers.
- [ ] **Step 6: Run sanitizers separately.** ASan/UBSan runs all applicable offline tests. TSan runs `jpl_threads` and `jpl_mmap`. Record exact compiler versions and exclusions.
- [ ] **Step 7: Run Windows clang-cl and macOS Clang hosted jobs.** Require Debug and Release build, tests, and install consumer. Retain workflow URLs and commit SHA.
- [ ] **Step 8: Run opt-in hosted DE440/DE440t tier.** Require both testpo suites, real ephemeris, complete all-ephemerides scan, fixture integrity, and cache reuse. Record downloaded sizes and manifest hashes.
- [ ] **Step 9: Run maintainer-local tier.** Require the dedicated runner preflight, `jpl_all_ephemerides_local`, at least one non-440 local testpo registration, and all available legacy/long-span/time-segmented suites. Record exact file set, skipped absent files, and full-scan duration.
- [ ] **Step 10: Run final policy and provenance checks.** Pass licenses, workflow policy, numerical-reference integrity, fixture integrity positive/negative, docs symbols/paths, examples, fetch rollback, and install tests.
- [ ] **Step 11: Close each ledger row.** For F01-F18 record owning commit, regression name, configurations, and evidence. A missing mandatory hosted or real-data gate leaves the row `Blocked`, not `Resolved`; do not merge while any row is open or blocked under the user's all-findings requirement.
- [ ] **Step 12: Commit acceptance evidence.** Commit:
```text
docs(review): record current remediation acceptance

Record fresh platform, sanitizer, parallel-stress, package, docs,
provenance, DE440/DE440t, and maintainer-local evidence for F01-F18.
All known review findings are resolved with tracked regressions.
```

### Task 18: Review and merge handoff

**Files:**
- No planned source changes.

**Interfaces:**
- Consumes: clean Task 17 branch and closed ledger.
- Produces: maintainer-reviewed branch ready for an explicit merge commit.

- [ ] **Step 1: Invoke `requesting-code-review`.** Dispatch a fresh reviewer over `main...bugfix/current-review-remediation`, the approved spec, this plan, and F01-F18 ledger. Require findings first with file/line references.
- [ ] **Step 2: Resolve every review finding.** Each code correction gets a regression, focused commit, and rerun of affected Task 17 gates. Documentation-only corrections get their own docs commit. Repeat review until no findings remain.
- [ ] **Step 3: Run the complete final gate again after the last correction.** Do not rely on pre-review results. At minimum rerun clean Windows Debug/Release, parallel stress, docs, install, policies, and any platform/real-data job affected by later changes.
- [ ] **Step 4: Present the branch for maintainer approval.** Report commit list, F01-F18 closure table, test commands and results, workflow URLs, real-data set, and residual environmental limits. Do not merge yet.
- [ ] **Step 5: After explicit merge authorization, invoke `finishing-a-development-branch`.** Update local `main` safely, verify the branch point, and merge with `git merge --no-ff bugfix/current-review-remediation`. Do not squash, rebase, or fast-forward.
- [ ] **Step 6: Verify the merge commit.** Build and run the required smoke/full suite on merged `main`, show `git log --oneline --graph -20`, and confirm the worktree is clean. Push or tag only under separate explicit authorization.

---

## Finding-to-Task Map

| ID | Finding | Owning task(s) | Required regression/evidence |
| --- | --- | --- | --- |
| F01 | Parallel validator corpus race | T01, T17 | Fixture setup plus repeated `-j 16` |
| F02 | Final endpoint selects trailing record | T02 | Distinct trailing coefficient, FILE/mmap |
| F03 | Constant/date equality rejects valid file | T03 | Equal-value valid mock, FILE/mmap |
| F04 | Invalid body precedence | T06 | Invalid codes combined with range/missing data |
| F05 | Aberration second norm overflow | T07 | Finite tiny-Sun ERFA/scale-safe reference |
| F06 | File-controlled large allocations | T04 | Wide sparse record with bounded-read proof |
| F07 | Cache loss on publish failure | T10 | Forced failure restores prior destination |
| F08 | Local workflow can pass without local data | T11, T17 | Variable/label/registration policy and dispatch |
| F09 | Incompatible exact 0.2.0 identity | T14, T17 | Exact 0.3.0 consumer and mismatch tests |
| F10 | Weak testpo diversity | T08, T17 | 120 duplicate rows fail; official files pass |
| F11 | Sweep misses interior records | T09, T17 | Interior corruption fails; complete real scan |
| F12 | Finite date overflow misclassified | T06 | All public date APIs return NUMERIC |
| F13 | Fixed endpoint tolerance | T05 | Valid distant rounded endpoint and >8-ULP negative |
| F14 | `malloc(0)` backend dependence | T05 | Big-endian zero-constant FILE/mmap success |
| F15 | Docs checks disappear with tests OFF | T12, T17 | Docs-only configure matrix |
| F16 | Stale numerical provenance hashes | T13, T17 | Positive and mutated-copy integrity checks |
| F17 | Mutable actions and implicit permissions | T15, T17 | Offline workflow policy plus hosted jobs |
| F18 | Live documentation drift | T06, T16, T17 | Doxygen, local links, README/license audit |

## Commit Order

1. `docs(plan): record current review remediation`
2. `docs(review): record current remediation baseline`
3. `fix(tests): generate validator corpus once per run`
4. `fix(core): keep trailing records out of date selection`
5. `fix(parser): validate layouts without payload-value heuristics`
6. `fix(parser): bound header reads for wide records`
7. `fix(parser): scale endpoint checks and zero-length reads`
8. `fix(api): validate bodies before dates and data access`
9. `fix(numeric): normalize aberration output without squaring overflow`
10. `fix(validators): require reference component coverage`
11. `fix(validators): scan every ephemeris data record`
12. `fix(fetch): restore stale cache when publication fails`
13. `fix(ci): require evidence from the local ephemeris sweep`
14. `fix(cmake): register docs checks independently of unit tests`
15. `test(provenance): enforce numerical reference hashes`
16. `chore(release): assign version 0.3.0 to breaking API`
17. `fix(ci): pin actions and restrict workflow permissions`
18. `docs(project): align notices and validation guidance`
19. `docs(review): record current remediation acceptance`

Commits discovered during acceptance are inserted immediately before the acceptance-evidence commit. Each gets a regression and must not be folded into an unrelated existing commit.
