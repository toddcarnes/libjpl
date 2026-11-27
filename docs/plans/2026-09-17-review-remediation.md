# Review Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve every finding and coverage gap in the review of `1327b20`, including the required two-part Julian-date migration, without losing standard DE compatibility.

**Architecture:** Keep the three-header C17 library and its existing module responsibilities. Introduce small private date helpers and shared test-only reference parsing; consolidate binary validation instead of maintaining three divergent open paths. Sequence API changes atomically, then verify independent numerical, platform, and installed-package behavior.

**Tech Stack:** C17, CMake 3.20+, CTest, MSVC/GCC/Clang/clang-cl, vendored ERFA 2.0.1, Doxygen, native Windows/POSIX test threads. No new runtime or ordinary-build dependencies.

**Spec:** [Approved remediation design](../specs/2026-09-17-review-remediation-design.md). Also read the saved project review in `docs/reviews/`.

## Global Constraints

- All computational Julian epochs use actual arrays of two doubles. Both parts survive arithmetic and ERFA calls.
- Necessary API and ABI breaking changes are permitted. Replace scalar-date interfaces directly; do not add scalar compatibility wrappers.
- All public AU-valued results and conversion helpers use the exact modern definition, `1 AU = 149597870.7 km`. Original header constants and raw records remain unchanged.
- License first-party code under MIT, with Todd Carnes as copyright holder. Preserve third-party licensing and do not edit vendored ERFA sources.
- Keep C17, `C_EXTENSIONS OFF`, three installed public headers, static `jpl`, private ERFA, and offline default tests.
- Retain native-frame legacy access. ICRS convenience functions reject legacy or unverified frames.
- Computed outputs are unchanged on failure; raw-read buffers may be partially written after I/O errors. Support the exact aliasing documented in the spec.
- Use normal IEEE binary64 semantics, not fast-math; do not use MSVC `long double` as a higher-precision oracle.
- No version bump, public release, compatibility shim, or new astronomy capability is authorized by this plan.
- Follow AGENTS.md. Preserve unrelated changes. Stop and ask if a required toolchain or workflow prerequisite is missing.
- Commands shown for local agents are PowerShell 5.1 compatible. Use `workdir`; never chain with `&&`. Verify a parent directory before creating build output.
- Do not commit, stage unrelated changes, push, or create branches/worktrees without the applicable authorization. Each task ends with a review checkpoint; it becomes a small commit only if commits are explicitly requested.

---

## Reading and execution order

This master document is authoritative for dependencies and coverage. The companion plans contain executable task details, not optional appendices:

1. [Core tasks](2026-09-17-review-remediation-core.md): T01, T02, T04-T13.
2. [Validation tasks](2026-09-17-review-remediation-validation.md): T03, T14-T17, T22.
3. [Distribution tasks](2026-09-17-review-remediation-distribution.md): T18-T21.
4. T00 and T23 are defined below in this master document.

Do not execute each companion file from top to bottom in isolation. Recommended serial order:

```text
T00 -> T01 -> T02 -> T03 -> T04 -> T05 -> T06 -> T07 -> T15
    -> T08 -> T09 -> T10 -> T11 -> T12 -> T13 -> T14
    -> T16 -> T17 -> T18 -> T19 -> T20 -> T21 -> T22 -> T23
```

T03 establishes independent row-parser tests early; T15 integrates them into live validators immediately after the API migration and before units/numerics change. T01 may precede baseline Linux testing because it repairs a known strict-C17 failure. T07 is intentionally one coordinated migration across public declarations and all callers; do not mark it complete with only some modules building.

