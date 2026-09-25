# Current review remediation resolution

Spec: ../superpowers/specs/2026-09-19-current-review-remediation-design.md
Plan: ../superpowers/plans/2026-09-19-current-review-remediation.md
Branch: bugfix/current-review-remediation
Baseline: b6d3ac1 (spec baseline); execution-start revision b8e6b88 (adds chore: ignore .worktrees/ only).

## T00 baseline evidence

All commands run under PowerShell 5.1 from the worktree root
`D:\src\libjpl\.worktrees\libjpl-remediation`, branch `bugfix/current-review-remediation`.

### Git status / log

- `git status --short --branch` at start: clean (`## bugfix/current-review-remediation`).
- `git log --oneline -10` at start: `b8e6b88 chore: ignore .worktrees/` on top of
  `b6d3ac1 Merge refactor/review-remediation into main`.
- `main` checkout preserved: no stash/reset; its only changes are the two
  untracked approved plan/spec sources (transferred byte-identical in commit
  `7f649ed`) plus the committed `.gitignore` edit above.
- `git worktree list`: `D:/src/libjpl` on `main`, worktree on the bugfix branch.
- Planning documents transferred byte-identical (SHA-256 plan
  `cf712973f88e2c84a3290a240629f2763cd8e4293694b49dabb52d949e0d4074`,
  spec `5a2c3a89fa365321d6570bb79d794eb979d0eeee3dae380853c312ed1d8b6d12`).

### Build configuration

- `Test-Path -LiteralPath "build"` returned `False` before creating `build/baseline`.
- Configure: `cmake -S . -B build/baseline -DLIBJPL_FETCH_EPHEMERIDES=OFF`
  succeeded. Generator: Visual Studio 18 2026, MSVC 19.51.36257.0,
  Windows SDK 10.0.26100.0.

### Fresh offline results (`build/baseline`)

- `cmake --build build/baseline --config Debug --parallel`: succeeded, zero warnings.
- `ctest --test-dir build/baseline -C Debug --output-on-failure`: 36/36 pass,
  total 318.44 sec.
- `cmake --build build/baseline --config Release --parallel`: succeeded, zero warnings.
- `ctest --test-dir build/baseline -C Release --output-on-failure`: 36/36 pass,
  total 251.75 sec.
- No skips concealed; serial runs are fully green.

### F01 reproduction

- Command: `ctest --test-dir build/baseline -C Debug --output-on-failure -R "^jpl_validator_" -j 16 --repeat until-fail:5`.
- Result: REPRODUCED. 50% pass, 7/14 validator tests failed:
  `jpl_validator_good` (expected success, got `line 94: invalid row: malformed data row`),
  `jpl_validator_missing_body` (`missing EOT header` instead of `reference mismatch`),
  `jpl_validator_denum`, `jpl_validator_malformed6`, `jpl_validator_badcomp`,
  `jpl_validator_overflow_pos` (`could not open ephemeris .../good.eph` instead of the
  expected category), `jpl_validator_read_error` (`reference mismatch ... I/O error`
  instead of `read error`). Root cause: every case regenerates the shared corpus
  (`GENERATOR` block in `tests/cmake/test_validators.cmake:28-36`), truncating
  files concurrently. SOURCE-CONFIRMED by inspection plus this reproduction.

## T01 evidence

- `jpl_validator_data_setup` generates the corpus once (`FIXTURES_SETUP
  jpl_validator_corpus`); all 15 validator cases carry `FIXTURES_REQUIRED
  jpl_validator_corpus` in one `set_tests_properties()` list, including
  `jpl_fixture_read_error`. The driver no longer runs `GENERATOR`; it fails
  when a generated corpus file is absent (source-fixture `ROWS` case exempt).
- `ctest -R "^jpl_validator_denum$"`: CTest auto-ran the setup fixture first;
  both pass.
