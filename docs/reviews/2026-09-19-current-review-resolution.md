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

## F01-F18 ledger

| ID | Task | State | Commit | Regression | Verification | Remaining limits |
| --- | --- | --- | --- | --- | --- | --- |
| F01 | T01, T17 | Resolved | (this commit) | Fixture setup plus repeated `-j 16` | `-R "^jpl_validator_" -j 16 --repeat until-fail:10` 15/15 x10; single-select auto-runs setup; full parallel 36/36 (threads standalone) | T17 stress/docs-only/platform gates |
| F02 | T02 | Resolved | (this commit) | Distinct trailing coefficient, FILE/mmap | Focused Debug+Release boundary suites | T17 full acceptance |
| F03 | T03 | Resolved | (this commit) | Equal-value valid mock, FILE/mmap | `jpl_parser`/`jpl_mmap`/`jpl_fixture` Debug+Release; ambiguous fixture still FORMAT | T17 real-fixture tier |
| F04 | T06 | Open | - | Invalid codes combined with range/missing data | Fix pending | - |
| F05 | T07 | Open | - | Finite tiny-Sun ERFA/scale-safe reference | Fix pending | - |
| F06 | T04 | Resolved | (this commit) | Wide sparse record with bounded-read proof | Parser matrix Debug+Release; read-limit hook reports no request above 2880 | T17 real-tier overflow/DE440t rerun |
| F07 | T10 | Open | - | Forced failure restores prior destination | Fix pending | - |
| F08 | T11, T17 | Open | - | Variable/label/registration policy and dispatch | Fix pending | macOS hosted dispatch pending everywhere |
| F09 | T14, T17 | Open | - | Exact 0.3.0 consumer and mismatch tests | Fix pending | - |
| F10 | T08, T17 | Open | - | 120 duplicate rows fail; official files pass | Fix pending | - |
| F11 | T09, T17 | Open | - | Interior corruption fails; complete real scan | Fix pending | - |
| F12 | T06 | Open | - | All public date APIs return NUMERIC | Fix pending | - |
| F13 | T05 | Open | - | Valid distant rounded endpoint and >8-ULP negative | Fix pending | - |
| F14 | T05 | Open | - | Big-endian zero-constant FILE/mmap success | Fix pending | - |
| F15 | T12, T17 | Open | - | Docs-only configure matrix | Fix pending | - |
| F16 | T13, T17 | Open | - | Positive and mutated-copy integrity checks | Fix pending; canonical-LF hashes disagree with plan prose, will regenerate per T13-Step1, not copy mechanically | - |
| F17 | T15, T17 | Open | - | Offline workflow policy plus hosted jobs | Fix pending | - |
| F18 | T06, T16, T17 | Open | - | Doxygen, local links, README/license audit | Fix pending | - |