| Task | Deliverable | Required predecessors |
| --- | --- | --- |
| T00 | Recorded baseline and execution ledger | Approved spec |
| T01 | Declared, bounded large-file I/O on strict platforms | T00 |
| T02 | Tested private compensated date arithmetic | T00 |
| T03 | Checked reference row parsing and tolerance policy | T02 |
| T04 | Safe header/query arithmetic and valid mocks | T01, T02 |
| T05 | Unique complete layout/byte-order validation | T04 |
| T06 | Geometry-bounded optional TT-TDB metadata | T05 |
| T07 | Buildable two-part public API and ERFA migration | T02, T03, T06 |
| T08 | Modern AU and exact-derived light speed | T07, T15 |
| T09 | Transactional finite state/TT-TDB results | T07, T08 |
| T10 | Safeguarded and independently checked light-time | T09 |
| T11 | Stable, status-returning aberration/frame/unit math | T08, T09 |
| T12 | Frame metadata and correct convenience shortcuts | T10, T11 |
| T13 | Identity-safe FILE clones | T05, T07 |
| T14 | Concurrent readers and surviving clone lifetimes | T12, T13 |
| T15 | Reference validators that fail for bad data | T03, T07 |
| T16 | Independent numerical/end-to-end truth and error matrix | T10, T11, T12, T15 |
| T17 | Fixture digests, row identities, reproducible provenance | T15 |
| T18 | MIT notices, ownership inventory, license tests | T00; final scan after all new files |
| T19 | Relocatable installed C/C++ package and notices | T12, T18 |
| T20 | Verified, non-destructive download-cache reuse | T15, T17 |
| T21 | Complete generated docs, migration, live guidance | T12, T16, T18, T19, T20 |
| T22 | Cross-platform CI and reproducible acceptance commands | T14, T16, T17, T19, T20, T21 |
| T23 | Issue-by-issue closure with fresh evidence | All other tasks |

Independent distribution work can overlap core work only with separate file ownership and explicit integration reviews. `CMakeLists.txt`, public headers, `src/jpl.c`, and shared test helpers are coordination points; do not run concurrent editors on them. Shared test checks remain active in Release.

## File map

| Files | Responsibility |
| --- | --- |
| `include/jpl.h`, `include/jpl_frames.h`, `include/jpl_convenience.h` | Three public contracts, status types, frame metadata, two-part dates |
| `src/jpl.c` | Storage, shared binary validation, interpolation, body states, light-time, existing aberration implementation |
| `src/jpl_private.h`, new `src/jpl_date.c` | Private physical constants and shared compensated date primitives |
| `src/jpl_frames.c`, `src/jpl_convenience.c` | Pure date/frame transformations and handle-based convenience composition |
| `tests/jpl_test_util.h` | Byte writers, valid configurable mock writer, active test checks |
| New `tests/jpl_testpo_util.h`, `tests/jpl_testpo_util.c` | Test-only checked row parser, component mapping, tolerance policy |
| New `tests/test_dates.c`, `tests/test_parser.c`, `tests/test_largefile.c`, `tests/test_api_errors.c`, `tests/test_numeric.c`, `tests/test_threads.c`, `tests/test_testpo_parser.c` | Focused regression executables |
| New `tests/make_validator_data.c`, `tests/cmake/test_validators.cmake` | Controlled input mutations and runner-exit tests |
| Existing `tests/test_*.c` | Preserve and extend geometric, light-time, aberration, frame, convenience, mmap, TT-TDB, fixture and real-data tests |
| New `tests/data/numerical_reference.h`, `tests/data/numerical_reference.md`, `tests/data/fixture_rows.txt`, `tests/data/PROVENANCE.md` | Pinned small references, generation provenance, ordered fixture row identities |
| New `tools/generate_numerical_reference.c` | Opt-in independent reference generator; never calls libjpl's production APIs |
| New `tests/cmake/check_fixture.cmake`, `tests/cmake/test_fixture_integrity.cmake` | Binary/text digests, deliberate corruption tests |
| New `LICENSE`, `LICENSES.md`, `tests/license_inventory.cmake`, `tests/cmake/check_licenses.cmake` | Full MIT grant, third-party attribution/exemptions, automated ownership checks |
| New `cmake/libjplConfig.cmake.in`, `tests/consumer/CMakeLists.txt`, `tests/consumer/main.c`, `tests/consumer/main.cpp`, `tests/cmake/test_install.cmake` | Installed package and consumer verification |
| New `cmake/LibjplTestData.cmake`, `tests/cmake/test_fetch_cache.cmake` | Shared manifest resolver and small offline cache tests |
| `Doxyfile.in`, new `tests/cmake/check_docs.cmake`, `tests/cmake/test_docs_paths.cmake`, `tests/public_symbols.txt` | Complete docs and path-with-spaces/symbol checks |
| `README.md`, `AGENTS.md`, `CHANGELOG.md`, new `docs/migration-two-part-dates.md`, `docs/validation.md`, `tests/examples.c` | Live instructions, examples, scientific contracts, migration and validation procedures |
| New `.github/workflows/ci.yml`, `.github/workflows/ephemerides.yml` | Offline platform gates and explicit opt-in real-data execution |
| New `docs/reviews/2026-09-17-remediation-progress.md` | Append-only task, command, blocker, and verification evidence |