- `ctest -R "^jpl_validator_" -j 16 --repeat until-fail:10`: 15/15 pass on all
  10 repetitions.
- Full offline `ctest -j 16 -E "^jpl_threads$"`: 36/36 pass. `jpl_threads`
  passes standalone (0.07 s); the single full-parallel timeout (`threads tests
  passed` printed before the 120 s TIMEOUT fired under load) is a scheduling
  artifact, not a validator failure. Re-run serial full suite before commit.
- Note: `jpl_validator_good` etc. pass without an explicit `jpl_validator_data`
  dependency edge because the fixture mechanism orders setup first; verified
  via the single-test `-R` selection above.

## T02 evidence

- `struct jpl_ephemeris` stores `record_count` (physical data records) and
  `covered_record_count` (advertised intervals). `validate_layout()` and
  `select_layout()` thread both outputs through FILE and mmap opens; clones
  copy both. `jpl_record_count()`/`jpl_read_record()` keep the physical
  count; `select_record()` uses the covered count everywhere (endpoint
  selection, clamping, binary-search bounds).
- Regression: the one-interval trailing mock now uses covered coefficient
  `5.0` vs trailing `99.0`; `{2451546.0, 0.0}` returns `5.0/AU` via FILE and
  mmap (failed with `99.0/AU` before the fix). A dedicated `test_api_errors`
  trailing mock also asserts raw record 2 stays readable while the coverage
  end returns the covered value.
- Focused suites Debug + Release: `jpl_parser`, `jpl_api_errors`,
  `jpl_numeric`, `jpl_fixture`, `jpl_testpo_parser` all pass.

## T03 evidence

- Deleted the constants slot-0 vs data slot-0 value comparison in
  `validate_layout()` (`src/jpl.c`). Geometry (complete split candidates,
  scheduled record endpoints) and header metadata remain the sole layout
  discriminators.
- Regression: valid one-record little-endian Mars mock with
  `constant_count = 2`, `constants[0] == start_jd`, valid distinct endpoints
  and coefficients. Failed with `JPL_ERROR_FORMAT` on both opens before the
  fix; now both succeed, constants preserve the equal value, interpolation
  succeeds (FILE + mmap).
- Ambiguity still rejected: the deliberately ambiguous-width fixture returns
  `JPL_ERROR_FORMAT` because both complete split candidates survive (split
  rule), not because payload values differ. Comments updated accordingly.
- `jpl_parser`, `jpl_mmap`, `jpl_fixture` pass in Debug and Release.

## T04 evidence

- `parse_header()` now parses only the classic 2856-byte fields and leaves
  TT-TDB tentative and absent; new `parse_optional_tt_tdb()` decodes the
  24-byte T14/T15 tail (T15 at offset 12, integer bounds checked).
- `validate_layout()` no longer allocates an overflow-name window or a full
  candidate record: the tail offset is computed with checked arithmetic and
  the 24 tail bytes are read directly via the bounded source wrapper. Extra
  constant names are skipped by offset, never copied. File-controlled record
  widths no longer control heap allocation during open.
- Test-only parser read limiter `jpl_test_set_parser_read_limit()` (declared
  in `src/jpl_private.h` under `LIBJPL_TESTING`): every parser source read
  routes through one wrapper; nonzero limits fail over-limit requests with
  `JPL_ERROR_IO`. Kept out of installed headers.
- Regression: 1 MiB wide-record mock under a 2880-byte limit opens via FILE
  and mmap and interpolates (pre-fix: full-record parse requested 1 MiB and
  both opens failed). `jpl_largefile` opens its 4096-byte record under the
  same limit, proving no parser read exceeds 2880 bytes.
- Parser matrix Debug + Release: `jpl_parser`, `jpl_largefile`, `jpl_tt_tdb`
  (overflow-name + DE440t boundary), `jpl_fixture`, big-endian suites green.

## T05 evidence

