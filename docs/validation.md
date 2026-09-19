<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Validation notes (T22 procedures and evidence)

> Owned incrementally: T12 contributes the frame-source table below; T21
> owns the scientific contracts; T22 owns the complete validation
> procedures and per-configuration evidence below. T23 records final
> closure; unrun configurations stay explicitly pending (a skipped tier
> is never a pass).

## Frame-source table (T12, R04)

Classification is content-derived from the header DENUM via an explicit
allowlist in `src/jpl.c` (`classify_frame`), exposed as
`jpl_header.reference_frame`. Source: JPL export information,
https://ssd.jpl.nasa.gov/planets/eph_export.html, which distinguishes
DE102 (dynamical equator/equinox of 1950), DE200/DE202 (dynamical
equator/equinox of 2000), and modern ICRF releases.

| DENUM | `jpl_reference_frame` | Basis |
| --- | --- | --- |
| 102 | `JPL_FRAME_DYNAMICAL_1950` | JPL export: referred to dynamical equator/equinox of 1950 |
| 200, 202 | `JPL_FRAME_DYNAMICAL_2000` | JPL export: referred to dynamical equator/equinox of 2000 |
| 403, 405, 406, 410, 413, 421, 422, 423, 430, 431, 432, 440, 441 | `JPL_FRAME_ICRF` | JPL export: documented ICRF-aligned releases (explicit allowlist, no numeric threshold) |
| all other values (e.g. 999, 0) | `JPL_FRAME_UNKNOWN` | No cited ICRF evidence; core native-frame reads still succeed, ICRS convenience returns `JPL_ERROR_UNSUPPORTED` |

Notes:

- Unknown values remain readable by core (`jpl_get_state`,
  `jpl_get_state_lighttime`); only ICRS convenience
  (`jpl_planet_position`, `jpl_moon_position`) gates on
  `reference_frame == JPL_FRAME_ICRF`.
- Frame eligibility is checked even for identical-body calls so a zero
  vector never implies legacy-frame support.
- Time-segmented variants use the same content-derived DENUM/frame
  classification.
- DE404 is a known real ephemeris: per Standish (1996), it is the same
  integration as DE403, condensed by omitting nutations and librations
  and by using fewer interpolation coefficients. It is not currently
  supported pending an authoritative file or layout specification.

## Scientific contracts (T21)

- Two-part dates: every computational epoch is a `double[2]` pair whose
  sum is the epoch; both parts survive arithmetic and ERFA calls
  (`eraEcm06`, `eraDtdb`). Scalar full-epoch values exist only at the
  serialization boundary (on-disk record fields, decoded as
  `{scalar, 0.0}`); splitting an already-rounded scalar is unsupported.
- TT-TDB model: `jpl_tt_to_tdb()` is the geocentric Fairhead &
  Bretagnon (1990) model via ERFA with nullified topocentric terms.
  Correction amplitude is roughly 1.7 ms; ERFA cites better than +/-3 ns
  model accuracy over 1950-2050 relative to DE405 integrations. The
  `1e-15`-day TDB-to-TT residual and `1e-12`-day light-time residual are
  solver tolerances, not accuracy claims. Interpolated TT-TDB from *t
  files is not mathematically exact, and file coverage is not a
  model-accuracy guarantee. Interpolation/model error and arithmetic
  precision are reported separately.
- Light-time: observer barycentric state fixed at reception;
  `tau = range(target(receive - tau), observer(receive))/c` with the
  emission epoch as a pair. Velocity is the mixed-epoch difference
  `v_target(emission) - v_center(reception)`, not an apparent tracking
  rate.
- Frames: `jpl_frame_rotation_matrix()` and the ecliptic helpers rotate
  position and velocity at a fixed epoch (`R*v`); no `dR/dt` term.
- Apparent place means light-time plus stellar aberration only
  (including the <=0.4uas solar-potential term when the Sun distance is
  known; not gravitational bending). The body-center observer is the
  center barycenter at reception (SSB/center 0/12: zero velocity).
  Adding apparent Moon/Earth legs mixes epochs and is approximate.
