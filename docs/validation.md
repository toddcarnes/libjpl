<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Validation notes

Current acceptance is recorded first. The dated 2026-09-19 results below
remain as historical observations, including tests that were pending then;
a skipped tier is never a pass.

## Review follow-up acceptance (2026-09-25)

Evidence for [F1-F7](reviews/2026-09-25-review-followup-resolution.md)
was collected on `bugfix/review-followup-fixes`, forked from revision
`05e9c6f`. Earlier T22/T23 evidence below describes older
worktrees and does not supersede these runs.

- Windows MSVC 19.51 / Visual Studio 18 2026, fresh offline
  `build/followup-accept`: Debug **42/42**, Release **42/42**; zero
  compiler warnings. `jpl_install` checks both the tests-ON installed
  archive and the tests-OFF custom-destination archive for defined
  `jpl_test_*` symbols. The pre-fix archive contained both leaked hooks;
  the new checker failed on the first one.
- Doxygen 1.17.0 with `WARN_AS_ERROR=YES`: `docs` builds; docs symbols,
  paths with spaces, and docs-only matrix pass **3/3** in
  `build/followup-docs`.
- Ubuntu WSL GCC 15.2, source copied once from the Windows worktree to
  `/home/todd/libjpl-review` (native Linux filesystem), native Debug
  `build/followup-linux-debug` **42/42**, native Release
  `build/followup-linux-release` **42/42**, both with
  `-Werror=implicit-function-declaration`, zero compiler warnings.
  The **whole** offline suite ran in each configuration, including
  `jpl_install`, `jpl_licenses`, `jpl_largefile`, `jpl_threads`,
  `jpl_mmap`, and `jpl_docs_only`. Nothing was built or tested on
  `/mnt/d`. The initial Debug run stopped at `jpl_docs_only` because
  Ubuntu had Doxygen but lacked Graphviz `dot`; after installing Graphviz
  14.1.2 inside Ubuntu, a fresh full run passed. This explains the
  historical cross-OS mount-related subset/timeout notes below rather
  than silently treating them as a full pass.
- Ubuntu native ASan/UBSan/float-cast-overflow **40/40** applicable offline
  tests (install/docs-only omitted from this sanitizer-only run; both
  passed in the unsanitized full runs). Native TSan `jpl_threads` and
  `jpl_mmap` **2/2**. Python-free proof: with `PATH=/nonexistent` and no
  Python executable discoverable, the four fixture-tool/integrity tests
  passed **4/4**. All five C mutation modes and canonical copies were
  compared byte-for-byte against the former Python outputs before its
  removal.

These checks cover the current Windows MSVC and Ubuntu GCC paths. This
follow-up did not run clang-cl, macOS, or the optional real-ephemeris
download tier; no pass is inferred for those configurations.

Merge acceptance found an additional worktree-layout issue: running
`jpl_licenses` and `jpl_install` from the main checkout picked up files
inside the ignored `.worktrees/` directory as unlisted source. The license
checker now excludes that directory, with a positive synthetic nested-
worktree regression in `jpl_licenses`. Both focused tests pass with the
worktree still present. The main checkout at `0038224` passed the full
MSVC Debug `build/merge-verify` suite **42/42** with the linked worktree
present. After syncing the fix into the native WSL copy, the full GCC
Debug and Release suites passed **42/42** each.

## Frame-source table

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

## Scientific contracts

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
- Frames: `jpl_frame_rotation_matrix()` and the ecliptic helper rotate
  position and velocity at a fixed epoch (`R*v`); no `dR/dt` term.
- In `jpl_planet_position()` and `jpl_moon_position()`, apparent place
  means light-time plus stellar aberration only
  (including the <=0.4uas solar-potential term when the Sun distance is
  known; not gravitational bending). The body-center observer is the
  center barycenter at reception (SSB/center 0/12: zero velocity).
  Adding apparent Moon/Earth legs mixes epochs and is approximate.
- `jpl_apparent_ecliptic_position()` is a distinct geocentric path for
  ICRF-aligned DE files. It adds finite-source
  solar light deflection before aberration (no self-deflection for the
  Sun), then returns longitude and latitude in the IAU2006/2000A true
  ecliptic and equinox of reception TT, plus range in AU. The older
  `jpl_planet_position()` and `jpl_moon_position()` keep their existing
  correction policy and state-velocity contract.
  Signed apparent longitude speed (degrees/TT day) comes from neighboring
  corrected positions. Central differences use +/-0.01 day; at coverage
  boundaries they become one-sided. Radial range-rate (km/s) is the
  Horizons-style projection of mixed-epoch target-minus-observer velocity
  on the light-time-corrected line of sight, not a derivative of range.