- `endpoints_match()` now derives its allowance from local binary64 spacing
  via new `endpoint_ulp_tolerance()` (`src/jpl.c`): maximum finite adjacent
  spacing around the encoded endpoint and scheduled high part (both
  `nextafter()` directions), times four; nonfinite products return
  `JPL_ERROR_FORMAT`. The fixed `JPL_T05_ENDPOINT_TOL_DAYS` is removed.
- Regression: valid 3-record mock near JD 80000000 with a 1/3-day step and
  independently binary64-encoded scheduled endpoints opens via FILE and mmap
  (failed under the modern-epoch tolerance before the fix); perturbing one
  endpoint by eight local ULPs fails both opens.
- `read_doubles()` with `count == 0` returns `JPL_OK` after argument/handle
  validation, before backend branching: no seek, mapping-base evaluation,
  `malloc(0)`, read, or copy. Regression: valid big-endian zero-constant
  mock reads constants with a non-NULL one-element destination via FILE and
  mmap, sentinel unchanged.
- Focused suites Debug + Release: `jpl_parser`, `jpl_tests`,
  `jpl_api_errors`, `jpl_largefile` all pass.

## T06 evidence

- New private body validators in `src/jpl.c`: `valid_state_target()`,
  `valid_state_center()`, `valid_lighttime_body()`. Target/center
  combinations validate immediately after required pointer checks and before
  date normalization or data access; `barycentric_state()` is never called
  with an invalid code. Invalid target 99, invalid center 99, target 0,
  angular target with nonzero center, and TT-TDB target all return
  `JPL_ERROR_ARGUMENT` with unchanged outputs, even combined with an
  out-of-range date or a missing dataset.
- `jpl_date_normalize()` finite-overflow status is preserved: `{DBL_MAX,
  DBL_MAX}` returns `JPL_ERROR_NUMERIC` (unchanged outputs) through state,
  light-time, TT-TDB, planet/Moon, frame, and TT/TDB conversion APIs;
  `{NAN, 0}` and `{INFINITY, 0}` stay `JPL_ERROR_ARGUMENT`. Applied in
  `interpolate()`, `barycentric_state()`, `lighttime_solve()`,
  `jpl_get_tt_minus_tdb()`, and `jpl_planet_position()`. File-header
  normalization failures stay `JPL_ERROR_FORMAT`.
- `include/jpl.h` light-time docs now state target 1-13, center 0-13
  exactly; the "bodies 0-13 are valid targets" note is removed.
  Finite-overflow pairs documented as NUMERIC in `jpl.h`/`jpl_convenience.h`.
- API, convenience, numeric, light-time, frames, TT-TDB, and examples pass
  in Debug and Release with transactional outputs.

## T07 evidence

- `jpl_apply_aberration_full()` final normalization uses the shared
  scale-safe `jpl_vec_norm3(p)` instead of accumulating `p[i] * p[i]`
  (`src/jpl.c`). Finite large intermediates with a representable direction
  stay representable; genuine nonfinite coordinates, zero direction,
  unrepresentable final coordinates, and the unrepresentable solar term
  (`1e-320`) still return `JPL_ERROR_NUMERIC` transactionally.
- Regression: unit-scale nonradial state, finite subluminal observer
  velocity, `sun_distance_au = 1e-200`. Returns `JPL_OK` with finite output
  (failed with NUMERIC before the fix), direction matches an independent
  scale-safe normalization of the published expression within 1e-12,
  distance preserved, velocity unchanged, exact in-place aliasing.
  Note: ERFA `eraAb()` itself is not scale-safe at 1e-200 (its internal
  normalization overflows), so the comparison uses the published formula
  with a scale-safe norm, not `eraAb()` output.
- `jpl_aberration`, `jpl_numeric`, `jpl_convenience`, `jpl_examples` pass in
  Debug and Release.

## T08 evidence