Do not create empty scaffolding files in advance. Create each file in its owning task with a runnable test or concrete deliverable. Avoid a generic test framework or splitting the core into unrelated architectural layers.

## Shared interfaces

Names in this section are the cross-task contract. Any change must be updated in every consuming task before handoff.

### Private date functions, owned by T02

Declare in `src/jpl_private.h`, define in `src/jpl_date.c`; none are installed:

```c
jpl_status jpl_date_normalize(const double in[2], double out[2]);
jpl_status jpl_date_add(const double a[2], const double b[2], double out[2]);
jpl_status jpl_date_subtract(const double a[2], const double b[2], double out[2]);
jpl_status jpl_date_add_days(const double date[2], double days, double out[2]);
jpl_status jpl_date_add_scaled(const double date[2], double step,
                               size_t index, double out[2]);
jpl_status jpl_date_partition(const double begin[2], const double end[2],
                              size_t count, size_t index, double out[2]);
int jpl_date_compare(const double a[2], const double b[2]);
```

Status-returning functions validate finiteness, preserve output on failure, and support exact aliasing. `jpl_date_compare` consumes normalized finite pairs only and returns -1/0/1; public callers normalize before using it. `partition` accepts `0 < count <= 2^53`, `index <= count`; endpoints copy exactly. `add_scaled` checks that index is exactly representable as a double before multiplying. Helpers report ARGUMENT for bad inputs and NUMERIC for unrepresentable finite arithmetic. T02 introduces the three new statuses from spec section 2.2 and extends `jpl_status_string` so later tasks can use them. `src/jpl_private.h` includes `jpl.h` to make these declarations self-contained.

T10 also adds the following private declaration under `LIBJPL_TESTING`, defined only in tests-enabled builds and never declared in an installed header:

```c
jpl_status jpl_test_lighttime_with_budget(const jpl_ephemeris *ephemeris,
    const double jd[2], int target, int center, double state[6], double *delay,
    int fast_iterations, int safeguarded_iterations);
```

The normal public solver calls the same internal implementation with 10/128 iterations. The test hook is absent from tests-OFF builds; its only purpose is deterministic exhaustion coverage.

### Mock writer, owned by T04

Extend the existing test utility rather than introducing a second mock framework:

```c
typedef struct jpl_test_mock {
    size_t record_doubles, advertised_records;
    int trailing_record, big_endian, denum;
    uint32_t constant_count;
    double start[2], end[2], step_days, au_km, emrat;
    unsigned char *header;
    double *constants;
    double *records;
} jpl_test_mock;
int test_write_mock(const char *path, const jpl_test_mock *mock);
```

The caller owns buffers. `header` is exactly `record_doubles * 8` bytes and carries raw pointer/name mutations; the writer fills standard prologue fields and DENUM in the requested byte order. Header start, end and step are independent inputs so a NaN-step regression keeps otherwise-valid endpoints. `constants` is a full record. `records` contains all advertised records plus the optional trailing record. The caller fills data dates and coefficient values explicitly, permitting malformed-date tests. The writer checks file operations and returns 0 on success, 1 on failure. At the serialization boundary only, combine the pair into the required encoded scalar and document that the wire format cannot preserve an arbitrary extra residual.

### Testpo parser, owned by T03

