# Remediation progress

Spec: ../superpowers/specs/2026-09-17-review-remediation-design.md
Baseline: 1327b20; execution-start revision 4471c67 (see evidence below).

| Task | Review IDs | State | Revision | Commands/evidence | Remaining blockers |
| --- | --- | --- | --- | --- | --- |
| T00 | Baseline only | Complete | Uncommitted | Baseline commands recorded below; Debug 8/8 pass, Release 8/8 pass | None established yet |
| T01 | R01c, R03a, R03b | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: strict Linux GCC 15.2 `build/t23-wsl` configured/built with `-Werror=implicit-function-declaration` (t23 subset 25/25 pass; threads/mmap/largefile binaries pass separately) | 32-bit, macOS, sanitizers, CI runs still pending (see T23) |
| T02 | R00, G06, G12, B05 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_dates` pass in `build/t23-clean` Debug+Release 36/36; `jpl_numeric` pass (independent time/frame/apparent truth incl. low-part) | CI/sanitizer gates pending |
| T03 | R00, R13c, R13d, S05, S06, S07 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_testpo_parser` pass in `build/t23-clean` Debug+Release | CI gates pending |
| T04 | R01a, R01b, R01c, R08a, G13 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_tests`/`jpl_api_errors` pass in `build/t23-clean` Debug+Release | CI/sanitizer gates pending |
| T05 | R01b, R01c, R06a, R06b, R09 | Partially verified — see blockers | Uncommitted | T23 fresh evidence: `jpl_parser` pass in `build/t23-clean` Debug+Release; BE header handling live: DE202 BE opens OK on Linux probe (denum 202); BUT big-endian DE200 repack in maintainer dir fails `JPL_ERROR_FORMAT` (trailing-record endpoint mismatch + extra=2 divisibility `42965216 % 6501 = 107`); `jpl_all_ephemerides_local` FAILS on it | R09 legacy-BE sweep + real tier blocked by DE200 open failure (root cause under investigation; no fix attempted in T23) |
| T06 | R10, G02 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_tt_tdb`/`jpl_numeric` pass in `build/t23-clean` Debug+Release | DE440t TT-TDB runtime blocked (see T10/T16) |
| T07 | R00, R01a, L03, L04, L05, G06, G12, B05 | Complete (per task ledger) | Uncommitted | T23 content check: all public date params are `const double jd[2]`/`double jd[2]` (`jpl.h` start/end/getters, `jpl_frames.h` 5 decls, convenience 2 decls); `src/erfa` untouched (`git diff --stat -- src/erfa` empty); `git diff --check` clean (LF warnings only) | CI gates pending |
| T08 | L01, L02 | Complete (per task ledger) | Uncommitted | T23 content check: `JPL_AU_KM 149597870.7`, `JPL_C_KM_PER_SECOND 299792.458`, derived `JPL_C_AU_PER_DAY` (no literals) in `src/jpl_private.h`; vendored ERFA AU references are third-party (`erfam.h`, `fk425/524.c` comments); stale `173.144632684` only in historical plan docs + quoted review row | None new in T23 |
| T09 | R01a, R01b, R08b, L03, G13 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_api_errors`/`jpl_numeric` pass in `build/t23-clean` Debug+Release | CI/sanitizer gates pending |
| T10 | R08c, R11, L01, G05, B01 | Partially verified — see blockers | Uncommitted | T23 fresh evidence: `jpl_lighttime` pass in `build/t23-clean` Debug+Release; BUT `jpl_testpo_440t` FAILS Debug+Release (`line 13: tt-tdb query failed: unsupported dataset or frame`; DE440t triple `(1019,13,8)` spans slots 1019-1122 == `record_double_count` 1122, rejected by `span_end >= record_double_count` guard in `src/jpl.c:1801`) | Real-tier T#17 TT-TDB blocked (no fix attempted in T23) |
| T11 | R08b, L01, S04, G13, B02 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_aberration` pass in `build/t23-clean` Debug+Release; execution-path `assert(`: only `assert()` in a `jpl_test_util.h` comment, `_Static_assert` in `src/jpl.c` (compile-time) | CI gates pending |
| T12 | R02, R04, R15, L05, B03 | Complete (per task ledger) | Uncommitted | T23 fresh evidence: `jpl_convenience` pass in `build/t23-clean` Debug+Release; frame table in `docs/validation.md` (R04 allowlist) unchanged by T23 | DE200/legacy convenience sweep blocked by open failure |
| T13 | R07, G04, G13, B06 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_mmap` pass (Debug+Release Windows; binary `mmap tests passed` on Linux) | CI/TSan gates pending |
| T14 | R07, G03, G04, B06 | Partially verified — see blockers | Uncommitted | T23 fresh evidence: `jpl_threads` pass Windows Debug+Release; Linux binary `threads tests passed` | TSan/ASan not run locally; CI required |
| T15 | R13a, R13b, R13c, R13d, R13e, L02, S05, S06, S07 | Complete (per task ledger) | Uncommitted | T23 fresh evidence: all 15 `jpl_validator_*` + `jpl_fixture_read_error` pass in `build/t23-clean` Debug+Release | Real-tier validator failure is a library TT-TDB dataset issue, not a runner false-pass (see T10) |
| T16 | R00, R02, R04, R08b, R11, L02, L04, L05, G01, G02, G05, G06, G11, G12, G13, B01, B02, B03, B04 | Partially verified — see blockers | Uncommitted | T23 fresh evidence: `jpl_numeric` pass in `build/t23-clean` Debug+Release; `jpl_testpo_440` + `jpl_real_ephemeris` + `jpl_testpo_441` pass in `build/remediation-real` Debug+Release; BUT `jpl_testpo_440t` fails (see T10) | Numerical closure blocked on DE440t TT-TDB + legacy-BE files |
| T17 | G07, G08 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_fixture`, `jpl_fixture_integrity`, `jpl_fixture_integrity_negative` pass in `build/t23-clean` Debug+Release | None new in T23 |
| T18 | R05a, R05b | Complete (per task ledger) | Uncommitted | T23 fresh evidence: `jpl_licenses` pass in `build/t23-clean` Debug+Release | Linux license run hung locally; CI required |
| T19 | R05b, R12a, R12b, R12c, G10 | Complete (per task ledger) | Uncommitted | T23 fresh evidence: `jpl_install` pass in `build/t23-clean` Debug (~144 s) + Release (~134 s); package audit: exactly 3 headers, config/version/targets files, no `m.lib` on VS generator, no `jpl_private` in consumer, no new runtime deps (`find_package` only Doxygen/Threads) | clang-cl/macOS/32-bit consumer variants pending CI |
| T20 | L06 | Complete (per task ledger; T23 did not re-audit internals) | Uncommitted | T23 fresh evidence: `jpl_fetch_cache` pass in `build/t23-clean` Debug+Release | None new in T23 |
| T21 | R00, R04, R05a, R14a, R14b, L04, L05, G08, B01, B02, B03, B04, B05, B06, S01, S02, S03 | Complete (per task ledger) | Uncommitted | T23 fresh evidence: `jpl_docs_symbols`/`jpl_docs_paths`/`jpl_examples`/`jpl_licenses` 4/4 pass in `build/t23-docs`; live docs carry no 70 s / 2 ms-floor / stale-c-literal claims (remaining hits are labeled historical review/plan quotations) | POSIX docs run pending CI |
| T22 | R03a, R03b, R05a, R12c, R13d, R14b, G03, G09, G10, S02 | Partially verified — see blockers | Uncommitted | T23 recorded fresh per-configuration evidence in `docs/validation.md`; workflows `.github/workflows/ci.yml` + `ephemerides.yml` present; no workflow execution authorized in T23 | clang-cl, macOS, 32-bit, sanitizers, Linux install/licenses, full Linux suite, actual CI runs: all pending/blocked |
| T23 | All IDs (closure) | In progress | Uncommitted | This acceptance run (see evidence below); `git diff --check` clean (LF warnings only); `src/erfa` diff empty | Blockers: DE440t TT-TDB guard; DE200-BE open; unrun platform/sanitizer/CI gates |