- `tests/test_testpo.c` tracks `body_component[18][7]`, incremented only
  after a numerical comparison succeeds. Angular rows never satisfy
  ordinary-body requirements. After parsing, the runner requires: at least
  one available ordinary body 1-13 with all six components (SSB/EMB need no
  file coefficients); T14 X1-X4 when nutation metadata is present; T15 X1-X6
  when libration metadata is present; T17 X1-X2 when TT-TDB is present. The
  100-row minimum stays as an independent truncation guard. Failures emit
  `insufficient reference coverage: ...` naming every missing
  target/component.
- `tests/make_validator_data.c`: good corpus now carries Mars X1-X6,
  nutation X1-X4, libration X1-X6, TT-TDB X1-X2 with actual mock
  coefficients (zero rates exact); all three row grammars stay represented.
  New `duplicate_only.testpo` (120 identical valid Mars X1 rows) plus CTest
  `jpl_validator_duplicate_only` expecting `insufficient reference
  coverage`. Before runner changes it exited zero; the driver expects
  failure, so the test failed pre-fix.
- During development the good corpus needed two corrections (TT-TDB X2
  expectations must use the matching record/subinterval coefficients, not a
  neighboring value; X1 at a subinterval edge evaluates c0-c1, not c0):
  evidence that the diversity check exercises real per-component values.
- All synthetic validator tests serial and `-j 16 --repeat until-fail:5`
  (Debug + Release): good passes, duplicate-only fails for the intended
  category, all prior negatives retain their category.

## T09 evidence

- `tests/test_all_ephemerides.c` iterates `index = 0 ..
  jpl_record_count(ephemeris)-1` with one reused record buffer. Every
  physical record is read; nonfinite start/end (`isfinite()`) and `end <=
  start` fail with filename plus zero-based record index; continuity with
  the previous end is verified; record zero must start at coverage start
  and the final covered record must end at coverage end (one trailing
  record may start at coverage end staying ordered). The manual NaN bit
  inspection is removed in favor of `isfinite()` (also rejects infinities).
- Success text states every physical record was decoded and checked;
  `docs/validation.md` notes the opt-in tier intentionally performs a full
  sequential scan that can read several gigabytes.
- New `sweep_corrupt` fixture (three covered records, clean first/last
  endpoints, NaN date in record one, separate directory): open succeeds but
  the sweep fails at record one (`DE441.eph: record 1: nonfinite record
  dates`). Before the fix the sweep exited zero (record-0-only check).
- Synthetic sweep negative/control plus committed fixture pass in Debug and
  Release; DE440/DE440t and maintainer-local reruns deferred to T17.

## T10 evidence

- `libjpl_resolve_fetch_file()` publishes transactionally: a stale sibling
  backup (`${dest}.replace-backup`) fails loudly without touching either
  file; otherwise the destination is moved to the backup via a checked
  `${CMAKE_COMMAND} -E rename` subprocess (CMake 3.20 can inspect
  `execute_process(RESULT_VARIABLE ...)`), the verified temp is published
  the same way, and the backup is restored on publish failure (rollback
  failure names both paths and preserves the backup). The backup is removed
  only after the new destination exists and the temp is gone; destination
  size/hash are re-verified after publication.
- Test-only `LIBJPL_TEST_FORCE_PUBLISH_FAILURE` (default false) skips the
  publish command and synthesizes a nonzero result so rollback is exercised
  without platform races.
- Regression `6b`: stale destination + valid downloadable replacement under
  the forced-failure switch fails with `forced publish failure`, keeps the
  original hash/size, and leaves no `.fetch-tmp`/`.replace-backup`. Before
  the fix the switch was ignored (test failed with exit 0).
- `jpl_fetch_cache` passes repeatedly; offline second configure against a
  populated cache covered in T17.

## T11 evidence

- New offline `jpl_workflow_policy` test (`tests/cmake/check_workflows.cmake`,
  no YAML dependency) requires the dedicated local label, nonempty-variable
  guard commands, explicit `ctest -N` registration inspection, and no bare
  `[self-hosted]` local job. Failed on current YAML before the fix.