```c
typedef struct jpl_testpo_row {
    int denum, target, center, component;
    double jd[2], expected;
} jpl_testpo_row;
int jpl_testpo_parse_row(const char *line, jpl_testpo_row *out);
int jpl_testpo_state_index(const jpl_testpo_row *row);
double jpl_testpo_tolerance(const jpl_testpo_row *row);
```

Parse returns 1 for a valid data row, 0 for a blank row, -1 for invalid data; it does not silently identify arbitrary text as a header. Output is unchanged on -1. Full-file header/EOT state belongs to the runner; fixture mode is data-only. `state_index` maps valid body/libration components to 0-5 and nutation components 1-4 to 0,1,3,4; returns -1 otherwise. T#17 is dispatched separately. Tolerance is in reference units; it does not include modern-AU conversion. T15 integrates this parser; T08 changes body comparison units explicitly.

### Public migration, owned by T07/T11/T12

- T07 changes every `double jd` parameter to `const double jd[2]`, both coverage fields to arrays, TT/TDB returns to `jpl_status` plus output arrays, and date/frame functions to status returns. Function names and state buffer order stay unchanged.
- T08 changes `jpl_au_per_day_to_km_per_s` and `jpl_km_to_au` to return `jpl_status` and makes all public AU results modern-AU.
- T11 changes both aberration functions to return `jpl_status`, retaining argument order and state/observer/output shapes.
- T12 introduces `jpl_reference_frame` values `JPL_FRAME_UNKNOWN`, `JPL_FRAME_DYNAMICAL_1950`, `JPL_FRAME_DYNAMICAL_2000`, `JPL_FRAME_ICRF`, and `jpl_header.reference_frame`.

## Common verification and review cycle

For each task, first add the named regression or negative test and observe its failure against the pre-fix code. Missing new symbols may cause an initial compile failure; after scaffolding compiles, demonstrate the relevant behavioral assertion fails too. For a pure coverage addition that already passes, deliberately perturb the expected reference or run a disposable mutated implementation to prove the test detects the defect; restore only your own mutation.

Whenever CMake target definitions change, reconfigure before attempting to build a newly named target; do not rely on an old generated solution to discover it. Every task's last step includes these separate commands, plus its focused commands:

```powershell
cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/remediation --config Debug --parallel
ctest --test-dir build/remediation -C Debug --output-on-failure
cmake --build build/remediation --config Release --parallel
ctest --test-dir build/remediation -C Release --output-on-failure
git diff --check
git status --short
```

For single-config builds use separate Debug/Release directories with explicit `-DCMAKE_BUILD_TYPE`. Never run tests from an old executable after a failed build. Record warnings, skipped configurations, commands, and results in the progress document. Review only intended changes and check for accidental edits under `src/erfa`. If authorized to commit, inspect status/diff/recent log first and make a noninteractive focused commit; do not amend or bypass hooks. Suggested commit subjects in companion plans are labels, not authorization.

## Task T00: Baseline and execution ledger

**Files:** Create `docs/reviews/2026-09-17-remediation-progress.md`; read the spec, review, all companion plans, and current `AGENTS.md`.

**Interfaces:** Consumes the approved spec and baseline revision; produces a durable progress record keyed by task and review ID. No code interfaces change.

- [ ] Inspect `git status --short --branch` and `git log --oneline -10`; record the starting revision and existing user changes without staging or reverting them.
- [ ] Verify the build parent with `Test-Path -LiteralPath "build"`. If it is absent, verify the existing workspace root with `Test-Path -LiteralPath "."` before allowing CMake to create the new nested build path. Never replace another build directory or reuse its configuration silently.
- [ ] Configure `cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF`, then run the common Debug/Release commands. Record the actual test count, not the stale AGENTS count. Existing strict POSIX failure belongs to T01 and is not a reason to weaken flags.
- [ ] Create the ledger with this initial record format and add one row per task at execution time:

```markdown
# Remediation progress

Spec: ../superpowers/specs/2026-09-17-review-remediation-design.md
Baseline: 1327b20; inspect and record the actual execution-start revision.

| Task | Review IDs | State | Revision | Commands/evidence | Remaining blockers |
| --- | --- | --- | --- | --- | --- |
| T00 | Baseline only | In progress | Uncommitted | Baseline commands recorded below | None established yet |
```

- [ ] Inventory available MSVC, clang-cl, Linux GCC/Clang, macOS CI, 32-bit, Doxygen, and sanitizer configurations. Record missing environments as pending gates. Do not claim they passed based on the prior review.
- [ ] Check the ledger and diff; mark T00 complete only after baseline evidence is recorded. Suggested checkpoint label: `docs(review): record remediation baseline`.

## Issue-to-task map

This is a complete expansion of spec section 11. Every row must remain mapped during implementation. Owning tasks provide tests; T23 records actual closure, not planned closure.

| ID | Owning tasks | Required evidence |
| --- | --- | --- |
| R00 | T02, T03, T07, T16, T21 | Two-part API/arithmetic/text parsing; distant-epoch low-part and ERFA tests |
| R01a | T04, T07, T09 | NaN/Inf in either part, no undefined casts, shortcuts validated |
| R01b | T04, T05, T09 | Header/data/subinterval span mutations |
| R01c | T01, T04, T05 | File-bounded count/index and overflow cases |
| R02 | T12, T16 | Centers 0/12 equivalent on mock and real DE441 |
| R03a | T01, T22 | Strict C17 declared file-offset APIs |
| R03b | T01, T22 | 32-bit large-offset and addressability gates |
| R04 | T12, T16, T21 | Cited frame classification; native legacy success/convenience rejection |
| R05a | T18, T21, T22 | Full MIT grant, Todd Carnes headers and inventory |
| R05b | T18, T19 | Installed full project/ERFA notices, vendor unchanged |
| R06a | T05 | Header-only/overlapping layout rejection |
| R06b | T05 | Correct trailing width, constants and endpoint behavior |
| R07 | T13, T14 | Cwd/replacement identity and independent reads |
| R08a | T04 | AU/EMRAT zero, negative, nonfinite rejection |
| R08b | T09, T11, T16 | FORMAT vs NUMERIC, no successful nonfinite results |
| R08c | T10 | Explicit convergence and NaN-center rejection |
| R09 | T05 | Ambiguous preliminary BE header succeeds after full validation |
| R10 | T06 | Legacy padding absent, valid appended TT-TDB retained |
| R11 | T10, T16 | Saved boundary case plus genuine pre-coverage failure |
| R12a | T19 | find_package and exact-version tests |
| R12b | T19 | Custom include/lib paths and relocated prefix |
| R12c | T19, T22 | Toolchain-resolved math dependency, clang-cl consumer |
| R13a | T15 | In-coverage ordinary-body failure causes nonzero exit |
| R13b | T15 | Only metadata-proven absent angular datasets are skipped |
| R13c | T03, T15 | 100-NaN T#17 reference file fails |
| R13d | T03, T15, T22 | One psi epoch factor; perturbed far-epoch reference fails |
| R13e | T15 | Missing versus unreadable/truncated present files |
| R14a | T21 | Three public API reference pages |
| R14b | T21, T22 | Public XML symbol inventory, not only clean Doxygen exit |
| R15 | T12 | Identical-body query without unrelated Sun coefficients |
| L01 | T08, T10, T11 | Exact-derived c and independent km/SI expectations |
| L02 | T08, T15, T16 | Nonmodern header AU, reference conversion, unchanged raw data |
| L03 | T07, T09 | SSB coverage endpoints/outside/nonfinite cases |
| L04 | T07, T16, T21 | Correct amplitude/model/epoch accuracy and time-scale docs |
| L05 | T07, T12, T21 | Target 0 invalid, centers 0/12 accepted |
| L06 | T20 | Valid offline cache reuse, invalid-cache safe failure |
| G01 | T16 | Exact TT-TDB midpoint value/rate |
| G02 | T06, T16 | Distinct TT-TDB subinterval and appended-layout values |
| G03 | T14, T22 | Actual concurrent mmap and FILE-clone reads |
| G04 | T13, T14 | Original/sibling close permutations, surviving handles |
| G05 | T10, T16 | Analytic light-time and residual |
| G06 | T02, T07, T16 | Absolute TT/TDB and low-part precision |
| G07 | T17 | Digests and independently recoverable provenance |
| G08 | T17, T21 | Interior row identity mutation rejection; honest provenance wording |
| G09 | T22 | Durable Windows/Linux/macOS/32-bit/sanitizer workflows |
| G10 | T19, T22 | Installed C/C++ tests and relocation/version/path variants |
| G11 | T16 | Pinned independent matching-convention apparent/frame truth |
| G12 | T02, T07, T16 | Higher-order derivatives and neighboring interval dates |
| G13 | T04, T09, T11, T13, T16 | Input/output/error/cleanup matrix |
| B01 | T10, T16, T21 | Mixed-epoch velocity formula tested/documented |
| B02 | T11, T16, T21 | R*v only, fixed epoch |
| B03 | T12, T16, T21 | Exact correction and body-center observer scope |
| B04 | T16, T21 | Accuracy interval distinct from file coverage |
| B05 | T02, T07, T21 | No scalar full-epoch computation except wire boundaries |
| B06 | T13, T14, T21 | Apparent-leg warning, immutability and lifetime rules |
| S01 | T21 | Small fixture versus operational data wording |
| S02 | T21, T22 | Actual tier names/gates and no stale counts |
| S03 | T21 | Source/build/docs paths with spaces |
| S04 | T11, T16 | Stable extreme-magnitude vectors and invalid velocity |
| S05 | T03, T15 | Nutation rates checked at indices 3/4 |
| S06 | T03, T15 | Six-token DENUM field regression |
| S07 | T03, T15 | Row grammar, overflow, labels, line length and read errors |