## T00 baseline evidence

All commands run under PowerShell 5.1 from the worktree root
`D:\src\libjpl\.worktrees\remediation`, branch `refactor/review-remediation`.

### Git status / log

- `git status --short --branch` at start: clean tree, only output
  `## refactor/review-remediation` (no staged, unstaged, or untracked changes).
- `git log --oneline -10` at start:
  `4471c67 docs(review): save project review, remediation design, and implementation plans`
  on top of `1327b20 chore(release): bump version to 0.2.0`.
- Execution-start revision (full): `4471c673295dc2a7a35e8458f5ee6b2f2d18ae3b`.
- `git diff --check` and `git diff --stat` at start: empty (no output).
- No library code changed by this task; only this ledger file is new
  (left uncommitted — no commit authorization was given).

### Build configuration

- `Test-Path -LiteralPath "build"` returned `False`; `Test-Path -LiteralPath "."`
  returned `True` before allowing CMake to create the nested build path.
- Configure: `cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF`
  succeeded. Generator: Visual Studio 18 2026, MSVC 19.51.36257.0,
  Windows SDK 10.0.26100.0. CMake version 4.3.4.

### Fresh Debug results (`build/remediation`)

- `cmake --build build/remediation --config Debug --parallel`: succeeded,
  zero reported warnings in captured output.