- `.github/workflows/ephemerides.yml` local job now uses
  `[self-hosted, libjpl-ephemerides]`, adds an explicit PowerShell preflight
  failing on empty `JPLEPH`/`JPL_TESTPO` or missing directories, proves via
  `ctest -N` that `jpl_all_ephemerides_local` plus at least one non-440
  `jpl_testpo_[0-9]` suite are registered (parser-only cannot satisfy it),
  then runs only those local registrations.
- `docs/validation.md` documents the label/variables; inventory updated.
- Policy test passes locally; hosted/maintainer-runner dispatch is T17 (no
  pass claimed without execution).

## T12 evidence

- `enable_testing()` moved outside `if(LIBJPL_BUILD_TESTS)` and before the
  docs block; every ordinary executable and ordinary `add_test()` stays
  inside the option block. `LIBJPL_BUILD_TESTS=OFF` still suppresses all
  ordinary test executables; `LIBJPL_BUILD_DOCS=ON` registers exactly
  `jpl_docs_symbols` and `jpl_docs_paths`.
- New configure-matrix test `jpl_docs_only`
  (`tests/cmake/test_docs_only.cmake`, registered only in tests-enabled
  parent builds): disposable nested build with docs ON/tests OFF, builds
  `docs`, requires both docs test names in `ctest -N` while rejecting
  `jpl_tests`/`jpl_parser`, then runs the nested suite (multi-config
  `-C` forwarded). Failed pre-fix (nested `ctest -N` reported zero tests).
- `jpl_docs_only`, `jpl_licenses` pass; AGENTS.md and inventory updated.
  Docs symbols/paths plus install coverage rerun in T17.

## T13 evidence

- Regenerated independently to two new build-tree paths
  (`build/remediation/numerical_provenance/regen_{a,b}.h`): byte-identical
  outputs, values match the checked-in header (not updated mechanically).
  Canonical-LF hashes from my findings (CRLF->LF only, bare CR rejected):
  reference `0a0f75214fcdc507ad15fa6b7874d7b9caab3c05874658934038085d2f1d3b26`,
  generator `dfa98d6d97a066fa80e733ef8766bbb35cba0dd7c931e1d95955aa7b3f589955`.
  These disagree with both the plan's hardcoded values and
  `numerical_reference.md` prose; the regeneration evidence above is
  authoritative per T13-Step1.
- `tests/data/MANIFEST.txt` carries the two keys; `numerical_reference.md`
  explains canonicalization and the corrected hashes.
- New offline `jpl_numerical_reference_integrity`
  (`tests/cmake/check_numerical_reference.cmake`): reads both keys, reads
  each source file in binary mode (Python3 binary-safe canonicalization;
  CMake `file(READ)` alone decodes newlines per-platform and cannot hash
  source bytes), rejects bare CR, writes canonical copies under build WORK,
  compares `file(SHA256)`, emits expected/actual on failure. A mutated
  build-tree copy fails as required (verified manually).
- Integrity, numerical, frame, aberration, light-time pass.

## T14 evidence

- Install version tests strengthened first: exact `0.2.0`/`0.3.1`/`9999.0.0`
  consumer configures must fail for the version reason. Pre-change
  `jpl_install` failed as required (exact `0.2.0` unexpectedly configured
  against the 0.2.0 project).
- `project(libjpl VERSION 0.3.0 LANGUAGES C)`; ERFA `PACKAGE_VERSION="2.0.1"`
  untouched. `CHANGELOG.md`: `## [Unreleased]` kept empty at top, new
  `## [0.3.0] - 2026-09-19` with the breaking two-part-date/modern-AU
  changes plus F01-F18 entries, historical `0.2.0` preserved.