- Legacy/unknown frames are rejected by convenience calls
  (`JPL_ERROR_UNSUPPORTED`); frame/time-model validity intervals are
  narrower than file coverage.
- Handles: FILE and mmap contents must stay immutable while any reader
  is active; clones do not survive path disappearance (IO error, NULL
  output); closing a handle while a reader uses it is unsupported, and
  clone-vs-close races need external synchronization.
- Outputs: computed results are transactional (unchanged on failure,
  exact aliasing only); raw record/constant buffers may be partially
  written after I/O failure.

## Procedures and evidence (T22)

CI workflows: `.github/workflows/ci.yml` (offline gates: Windows MSVC
Debug/Release, Windows clang-cl Debug/Release, Ubuntu GCC Debug/Release,
Ubuntu Clang Debug/Release, macOS Clang Debug/Release, Linux 32-bit
Debug/Release, ASan/UBSan, TSan, docs/licenses/fixtures) and
`.github/workflows/ephemerides.yml` (`workflow_dispatch` only: DE440/DE440t
fetch tier in both configurations plus the maintainer-runner local sweep).
Default CI remains offline with respect to ephemeris data
(`LIBJPL_FETCH_EPHEMERIDES=OFF`); workflow files alone authorize no runs.
Actual CI run results are required before T23 marks platform findings
closed. Hosted CI is never assumed to own the maintainer's local
ephemerides, and maintainer-local files are never transmitted as
artifacts. Sanitizer jobs are separate: ASan/UBSan and TSan are never
combined; `jpl_install`/`jpl_docs_paths` are excluded only from sanitizer
sub-builds (they configure an unsanitized consumer) and run in all
regular jobs.

### Common commands

PowerShell (Windows MSVC, multi-config generator):

```powershell
cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/remediation --config Debug --parallel
ctest --test-dir build/remediation -C Debug --output-on-failure
cmake --build build/remediation --config Release --parallel
ctest --test-dir build/remediation -C Release --output-on-failure
```

POSIX (Linux/macOS, single-config; use separate Debug/Release directories
with explicit `-DCMAKE_BUILD_TYPE`):

```sh
cmake -S . -B build/ci -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/ci --config Debug --parallel
ctest --test-dir build/ci -C Debug --output-on-failure
cmake -S . -B build/ci-rel -DCMAKE_BUILD_TYPE=Release -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/ci-rel --config Release --parallel
ctest --test-dir build/ci-rel -C Release --output-on-failure
```

Prerequisites: CMake 3.20+, a C17 compiler, Doxygen 1.9+ only for the
docs job (`-DLIBJPL_BUILD_DOCS=ON`). No network access is needed for the
default offline tier. Paths with spaces must stay quoted.

### Windows MSVC (Debug and Release)

Same PowerShell commands as Common above (`/W4 /permissive-`, zero
warnings). Observed 2026-09-19 (worktree, MSVC 19.51.36257.0,
T23 `build/t23-clean` fresh offline configure): Debug 36/36 pass
(~406 s incl. `jpl_install` ~144 s), Release 36/36 pass (~437 s
incl. `jpl_install` ~134 s). Focused docs build `build/t23-docs`
(`-DLIBJPL_BUILD_DOCS=ON`): `jpl_docs_symbols`/`jpl_docs_paths`/
`jpl_examples`/`jpl_licenses` 4/4 pass. Supersedes the older
`build/t22-red` 33/33 note (pre-T22 suite count).

### Windows clang-cl (Debug and Release, Ninja, initialized MSVC environment)

Prerequisite: LLVM with clang-cl plus a full MSVC/SDK install; missing
components fail configuration clearly. POSIX shell (Git Bash) or CMD with
`vcvars64`:

```sh
cmake -S . -B build/ci -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/ci --config Debug --parallel
ctest --test-dir build/ci -C Debug --output-on-failure
```

