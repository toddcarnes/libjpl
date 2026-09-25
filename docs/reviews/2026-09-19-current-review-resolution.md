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

## F01-F18 ledger

| ID | Task | State | Commit | Regression | Verification | Remaining limits |
| --- | --- | --- | --- | --- | --- | --- |
| F01 | T01, T17 | Open | - | Fixture setup plus repeated `-j 16` | T00 reproduction above; fix pending | Parallel stress, docs-only, platform gates pending |
| F02 | T02 | Open | - | Distinct trailing coefficient, FILE/mmap | Fix pending | - |
| F03 | T03 | Open | - | Equal-value valid mock, FILE/mmap | Fix pending | - |
| F04 | T06 | Open | - | Invalid codes combined with range/missing data | Fix pending | - |
| F05 | T07 | Open | - | Finite tiny-Sun ERFA/scale-safe reference | Fix pending | - |
| F06 | T04 | Open | - | Wide sparse record with bounded-read proof | Fix pending | - |
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