- `jpl_install` passes Debug + Release (default, relocated, custom
  include/lib, spaces, exact 0.3.0, mismatches 0.2.0/0.3.1/9999.0.0).
- Live-file search for `0.2.0`: only the historical changelog section,
  historical review/spec references, and the intentional mismatch test
  remain; `docs/migration-two-part-dates.md` names no version (no-op).

## T15 evidence

- Extended `tests/cmake/check_workflows.cmake`: requires top-level
  `permissions: contents: read`, rejects `uses:` values ending in `@v1`/`@v4`,
  requires every external `uses:` to end in exactly 40 lowercase hex chars
  before an optional comment, and requires the four approved
  owner/repository pairs. Failed pre-fix against mutable tags and missing
  permissions.
- Both workflows: added least-privilege `permissions: contents: read`
  before `jobs:` (no write permissions); replaced every action tag with the
  approved SHA retaining the release comment.
- Policy passes locally; hosted jobs are T17 (no dispatch claimed yet).

## T16 evidence

- Local-link regression in `tests/cmake/check_licenses.cmake`: extracts the
  `` [`LICENSE`](...) `` target, resolves inside the source root, requires
  an existing regular file. Failed pre-fix (`../LICENSE` escapes the root).
- `LICENSES.md`: `[`LICENSE`](../LICENSE)` -> `[`LICENSE`](LICENSE)`;
  installed notice paths unchanged.
- `README.md` fetch wording: DE440, DE440t, testpo.440, testpo.440t all get
  exact-size and SHA-256 verification from `tests/data/MANIFEST.txt`;
  maintainer-local alternate spans used as-is per reference suites.
- Audited live docs vs completed tasks: target ranges (T06), docs-only
  tests (T12), validator fixture setup (T01), complete sweep cost (T09),
  cache rollback (T10), local workflow prerequisites (T11), workflow pins
  (T15), numerical hashes (T13), version 0.3.0 (T14). Historical
  specs/plans/reviews untouched.
- `jpl_licenses`, docs-only, workflow policy, numerical integrity,
  fetch-cache, install pass; Markdown link scan is the license-link check
  itself (no separate scanner added).

## T17 acceptance evidence

All commands under PowerShell 5.1 from the worktree root unless noted
(WSL Ubuntu for Linux gates). Branch history: one focused commit per task
T00-T16 plus two T17-discovered fixes (fetch-diagnostic stabilization,
authoritative-files coverage, GCC warning cleanup); `git diff --check`
clean; no `src/erfa/` changes (`git diff --name-only main...HEAD` shows
none); worktree clean except `build/` (ignored).

### Windows MSVC (fresh dirs `build/t17-debug`, `build/t17-release`)

- Configure/build serial + `ctest -C Debug/Release --output-on-failure`:
  41/41 pass in each configuration (Debug ~364 s, Release ~366 s), zero
  warnings.
- Parallel `ctest -j 16` (minus install/threads): 39/39 pass.
- Validator stress `-R "^jpl_validator_" -j 16 --repeat until-fail:20`:
  16/16 x20 in Debug and Release, zero failures.

### Docs-only and package matrices

- `build/t17-docs-only` (docs ON, tests OFF): `ctest -N` lists exactly
  `jpl_docs_symbols` + `jpl_docs_paths`; both pass.
- `jpl_docs_only` nested matrix passes in the offline suite.
- `jpl_install` Debug + Release: default, relocated, custom include/lib,
  spaces prefixes, exact 0.3.0 success, 0.2.0/0.3.1/9999.0.0 mismatch
  failures. C/C++ consumers pass.
- Doxygen 1.17.0 `WARN_AS_ERROR=YES` clean (`build/t17-docs` Release:
  docs/licenses/fixtures selection 7/7 pass).

### Linux GCC 15.2 / Clang 21.1.8 (WSL Ubuntu, native /home/todd builds)