- `ctest --test-dir build/remediation -C Debug --output-on-failure`:
  **8/8 passed** (jpl_tests, jpl_lighttime, jpl_aberration, jpl_frames,
  jpl_convenience, jpl_mmap, jpl_tt_tdb, jpl_fixture), ~113 s total.
  Note: Debug `jpl_frames` took ~32 s on first run.

### Fresh Release results (`build/remediation`)

- `cmake --build build/remediation --config Release --parallel`: succeeded.
- `ctest --test-dir build/remediation -C Release --output-on-failure`:
  **8/8 passed**, same 8 suites, ~91 s total.

### Actual suite count

- `ctest -N` lists **8 tests**. The offline default (without `JPLEPH`-enabled
  real-data tier) therefore runs 8 suites, not the stale AGENTS.md count of 6.
  (AGENTS.md correction belongs to a later task; recorded here as observed.)
- `JPLEPH` is set in this environment (`D:\ephem\jpl\bin`, containing DE102
  through DE441 plus checksums), but the baseline build used
  `LIBJPL_FETCH_EPHEMERIDES=OFF`, so the opt-in real-data tier did not run.

### Environment inventory

| Environment | Status |
| --- | --- |
| MSVC (VS 18 2026, 19.51.36257.0, x64) | Present; Debug + Release built and tested |
| clang-cl | Not found on PATH (`where.exe clang-cl` failed) — pending gate |
| Linux GCC/Clang | Not available on this Windows host — pending gate (CI) |
| macOS CI | Not available locally — pending gate (CI) |
| 32-bit build | Not configured or tested — pending gate |
| Doxygen | Present, version 1.17.0 |
| Sanitizers (ASan/UBSan) | Not configured — pending gate |
| CMake | Present, version 4.3.4 |
| `cl` on PATH | Not directly invocable in PowerShell 5.1 shell; toolchain works via VS generator |
| `build/remediation` | Git-ignored (`git check-ignore` confirms); safe to rebuild |

### Known pre-existing items (not T00 defects)

- The strict-POSIX C17 failure noted in the plan belongs to T01; no flags
  were weakened for this baseline.

## T23 acceptance evidence (2026-09-19, uncommitted T00-T22 tree)