The `jpl_install` consumer runs in this job and proves there is no `m.lib`
dependency. Latest observed result: pending (no local clang-cl; CI run
required before T23).

### Linux GCC / Clang, strict C17 (Debug and Release)

Prerequisite: GCC or Clang with `-Werror=implicit-function-declaration`
for strict POSIX project code; MSVC flags untouched.

```sh
cmake -S . -B build/ci -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF "-DCMAKE_C_FLAGS=-Werror=implicit-function-declaration"
cmake --build build/ci --config Debug --parallel
ctest --test-dir build/ci -C Debug --output-on-failure
```

Observed 2026-09-19 (WSL Ubuntu, GCC 15.2.0, Debug, T23
`build/t23-wsl` with `-Werror=implicit-function-declaration`): 25/25
pass on the subset excluding `jpl_install`/`jpl_licenses`/
`jpl_fetch_cache`/`jpl_largefile`/`jpl_threads`/`jpl_mmap`/
`jpl_examples`/`jpl_fixture` (slow suites run separately below); then
`jpl_threads` binary `threads tests passed`, `jpl_mmap` binary `mmap
tests passed`, `jpl_largefile` binary `largefile tests passed`
(oversize case skipped with `SIZE_MAX == UINT64_MAX`).
`jpl_install`/`jpl_licenses`/`jpl_fetch_cache` on Linux: not completed
locally (the combined selection hung past successive 240-850 s
timeouts; CI run required before closure of G09/G10). Full-suite
`ctest --test-dir build/t23-wsl` without exclusions also exceeded the
900 s tool timeout and is recorded as incomplete, not a pass.
Supersedes the older T22 filtered-run note.

### macOS Clang (Debug and Release)

```sh
cmake -S . -B build/ci -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF "-DCMAKE_C_FLAGS=-Werror=implicit-function-declaration"
cmake --build build/ci --config Debug --parallel
ctest --test-dir build/ci -C Debug --output-on-failure
```

Plus the installed-consumer smoke test (`jpl_install` runs in every
regular job). Latest observed result: pending (no local macOS host; a
macOS pass must never be inferred from Windows compilation; CI run
required before T23).

### Linux 32-bit (-m32, Debug and Release)

Prerequisite: multilib toolchain installed explicitly
(`gcc-multilib g++-multilib`); without it the configure fails.

```sh
cmake -S . -B build/ci -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_FETCH_EPHEMERIDES=OFF "-DCMAKE_C_FLAGS=-m32 -Werror=implicit-function-declaration" "-DCMAKE_EXE_LINKER_FLAGS=-m32"
cmake --build build/ci --config Debug --parallel
ctest --test-dir build/ci -C Debug --output-on-failure
```

Includes the large-file tests (`jpl_largefile`, CTest timeout 600 s).
Address-space-dependent mmap rejections are recorded as expected explicit
cases, not silent omissions. Latest observed result: pending (WSL host
lacks multilib: `Scrt1.o`/`crti.o` absent, `-m32` link fails; CI run
required before T23).

### Sanitizers (Linux, separate jobs)