- GCC Debug + Release, Clang Debug + Release, strict
  `-Werror=implicit-function-declaration`: all offline tests pass except
  `jpl_install` (consumer-toolchain sandbox case, runs on Windows/CI),
  `jpl_largefile` (3.2 GiB sparse fixture; Windows-covered), `jpl_licenses`
  (hangs on /mnt/d cross-OS file probing; Windows + CI covered), and
  `jpl_docs_only` under sanitizers (no Doxygen in WSL).
- GCC Release full run: 35/35 of the runnable set pass (verified
  `Passed` count; the CTest summary line is not emitted because the
  `jpl_licenses` hang from the earlier full-set invocation left a stale
  run, since excluded here).
- T17-discovered fix `b7fad34` removes the `-Wunused-but-set-variable`
  warnings GCC reported (`off_first`, `enc_start`).

### Sanitizers (WSL Ubuntu, GCC)

- ASan/UBSan (`-fsanitize=address,undefined,float-cast-overflow`):
  all applicable offline tests pass (37/37 minus install/docs-paths/
  largefile/licenses/docs-only per CI policy).
- TSan: `jpl_threads` + `jpl_mmap` pass.
- Exclusions match `.github/workflows/ci.yml` (unsanitized consumers).

### clang-cl / macOS

- clang-cl: not installed on this machine (`clang-cl.exe` absent; VS LLVM
  tools absent); covered by CI (`windows-clang-cl` job). Pending.
- macOS Clang: no macOS runner available; covered by CI (`macos-clang`
  job). Pending per maintainer agreement.

### Opt-in DE440/DE440t tier (`build/t17-real`, Release, JPLEPH/JPL_TESTPO)

- Local redirect in effect (no download): `DE440.eph`/`DE440t.eph` from
  `D:/ephem/jpl/bin`, `testpo.440[t]` from `D:/ephem/jpl/testpo`.
- `jpl_testpo_440`, `jpl_testpo_440t`, `jpl_real_ephemeris`: pass.
- Downloaded sizes/hashes: manifest DE440 102272352 B
  `29915576d0...`, DE440t 112720608 B `ddd474135f...`; local copies differ
  (DE440 `8d0dbb47...c24a9e5`, DE440t `e2bdf430...f165331ac`) because local
  spans differ -- used as-is per policy, validated by reference suites.

### Maintainer-local tier (`build/t17-real`)

- `jpl_all_ephemerides_local`: pass (29.6 s full sequential scan of every
  physical record of every present binary).
- All 27 local `jpl_testpo_*` suites (102/200/202/403/405/406/410/413/414/
  418/421/422/423/424/430/430t/431/432/432t/433/434/435/436/436t/438/438t/
  441 plus 440/440t): pass. T17-discovered fix `86fb903`: authoritative
  files without optional-dataset rows (testpo.200 has 6833 ordinary rows,
  zero T14 rows) no longer fail coverage; the duplicate-only negative
  still fails as required.
- Present set: 31 DE binaries + 29 testpo files listed in T17 (see
  `D:/ephem/jpl/bin`, `D:/ephem/jpl/testpo`); `.defective-backup` and
  `.quarantined` files correctly ignored by the sweep's name list.
- Preflight/registration proof is workflow-level (T11); locally verified
  via `ctest -N` (44 `jpl_all_ephemerides_local`, 27 non-440 testpo names).

### Final policy and provenance

- `jpl_licenses`, `jpl_workflow_policy`, `jpl_numerical_reference_integrity`
  (positive + mutated-copy negative), fixture integrity positive/negative,
  docs symbols/paths, examples, fetch rollback, install: all pass
  (covered in the runs above).

## F01-F18 ledger