All commands PowerShell 5.1 from the worktree root unless noted;
WSL notes use Ubuntu GCC 15.2.0 / CMake 4.2.3. `build/` is git-ignored;
`build/t23-clean`, `build/t23-docs`, `build/t23-wsl`,
`build/remediation-real` were fresh configures (parent `build/`
verified present with `Test-Path`). No commits; no maintainer-local
files altered (local DE/testpo files only read, incl. probes).

- `cmake -S . -B build/t23-clean -DLIBJPL_FETCH_EPHEMERIDES=OFF`:
  configure OK (VS 18 2026, MSVC 19.51.36257.0).
- `cmake --build build/t23-clean --config Debug --parallel`: OK, zero
  warnings observed. `ctest --test-dir build/t23-clean -C Debug
  --output-on-failure`: **36/36 pass** (~406 s; `jpl_install` ~144 s).
- `cmake --build build/t23-clean --config Release --parallel`: OK.
  `ctest --test-dir build/t23-clean -C Release --output-on-failure`:
  **36/36 pass** (~437 s; `jpl_install` ~134 s).
- `cmake -S . -B build/t23-docs -DLIBJPL_FETCH_EPHEMERIDES=OFF
  -DLIBJPL_BUILD_DOCS=ON` + Debug build OK; `ctest --test-dir
  build/t23-docs -C Debug -R 'jpl_docs|jpl_examples|jpl_licenses'`:
  **4/4 pass**.
- Linux strict: `cmake -S . -B build/t23-wsl
  -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF
  "-DCMAKE_C_FLAGS=-Werror=implicit-function-declaration"` configure +
  build OK; 25-test subset
  (`-E 'jpl_install|jpl_licenses|jpl_fetch_cache|jpl_largefile|jpl_threads|jpl_mmap|jpl_examples|jpl_fixture'`)
  **25/25 pass**; `jpl_threads_tests` prints `threads tests passed`;
  `jpl_mmap_tests` prints `mmap tests passed`; `jpl_largefile` prints
  `largefile tests passed` (oversize skipped, `SIZE_MAX == UINT64_MAX`).
  Full Linux suite, `jpl_install`, `jpl_licenses`, `jpl_fetch_cache`:
  **incomplete locally** (combined selections hung past 240-850 s
  timeouts; recorded as blocked, not passed).
- Real tier: `cmake -S . -B build/remediation-real
  -DLIBJPL_FETCH_EPHEMERIDES=ON` resolved per-file local sources only
  (`D:\ephem\jpl\bin\DE440.eph`, `DE440t.eph`,
  `D:/ephem/jpl/testpo/testpo.440[.t]`; no downloads). Debug+Release:
  `jpl_testpo_440`, `jpl_real_ephemeris`, `jpl_testpo_441` pass;
  **`jpl_testpo_440t` FAILS** (`line 13: tt-tdb query failed:
  unsupported dataset or frame` both configs);
  **`jpl_all_ephemerides_local` FAILS** (present
  `DE200.eph` fails to open: `invalid JPL DE format`). Hashes/sizes in
  `docs/validation.md` real-tier section.
- Content searches: public date decls all `[2]` pairs; constants from
  `src/jpl_private.h` (`JPL_AU_KM`, `JPL_C_KM_PER_SECOND`, derived
  `JPL_C_AU_PER_DAY`); stale `173.144632684` only in historical plans +
  quoted review row; no live 70 s / 2 ms-floor claims; execution-path
  `assert(` none (comment + `_Static_assert` only); `git diff --stat --
  src/erfa` empty; `git diff --check` clean apart from LF warnings;
  package has exactly 3 headers, config/version/targets files, no
  `m.lib`, no `jpl_private` in consumer, `find_package` only
  Doxygen/Threads. `build/` trees ignored; probe scripts under
  `build/` (ignored scratch, not tracked evidence).
- Diff review: 26 tracked files changed (5139+/915-), 27 untracked
  paths (new tests, docs, workflows, license files, `src/jpl_date.c`,
  `cmake/`, `tools/`); no generated reference/provenance artifacts
  inside the diff beyond the intended tracked fixtures; no new runtime
  dependency; no private helper exposed in `include/`.