ASan/UBSan (Debug; floating-to-integer overflow checks where supported):

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined,float-cast-overflow" -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure -E 'jpl_install|jpl_docs_paths'
```

TSan (Debug, concurrency suites only):

```sh
cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=-fsanitize=thread -fno-omit-frame-pointer" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/tsan --parallel
ctest --test-dir build/tsan -R 'jpl_threads|jpl_mmap' --output-on-failure
```

An unsupported sanitizer runtime is a blocked gate requiring a supported
runner, not a pass or an excuse to suppress diagnostics. Latest observed
result: pending (sanitizer sub-builds not yet run locally; CI run
required before T23).

### Installed package (all regular jobs)

Covered by `jpl_install` in every ordinary build (default, version
mismatch, relocated/spaces prefix, custom destinations, PIC paths;
derives notice paths from `JPL_NOTICE_DATADIR`). Latest observed result:
Windows MSVC `build/t23-clean` Debug and Release 1/1 pass each (2026-09-19,
~144 s / ~134 s); other platforms pending CI runs.

### Docs (Doxygen + symbols + spaces paths)

Prerequisite: Doxygen installed (1.17.0 locally, 1.9+ known good).

PowerShell:

```powershell
cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF -DLIBJPL_BUILD_DOCS=ON
cmake --build build/remediation --config Debug --parallel
ctest --test-dir build/remediation -C Debug -R 'jpl_docs|jpl_examples|jpl_licenses' --output-on-failure
```

POSIX equivalent uses `-DCMAKE_BUILD_TYPE=...` single-config with
`-DLIBJPL_BUILD_DOCS=ON`. Latest observed result: Windows MSVC T23
`build/t23-docs` Debug docs/examples/licenses focused set 4/4 pass
(supersedes the T21 4/4 note); CI docs job re-runs the full
symbol/spaces/license/fixture-cache selection.

### Offline cache

```sh
ctest --test-dir build/ci -C Debug --output-on-failure -R 'jpl_fetch_cache|jpl_fixture_integrity|jpl_fixture_integrity_negative'
```

Exercises the production resolver (`cmake/LibjplTestData.cmake`) against
disposable build-tree artifacts with no network. Latest observed result:
T23 Windows MSVC `build/t23-clean` Debug+Release in-suite pass; Linux
`jpl_fetch_cache` selection not completed locally (see Linux section).

### Real tier (opt-in only, never default)

Requires explicit `LIBJPL_FETCH_EPHEMERIDES=ON` plus local or download
sources; `JPLEPH`/`JPL_TESTPO` env dirs only redirect per-file sources
inside this tier and never enable it alone.

```sh
cmake -S . -B build/remediation-real -DLIBJPL_FETCH_EPHEMERIDES=ON
cmake --build build/remediation-real --config Debug --parallel
ctest --test-dir build/remediation-real -C Debug --output-on-failure
```

Runs DE440/DE440t testpo in both configurations, retains summary
counts/hashes as artifacts (CI), and runs the local legacy/long-span
sweep only on a configured maintainer runner. Latest observed result
(T23, 2026-09-19, `build/remediation-real` resolving per-file local
sources `D:\ephem\jpl\bin\DE440.eph`,
`D:\ephem\jpl\bin\DE440t.eph`, `D:/ephem/jpl/testpo/testpo.440`,
`D:/ephem/jpl/testpo/testpo.440t`; no downloads; maintainer-local files
unaltered): Debug `jpl_testpo_440` pass, `jpl_real_ephemeris` pass,
`jpl_testpo_441` pass (local `JPL_TESTPO` sweep entry), Release same
3/3 pass; `jpl_testpo_440t` FAILS in both configurations
(`line 13: reference mismatch: tt-tdb query failed: unsupported
dataset or frame` — DE440t TT-TDB triple `(1019,13,8)` spans slots
1019-1122 at `record_double_count` 1122, rejected by the boundary-equality
guard `span_end >= record_double_count` in `src/jpl.c`); local
`JPLEPH` sweep entry `jpl_all_ephemerides_local` FAILS (big-endian
`D:\ephem\jpl\bin\DE200.eph` opens `JPL_ERROR_FORMAT`: trailing-record
hypothesis requires `endpoints_match(sched_trail, tfirst)` but both the
last and trailing records carry `[2513360.5, 2513392.5]`, 32 days from
the scheduled `[2513392.5, ...]`; the plain extra=2 hypothesis fails the
divisibility check since `42965216 % 6501 = 107`). Source hashes:
DE440.eph `8d0dbb47...c24a9e5` (102272352 B), DE440t.eph `e2bdf430...f165331ac`
(112720608 B), testpo.440 `45084a7f...16693429f`, testpo.440t
`9801f019...4491c7a3f2ad` (858238 B each). CI `ephemerides.yml` is
`workflow_dispatch` only.