| ID | Task | State | Commit | Regression | Verification | Remaining limits |
| --- | --- | --- | --- | --- | --- | --- |
| F01 | T01, T17 | Resolved | 11edbab | Fixture setup plus repeated `-j 16` | `-R "^jpl_validator_" -j 16 --repeat until-fail:20` 16/16 x20 Debug+Release (T17); single-select auto-runs setup | clang-cl/macOS hosted |
| F02 | T02 | Resolved | 9a6be7a | Distinct trailing coefficient, FILE/mmap | Focused Debug+Release boundary suites; T17 full suites | clang-cl/macOS hosted |
| F03 | T03 | Resolved | 622a41d | Equal-value valid mock, FILE/mmap | `jpl_parser`/`jpl_mmap`/`jpl_fixture` Debug+Release; ambiguous fixture still FORMAT; T17 real fixture | clang-cl/macOS hosted |
| F04 | T06 | Resolved | d0b1a6c | Invalid codes combined with range/missing data | API/convenience/numeric/light-time/frames/TT-TDB/examples Debug+Release; T17 full suites | clang-cl/macOS hosted |
| F05 | T07 | Resolved | 45c0d5c | Finite tiny-Sun scale-safe reference | jpl_aberration/numeric/convenience/examples Debug+Release; T17 ASan/UBSan | clang-cl/macOS hosted |
| F06 | T04 | Resolved | 95d4a1a | Wide sparse record with bounded-read proof | Parser matrix Debug+Release; read-limit hook reports no request above 2880; T17 overflow/DE440t | clang-cl/macOS hosted |
| F07 | T10 | Resolved | 4493ec3 (+705ddc5 diagnostic stabilization) | Forced failure restores prior destination | jpl_fetch_cache repeated Windows + Linux; T17 offline suites | clang-cl/macOS hosted |
| F08 | T11, T17 | Resolved | c3202fe | Variable/label/registration policy and dispatch | Policy test passes locally; local sweep proven in T17 | clang-cl/macOS/dispatched-run URLs |
| F09 | T14, T17 | Resolved | 6a595a8 | Exact 0.3.0 consumer and mismatch tests | jpl_install Debug+Release; 0.2.0/0.3.1/9999.0.0 rejected | clang-cl/macOS hosted |
| F10 | T08, T17 | Resolved | bb7532c (+86fb903 authoritative-rows fix) | 120 duplicate rows fail; official files pass | Synthetic serial + -j16 x20 Debug+Release; all 27 local + 440/440t testpo pass | clang-cl/macOS hosted |
| F11 | T09, T17 | Resolved | 4ac33bf | Interior corruption fails; complete real scan | Synthetic + fixture Debug+Release; `jpl_all_ephemerides_local` full scan pass | clang-cl/macOS hosted |
| F12 | T06 | Resolved | d0b1a6c | All public date APIs return NUMERIC | Same focused run; T17 full suites | clang-cl/macOS hosted |
| F13 | T05 | Resolved | 4df8cb5 | Valid distant rounded endpoint and >8-ULP negative | Focused Debug+Release suites; T17 full suites | clang-cl/macOS hosted |
| F14 | T05 | Resolved | 4df8cb5 | Big-endian zero-constant FILE/mmap success | Same focused run; T17 full suites | clang-cl/macOS hosted |
| F15 | T12, T17 | Resolved | f590a6c | Docs-only configure matrix | jpl_docs_only + licenses pass; symbols/paths/install/docs-build in T17 | clang-cl/macOS hosted |
| F16 | T13, T17 | Resolved | e3cca40 | Positive and mutated-copy integrity checks | Integrity + numerical/frame/aberration/light-time pass; mutated copy fails; T17 rerun | clang-cl/macOS hosted |
| F17 | T15, T17 | Resolved | e0e053e | Offline workflow policy plus hosted jobs | Policy passes; local validation notes updated | clang-cl/macOS/dispatched-run URLs |
| F18 | T06, T16, T17 | Resolved | d0b1a6c + f29b344 | Doxygen, local links, README/license audit | Licenses/docs-only/policy/provenance/fetch/install pass; Doxygen 1.17.0 clean; full suites in T17 | clang-cl/macOS hosted |