## Task T23: Fresh acceptance and issue closure

**Files:** Modify `docs/reviews/2026-09-17-remediation-progress.md`, the resolution log in `docs/reviews/2026-09-17-project-review.md`, and `docs/validation.md`. Inspect every changed implementation/test/distribution file.

**Interfaces:** Consumes T01-T22 artifacts and their evidence; produces the final per-issue disposition. No speculative pass claims or omitted limitations.

- [ ] Run all default Debug/Release commands from clean build directories and every mandatory configuration in T22. Re-run installed-package, docs-symbol, license, fixture-integrity, and threaded tests against those builds.
- [ ] Run the opt-in real-data tier with `cmake -S . -B build/remediation-real -DLIBJPL_FETCH_EPHEMERIDES=ON`, build Debug/Release, and run CTest with `--output-on-failure`. Configure only after verifying the parent and confirming local/download sources; record the actual DE versions, spans, hashes and skipped data. The option is explicit authorization within an implementation session, not permission to alter maintainer-local files.
- [ ] Inspect each review ID in the map. For each, append the actual fix revision or `uncommitted`, tracked test names, configurations, command results, and remaining limits. A missing mandatory gate leaves the relevant item blocked, not resolved.
- [ ] Check actual public declarations and use content searches for scalar date patterns, old speed-of-light literals, false 2 ms/70 s claims in live docs, stale suite names, `assert(` in test execution paths, and `src/erfa` changes. Historical review/spec quotations may remain clearly labeled as historical.
- [ ] Audit package contents outside the source checkout, preserving ERFA notices, exact public include paths, and no private headers. Re-run examples in both C and C++ consumers.
- [ ] Review the whole diff for unintended behavior changes, untracked generated references/provenance, dependency additions, exposed private helpers, weakening tolerances, and dropped tests. Run `git diff --check` and `git status --short`.
- [ ] Report completed IDs and blocked IDs separately. Do not publish, bump versions, or commit without explicit authorization. Suggested checkpoint label: `docs(review): record verified remediation outcomes`.

## Plan status

All task checkboxes start unchecked. This document and its companions are a plan, not evidence that the library is fixed. Implementation begins only after the maintainer chooses execution mode and any required isolation/commit permissions are settled.