- Pinned Horizons DE441 observer references contain 22 TT dates for each
  of the Sun, Moon, and eight planet barycenters. Quantity 45 supplies
  airless apparent ICRF RA/Dec and is rotated into the modern frame for
  comparison; quantity 31 is an IAU76/80 ecliptic diagnostic, not an
  interchangeable expected longitude. The committed DE441 slice checks
  120 apparent rows offline; the full local DE441 file checks 220 when
  `JPLEPH` is set. Deep-time apparent residuals are diagnostics; JPL
  testpo covers full-span *geometric* interpolation. Quantity 20's signed
  range-rate is checked against the helper in km/s; neighboring Q45
  positions check longitude speed in degrees/TT day. Exact request
  settings and source evidence are in `tests/data/PROVENANCE.md`.
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

## Platform support

64-bit only. 32-bit builds are unsupported: they cannot address the
multi-gigabyte long-span ephemerides (DE431/DE441), and macOS offers no
32-bit target. Size and offset guards remain in the code as defense in
depth, but no 32-bit configuration is tested or claimed. The historical
32-bit job, multilib instructions, and width-conditional test scaffolding
have been removed.

## Language bindings

Official bindings are provided for four languages. All require the shared
library (`-DLIBJPL_BUILD_SHARED=ON`) and are built on demand via CMake
options (default `OFF`). Each binding targets the same C API and passes
the same offline test suite.

### C++ (`bindings/cpp/`)

Header-only RAII wrapper (`jpl::Ephemeris`, `jpl::JplError`, members of `jpl::Ephemeris`: `planet_position`, `moon_position`, `tt_to_tdb`).
CMake option: `LIBJPL_BUILD_CPP_BINDING=ON`. Installs headers to
`${CMAKE_INSTALL_INCLUDEDIR}/jpl/`.

### Python (`bindings/python/`)

cffi wrapper: `libjpl.Ephemeris` context manager, `libjpl.JplError`,
`libjpl.get_state`/`planet_position` helpers. CMake option:
`LIBJPL_BUILD_PYTHON_BINDING=ON`. Requires Python 3 + `cffi`.

### C# (`bindings/csharp/`)

P/Invoke with `SafeHandle` wrapper: `LibJpl.JplEphemeris : SafeHandle`,
`LibJplApi.Open`/`OpenMmap`, `JplStatus`/`JplTarget`/`JplByteOrder`/`JplReferenceFrame`
enums. CMake option: `LIBJPL_BUILD_CSHARP_BINDING=ON`. Requires .NET 8 SDK.

### VB.NET (`bindings/vbnet/`)

Direct port of C# API: `LibJpl.JplEphemeris : SafeHandle`,
`LibJplApi.Open`/`OpenMmap`, identical enums and exceptions. CMake option:
`LIBJPL_BUILD_VBNET_BINDING=ON`. Requires .NET 8 SDK.

Binding tests are executed in CI on Windows (`bindings-windows` job) and Linux (`bindings-linux` job). C++ tests link against the static library, while Python, C#, and VB.NET link against the shared library.

## Procedures and historical evidence (2026-09-19 checkpoint)

The commands below remain useful, but results marked “pending” or “not
completed” describe this checkpoint only. See the current acceptance
section above before treating an older result as the latest evidence.

CI workflows: `.github/workflows/ci.yml` (offline gates: Windows MSVC
Debug/Release, Windows clang-cl Debug/Release, Ubuntu GCC Debug/Release,
Ubuntu Clang Debug/Release, macOS Clang Debug/Release,
ASan/UBSan, TSan, docs/licenses/fixtures) and
`.github/workflows/ephemerides.yml` (`workflow_dispatch` only: DE440/DE440t
fetch tier in both configurations plus the maintainer-runner local sweep).
Both workflows declare top-level `permissions: contents: read` and pin
third-party actions to reviewed full commit SHAs (checkout/cache/
upload-artifact/msvc-dev-cmd, release line kept in a trailing comment).
The local sweep requires the dedicated `[self-hosted, libjpl-ephemerides]`
runner label plus nonempty `JPLEPH` and `JPL_TESTPO` repository variables
(preflight fails when either is empty or names a missing directory), proves
via `ctest -N` that `jpl_all_ephemerides_local` and at least one local
non-440 `jpl_testpo_` suite are registered (parser-only tests cannot
satisfy it), then runs only `jpl_all_ephemerides_local` and
`jpl_testpo_[0-9]` local registrations.
Default CI remains offline with respect to ephemeris data
(`LIBJPL_FETCH_EPHEMERIDES=OFF`); workflow files alone authorize no runs.
Actual CI results, not workflow definitions, establish platform coverage.
Hosted CI is never assumed to own the maintainer's local
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

Prerequisites: CMake 3.20+, a C17 compiler, and Doxygen 1.9+ for the docs
job (`-DLIBJPL_BUILD_DOCS=ON`); Linux docs checks also need Graphviz `dot`
with the tested Doxygen setup. No network access is needed for the default
offline tier. Paths with spaces must stay quoted.

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
dependency. At this 2026-09-19 checkpoint, clang-cl was pending (no local
compiler; a CI run was required).

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
`jpl_install`/`jpl_licenses`/`jpl_fetch_cache` on Linux were not completed
at this checkpoint (the combined selection hung past successive 240-850 s
timeouts). Full-suite
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
regular job). At this checkpoint macOS was pending. A local macOS Tahoe VM
was available, but direct transfer via VMware shared folders crashed the
guest during unzip, so no local macOS build was recorded at the checkpoint.
A macOS pass cannot be inferred from Windows compilation; a hosted or guest
run is required to establish macOS coverage.

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
runner, not a pass or an excuse to suppress diagnostics. At this checkpoint,
sanitizer sub-builds had not yet run locally; a CI run was needed.

### Installed package (all regular jobs)

Covered by `jpl_install` in every ordinary build (default, version
mismatch, relocated/spaces prefix, custom destinations, PIC paths;
derives notice paths from `JPL_NOTICE_DATADIR`). At this checkpoint:
Windows MSVC `build/t23-clean` Debug and Release 1/1 pass each (2026-09-19,
~144 s / ~134 s); other platforms were pending CI runs.

### Docs (Doxygen + symbols + spaces paths)

Prerequisite: Doxygen installed (1.17.0 locally, 1.9+ known good).

PowerShell:

```powershell
cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF -DLIBJPL_BUILD_DOCS=ON
cmake --build build/remediation --config Debug --parallel
ctest --test-dir build/remediation -C Debug -R 'jpl_docs|jpl_examples|jpl_licenses' --output-on-failure
```

POSIX equivalent uses `-DCMAKE_BUILD_TYPE=...` single-config with
`-DLIBJPL_BUILD_DOCS=ON`. At this checkpoint, Windows MSVC T23
`build/t23-docs` Debug docs/examples/licenses focused set 4/4 pass
(supersedes the T21 4/4 note); CI docs job re-runs the full
symbol/spaces/license/fixture-cache selection.

### Offline cache

```sh
ctest --test-dir build/ci -C Debug --output-on-failure -R 'jpl_fetch_cache|jpl_fixture_integrity|jpl_fixture_integrity_negative'
```

Exercises the production resolver (`cmake/LibjplTestData.cmake`) against
disposable build-tree artifacts with no network. At this checkpoint:
T23 Windows MSVC `build/t23-clean` Debug+Release in-suite pass; Linux
`jpl_fetch_cache` selection not completed locally (see Linux section).

### Real tier (opt-in only, never default)

Requires explicit `LIBJPL_FETCH_EPHEMERIDES=ON` plus local or download
sources; `JPLEPH`/`JPL_TESTPO` env dirs only redirect per-file sources
inside this tier and never enable it alone. The all-ephemerides sweep
(`jpl_all_ephemerides[_local]`) decodes and validates every physical data
record of every present binary: finite ordered encoded dates, continuity
with the previous end, anchoring at coverage start/end (one trailing
record may start at coverage end staying ordered). This opt-in tier
intentionally performs a full sequential scan and can read several
gigabytes.

```sh
cmake -S . -B build/remediation-real -DLIBJPL_FETCH_EPHEMERIDES=ON
cmake --build build/remediation-real --config Debug --parallel
ctest --test-dir build/remediation-real -C Debug --output-on-failure
```

Runs DE440/DE440t testpo in both configurations, retains summary
counts/hashes as artifacts (CI), and runs the local legacy/long-span
sweep only on a configured maintainer runner. The recorded result
(post-checkpoint, 2026-09-19, `build/verify-real`): all five `*t` suites
pass (DE430t/432t/436t/438t/440t, 13201 lines each) after the TT-TDB
boundary-equality fix (`span_end > width`); `testpo.405` validates 7214
lines after the padded-EOT fix; the full local sweep opens through
DE431/DE441 after the 64-bit sweep-size fix. Supersedes the T23
440t/DE200 failure notes below, which were recorded before those fixes.
Local DE200/DE403 replaced with official NASA files (defective
originals kept as `.defective-backup`); suspect DE404 quarantined as
`DE404.eph.quarantined` (no official source available). Source hashes:
DE440.eph `8d0dbb47...c24a9e5` (102272352 B), DE440t.eph `e2bdf430...f165331ac`
(112720608 B), testpo.440 `45084a7f...16693429f`, testpo.440t
`9801f019...4491c7a3f2ad` (858238 B each). CI `ephemerides.yml` is
`workflow_dispatch` only.
