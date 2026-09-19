# Project review

- Review date: 2026-09-17.
- Reviewed revision: `1327b20` on `main`, release version 0.2.0.
- Scope: handwritten library code, public API contracts, tests, CMake installation, documentation, and relevant ERFA/JPL references.
- Status: findings are open unless a later entry below records a fix and its verification. Saving this report did not fix library code.
- Evidence: "reproduced" means exercised locally during the review; source inspection and external-reference checks are identified separately.
- References and line numbers describe the reviewed revision and may move in later revisions.

## Required correction from the maintainer

### R00. High: Julian dates must use arrays of two doubles

After the initial report, the maintainer clarified that **all Julian dates are supposed to be represented by an array of two doubles** to preserve precision over ephemerides spanning thousands of years. The initial report treated single-double Julian dates as a precision limitation. That classification was insufficient: the current representation violates the required design.

Verified against the current code:

- `include/jpl.h:92-93` stores coverage dates in single doubles.
- `include/jpl.h:190,225,247` accepts single-double dates for TT-TDB interpolation, states, and light-time.
- `include/jpl_frames.h:20,32,40,49,57` uses single-double dates, including scalar return values for time conversion.
- `include/jpl_convenience.h:27,41` accepts single-double observation dates.
- `src/jpl.c:558,579,581,678` performs record indexing, normalization, and emission-time subtraction on single-double dates.
- `src/jpl_frames.c:10,27,30-32` splits an already-rounded scalar into `JPL_DJM0` and `jd - JPL_DJM0` for ERFA, then returns scalar dates. Splitting after rounding cannot recover lost precision.

Near J2000, adjacent binary64 Julian dates are roughly 40 microseconds apart. Spacing grows with the magnitude of the Julian date. This is larger than the light-time convergence threshold of `1e-12` day, or 0.0864 microseconds. That threshold alone therefore does not establish emission-epoch accuracy.

Required follow-up:

- Represent Julian-date inputs, outputs, and in-memory epochs as two-element double arrays with a documented split convention.
- Preserve both parts through time conversion, comparisons, record/subinterval selection, interpolation offsets, and light-time iteration. Do not add the parts into a full scalar Julian date before these operations.
- Pass both parts to ERFA without first collapsing them to a scalar.
- Preserve the standard DE on-disk binary layout. Decode its scalar date fields and promote them to the in-memory two-part representation; do not invent a new ephemeris file format. Raw coefficient-record reads still reflect that file format.
- Distinguish Julian dates from durations: record spans, light-time delays, and TT-TDB corrections are not full epochs and do not automatically require the same representation.
- Test small time increments below one full-JD ULP, distant positive and negative epochs, record/subinterval boundaries, equivalent date splits, and time conversions against independent references.
- Decide the public API migration and any concrete compatibility obligations before implementation. No compatibility wrapper or new public type was selected by this documentation task.

Two doubles do not eliminate all floating-point error. The requirement is to avoid losing the small part of an epoch by adding it to a large Julian date prematurely. This is a cross-cutting correction, not a signature-only change.

## High-priority findings

### R01. Nonfinite dates and record metadata reach undefined integer conversions

References: `src/jpl.c:106-111,179-187,556-581`.

Range checks using ordered comparisons do not reject NaN. The code subsequently converts values to `size_t` for record counts, record indices, or subinterval indices. Converting NaN or an out-of-range floating-point value to an integer is undefined behavior.

This is reachable through public queries and malformed files. Reproduced with MSVC: a NaN query returned `JPL_OK` with NaN coordinates; a record with a NaN start date returned successful NaN results through FILE and mmap backends. These observed outcomes do not make the undefined conversions safe on other builds.

Header record-count conversion has another boundary problem: on 64-bit platforms, converting `SIZE_MAX - 2` to `double` rounds upward, weakening the upper-bound check. The NaN-header-step probe returned a format error on MSVC, but source inspection still identifies an unsafe cast before reliable rejection.

Recommendation: validate finiteness before indexing, require finite positive record/subinterval spans, and bound record counts against physical file geometry before converting to integers. Add malformed-input regressions for both backends and retain these guards during R00.

### R02. The documented SSB center alias 0 fails in jpl_planet_position

References: `src/jpl_convenience.c:32-43`, `include/jpl_convenience.h:14-18`.

The wrapper accepts center `0`, completes light-time evaluation, then requests the observer-to-Sun vector with `jpl_get_state(e, tdb, center, JPL_SUN, sun_obs)`. This uses `0` as a target, although core supports it only as a center alias.

Reproduced against the committed DE441 fixture: Mars with center `0` returns `JPL_ERROR_ARGUMENT`; the equivalent center `12` succeeds.

Recommendation: normalize the alias before using it as a target, or skip the unnecessary aberration calculation for the stationary SSB observer. Test successful calls with both aliases.

### R03. Strict Linux C17 compilation fails on file-offset functions

References: `src/jpl.c:1-26,148-150,319`.

The POSIX implementation uses `fseeko` and `ftello` without requesting their declarations through feature-test macros.

Reproduced with Ubuntu GCC 15.2 using strict C17 syntax checking:

```text
error: implicit declaration of function 'fseeko'
error: implicit declaration of function 'ftello'
```

Older compilers that tolerate implicit declarations can assume the wrong return type for `ftello`, particularly dangerous for large ephemerides. The code also does not explicitly enable 64-bit offsets on 32-bit POSIX systems.

Recommendation: establish POSIX and large-file definitions before system headers, verify actual offset-type bounds, and add strict Linux compilation to automated checks. A full Linux CMake build was not run during this review.

### R04. Legacy ephemerides are silently mislabeled as ICRS

References: `src/jpl_convenience.c:6-8`, `include/jpl_convenience.h:16-18`.

The convenience API assumes every DE file already contains ICRS-equatorial coordinates. [JPL's export documentation](https://ssd.jpl.nasa.gov/planets/eph_export.html) distinguishes DE102, referred to the dynamical equator and equinox of 1950; DE200/DE202, referred to the dynamical equator and equinox of 2000; and modern ICRF releases.

The wrapper neither checks the ephemeris number nor transforms legacy frames. Successful calls can return incorrect "ICRS" directions. For DE102, the orientation difference can be of order 0.7 degrees, depending on direction.

Evidence: source inspection and the JPL reference, not a runtime legacy-file orientation comparison.

Recommendation: restrict the ICRS convenience contract to supported frame families or independently validate the necessary legacy transformations. These legacy frames are equatorial, so applying an ecliptic conversion is not the solution.

### R05. Installation omits required notices; project license is unspecified

References: `CMakeLists.txt:62-75`, `src/erfa/LICENSE:28-36`.

The static archive includes ERFA code. Its license requires copyright, conditions, and disclaimer to accompany binary redistribution. The inspected installation contains only the archive, exported targets, and headers, with no installed license or notice.

The only license file found belongs to ERFA. No explicit license grant was found for libjpl's own code.

Recommendation: establish the project license and install third-party notices before distributing a standalone binary package. This is a release concern, not a runtime defect.

## Medium-priority findings

### R06. Record-layout inference can choose the wrong width or accept a header-only file

Reference: `src/jpl.c:106-125`.

Inference tests divisibility and coefficient extent, but does not establish that the selected layout matches header and data-record boundaries.

Reproduced through both open APIs:

| Input | Actual behavior |
| --- | --- |
| One advertised interval, 2,880-byte records, one supported trailing data record | Chooses 3,840-byte records, reports the wrong record count, and reads constants at the wrong offset |
| A 2,880-byte header with no constants/data records | Opens successfully by treating it as three 960-byte records |

The trailing-record probe read a constant expected to be `11` as `0`.

Recommendation: require enough space for the complete header and applicable extensions, validate candidate layouts against record dates, and reject or resolve ambiguity rather than taking the first divisible candidate.

### R07. FILE clones can read a different file under the original metadata

References: `src/jpl.c:316-317,503-525`.

A FILE clone copies the original header and record count, then reopens the stored path without checking file identity.

Reproduced: open a relative path, change working directory, and clone with another file under the same relative name. Cloning succeeds; the original returns constant `11`, while the clone returns `99`.

Replacement or symlink retargeting poses a related risk even for absolute paths.

Recommendation: preserve or verify file identity when creating independent readers. Resolving relative paths fixes only the directory-change case. POSIX `dup` alone is not sufficient with the existing seek/read implementation because duplicated descriptors share an offset.

### R08. Corrupt numerical inputs produce successful NaN/Inf results

References: `src/jpl.c:184-187,587-603,635-652,675-687`.

Header AU and Earth-Moon mass ratio values are not validated; interpolated results are not checked for finiteness.

Reproduced:

- Zero header AU: opening succeeds, then a state query returns `JPL_OK` with infinity.
- NaN position coefficient: query returns `JPL_OK` with NaN.
- SSB target with a NaN center position: light-time returns `JPL_OK` with NaN delay.

In the last case, `fabs(lt - prev) >= tolerance` is false for NaN, so the final non-convergence check accepts it.

Recommendation: validate physical scaling fields, reject nonfinite interpolation results, and explicitly track successful convergence. Test invalid mass ratios as well as AU, state, and TT-TDB coefficients.

### R09. Byte-order selection precedes complete layout validation

References: `src/jpl.c:324-345,419-423` and the equivalent POSIX mmap path.

Little-endian interpretation is accepted when preliminary header parsing succeeds. If physical layout validation then fails, parsing does not retry big-endian interpretation.

Reproduced: a synthetic big-endian file with dates `[0,1]`, step `1`, two constants, DENUM `102`, and a coefficient triple `{3,1,1}` passes preliminary plausibility under the wrong byte order and is rejected by both open APIs.

Recommendation: evaluate header parsing and complete layout validation together for each byte-order candidate. Select an interpretation only after the complete candidate succeeds.

### R10. Optional TT-TDB padding causes rejection before intended validation

References: `src/jpl.c:256-272,345-353`.

Comments promise unrelated bytes at legacy extension locations will be treated as absent. A positive-looking TT-TDB triple instead increases the required record size before physical layout is established.

Reproduced: `{1000,1,1}` in the optional slot of an otherwise accepted 360-double-record mock causes both open APIs to reject the file. The later out-of-record-means-absent check is never reached.

Recommendation: keep optional pointers tentative until geometry is established, then apply an explicit absent-versus-corrupt policy. Preserve support for genuinely appended TT-TDB datasets.

### R11. Light-time evaluation rejects valid solutions near coverage start

Reference: `src/jpl.c:673-682`.

The initial delay uses reception-time geometric range. For a receding target, this overestimates the converged delay, potentially placing the first trial before coverage even when the true emission epoch is inside it.

Reproduced with coverage starting at `S = 2451545.0`, SSB observer, target `x(t) = 1 + 0.01*(t-S)` AU, and reception JD `2451545.0057756184`. The analytical emission epoch is about 8.64 milliseconds after coverage start. The initial trial is about 20.20 milliseconds before coverage and returns `JPL_ERROR_RANGE`.

Recommendation: recover from out-of-range initial estimates with a bounded solution strategy. Test this independently of cases whose true emission epoch precedes coverage, and preserve the distinction when migrating dates under R00.

### R12. Installed CMake package is incomplete

References: `CMakeLists.txt:25-29,53-75`.

- Reproduced: `find_package(libjpl CONFIG REQUIRED)` fails against the installation because only `libjplTargets.cmake` is installed, not a package config file.
- Source inspection: headers honor `CMAKE_INSTALL_INCLUDEDIR`, while the exported include path is hard-coded to `include`. Custom include destinations break consumers.
- Source inspection: the math-library condition excludes compiler ID `MSVC`, not all MSVC-compatible toolchains. clang-cl identifies as `Clang`, so consumers can be asked to link nonexistent `m.lib`.

Recommendation: install package config/version files, use consistent installation paths, resolve math-library requirements for the build toolchain, and test an installed-package consumer.

### R13. Reference validators contain false-pass paths

References: `tests/test_testpo.c:121-149,171-183,193-210`, `tests/test_all_ephemerides.c:28-34`.

- The testpo runner skips ordinary-body `JPL_ERROR_RANGE` results after already confirming the date is inside coverage.
- TT-TDB reference rows bypass finite-input validation. Reproduced: 100 rows containing NaN expected values were reported as 100 successful comparisons.
- Libration component 3 receives both magnitude-relative scaling and an epoch-dependent tolerance multiplier. The compounded tolerance becomes overly permissive for long-span data; the stated JPL exception does not justify both growing factors.
- The ephemeris sweep treats every `JPL_ERROR_IO` as absent, including existing truncated or unreadable files.

Recommendation: distinguish legitimate omissions from failures, validate all reference inputs before dispatch, and justify the tolerance envelope independently. Skip missing angular datasets only after checking their header pointers.

### R14. Doxygen succeeds but omits two public API reference pages

References: `include/jpl_frames.h:1-6`, `include/jpl_convenience.h:1-7`, `Doxyfile.in`.

Unlike `jpl.h`, the other public headers lack file-level Doxygen documentation. With current extraction settings, their functions are absent from the generated function index and API reference pages.

Reproduced with Doxygen 1.17 and `WARN_AS_ERROR=YES`: only `jpl.h` gets a reference page. The other headers have source listings, not API reference pages.

Recommendation: add file-level documentation and check generated API coverage, not only the tool's exit status.

### R15. Identical target/center queries unnecessarily require Sun coefficients

References: `src/jpl_convenience.c:26-43`, `include/jpl_convenience.h:22`.

The contract promises a successful zero vector for identical bodies. After obtaining zero, the wrapper still looks up the Sun.

Reproduced: an in-range partial ephemeris containing EMB but no Sun returns `JPL_ERROR_RANGE` for EMB relative to itself.

Recommendation: after the underlying query performs necessary date/data validation, return the zero state without unrelated correction lookups.

## Lower-priority findings

| ID | Finding | Impact and references |
| --- | --- | --- |
| L01 | Incorrect speed-of-light constant | `173.144632684` AU/day differs from the value derived from exact light speed and the stated AU, approximately `173.14463267424031`. At 40 AU the delay error is about 1.125 microseconds. Tests repeat the production literal. `src/jpl_private.h:8-11`, `tests/jpl_test_util.h:15-17`. |
| L02 | Inconsistent AU conventions | Core states use the file AU; helpers and light-speed scaling use a fixed modern AU. Legacy-file conversions do not exactly round-trip. `src/jpl.c:652`, `src/jpl_convenience.c:9-14`. |
| L03 | SSB shortcuts bypass date validation | Reproduced SSB-to-SSB outside coverage returning `JPL_OK`, contrary to the range contract. `src/jpl.c:622-652`. |
| L04 | Misleading TT/TDB accuracy documentation | README says TT and TDB differ by about 70 seconds, confusing their millisecond-scale separation with TT-UTC. The claimed 2 ms model floor confuses correction amplitude and model accuracy. ERFA describes better than +/-3 ns model accuracy over 1950-2050 relative to DE405. Single-double JD quantization is a separate defect, now R00. `README.md:47`, `include/jpl_frames.h:45-55`, `src/erfa/dtdb.c:127-134`. |
| L05 | Inconsistent light-time target-zero contract | Parameter documentation says targets 1-13, another note says 0-13, and implementation ultimately rejects target 0. `include/jpl.h:232-242`. |
| L06 | Fetch-tier reconfiguration repeats binary downloads | Existing destination binaries are not checked before hashless downloads run again. Reconfiguration can repeat about 210 MB of transfer and still needs network access. `CMakeLists.txt:147-182`. |

## Coverage gaps

- TT-TDB unit tests check only finiteness on successful reads. Returning zero can pass. The first mock can assert `0.001` seconds and `0.00004` seconds/day. Sample distinct subintervals/endpoints too. `tests/test_tt_tdb.c:59-68,97-109`.
- The mmap shareability regression uses sequential reads, not concurrent reads. Clone tests close clones before originals, leaving independent lifetime behavior untested. `tests/test_mmap.c:23-66`.
- Moving-target light-time expectations use the returned delay. This checks observer epoch selection, not independently that the delay solves the equation. Add an analytic solution or independent residual. `tests/test_lighttime.c:87-96`.
- Time conversion only has a round-trip check. Identity implementations or coordinated sign errors can pass. Add independently sourced absolute values and precision tests for R00. `tests/test_frames.c:75-78`.
- Fixture row count and first/last dates do not establish byte-verbatim provenance or preserve interior body/component coverage. The manifest lacks fixture digests. `tests/test_fixture.c:218-231`, `tests/data/MANIFEST.txt:2-4`.
- No tracked CI workflow or installed-consumer test was found. Windows tests do not establish strict POSIX compilation or package usability.
- The committed real-data fixture checks geometric states, not independent apparent-position or convenience-frame accuracy. Add references with matching correction conventions.

## Intentional contract boundaries

- Light-time velocity is effectively `v_target(emission) - v_center(reception)`, not the derivative of returned apparent position. Document that expression explicitly; the existing retarded-epoch wording is imprecise.
- Frame helpers rotate velocity with `R*v` and intentionally omit `dR/dt*position`. The public header discloses this fixed-epoch convention.
- Apparent position here means light-time plus stellar aberration, not full observed-place calculation with gravitational light deflection, surface-observer motion, Earth orientation, or refraction.
- Long DE coverage does not itself validate frame and time-conversion model accuracy over the entire interval.
- Single-double Julian dates are NOT an accepted contract boundary. The maintainer explicitly requires two-double arrays; see R00.

## Verification performed

| Check | Result |
| --- | --- |
| Clean MSVC Debug build | Passed; no warnings observed |
| Default Debug CTest suites | 8/8 passed |
| Clean MSVC Release build | Passed; no warnings observed |
| Default Release CTest suites | 8/8 passed |
| Doxygen 1.17, warnings as errors | Passed, but API omissions confirmed |
| Installation | Completed; package contents inspected |
| Installed find_package consumer | Failed as described in R12 |
| Strict Ubuntu GCC 15.2 C17 syntax check | Failed on fseeko/ftello declarations |
| Targeted malformed-input/API probes | Confirmed the cases labeled reproduced above |

Principal commands, run from the repository root unless noted:

```powershell
cmake -S . -B build/review -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/review --config Debug --parallel
ctest --test-dir build/review -C Debug --output-on-failure
cmake --build build/review --config Release --parallel
ctest --test-dir build/review -C Release --output-on-failure -j 8
cmake -S . -B build/review -DLIBJPL_FETCH_EPHEMERIDES=OFF -DLIBJPL_BUILD_DOCS=ON
cmake --build build/review --config Debug --target docs
cmake --install build/review --config Release --prefix D:/src/libjpl/build/review/install
wsl -d Ubuntu --exec gcc -std=c17 -Wall -Wextra -Wpedantic -I/mnt/d/src/libjpl/include -fsyntax-only /mnt/d/src/libjpl/src/jpl.c
```

Disposable probe sources and the installed-consumer project were created under ignored `build/review/probes/` and `build/review/consumer/`. They are local evidence, not durable regression tests or guaranteed contents of a future checkout. The trigger descriptions in this report must be turned into tracked tests when fixes are implemented.

The testpo false-pass probe used a mock with a present TT-TDB dataset and 100 identical rows shaped as follows:

```text
441 2000.01.01 2451545.5 17 0 1 nan
```

The runner printed `validated 100 of 100 parsed lines` and succeeded.

The committed DE441 fixture was exercised. The optional large-download integration tier, full Linux/macOS builds, sanitizers, and concurrency stress tests were not run. Vendored ERFA was treated as upstream code, not exhaustively re-audited. Verification results above belong to the reviewed revision, not to future changes or the proposed two-part-date migration.

## What to preserve

No definite defect was found in the normal-range Chebyshev recurrence and derivative scaling, Earth/EMB/Moon formulas, or ERFA frame rotation direction. The tests remain active under `NDEBUG`; they do not disappear in Release. These checks should survive the fixes and date migration.

## Suggested order

1. Design the required two-part Julian-date migration R00, including arithmetic, public contracts, binary-format boundaries, and precision tests. It affects all date-handling fixes.
2. Fix unsafe nonfinite handling, center-0 behavior, strict POSIX compilation, and ambiguous record geometry with tracked regressions.
3. Resolve legacy-frame contracts, clone identity, and distribution licensing.
4. Tighten reference validators, implement the remaining correctness fixes, and extend platform/concurrency coverage.
5. Repair package consumption, documentation generation, and accuracy explanations.

The modern-DE happy path has useful validation, but a green default suite overstates robustness. The date representation, parser validation, API edge cases, portability, and installation need attention before treating this as a dependable cross-platform release.

## Resolution log

T23 acceptance (2026-09-19, uncommitted T00-T22 tree; BASE
`4471c673295dc2a7a35e8458f5ee6b2f2d18ae3b`): fresh `build/t23-clean`
offline Debug 36/36 and Release 36/36 pass; `build/t23-docs` focused
docs/examples/licenses 4/4 pass; Linux strict GCC 15.2 `build/t23-wsl`
25/25 subset pass plus threads/mmap/largefile binaries pass; real tier
`build/remediation-real` Debug+Release `jpl_testpo_440`,
`jpl_real_ephemeris`, `jpl_testpo_441` pass. Per-task evidence and
remaining blockers live in `docs/reviews/2026-09-17-remediation-progress.md`;
per-configuration evidence in `docs/validation.md`. Entries below mark
an ID verified only where T23 reproduced a pass; otherwise the ID stays
open/blocked with the reason stated. No finding is closed by plan
existence.

### R00 - Open (blocked on platform/CI gates; core evidence passes)

Fix: uncommitted (T02/T03/T07/T16/T21 tree). Tests: `jpl_dates`,
`jpl_testpo_parser`, `jpl_numeric` (all pass `build/t23-clean`
Debug+Release 36/36). Evidence: public decls all `[2]` pairs;
`src/erfa` untouched; `jpl_testpo_440` passes real tier Debug+Release.
Remaining: macOS/32-bit/sanitizer/CI runs pending; full Linux suite
incomplete locally (timeouts) - see progress log.

### R01a - Open (platform/CI gates pending; unit evidence passes)

Fix: uncommitted (T04/T07/T09 tree). Tests: `jpl_tests`,
`jpl_api_errors` (pass Debug+Release). Remaining: sanitizer/CI gates.

### R01b - Open (platform/CI gates pending; unit evidence passes)

Fix: uncommitted (T04/T05/T09 tree). Tests: `jpl_tests`,
`jpl_api_errors`, `jpl_parser` (pass Debug+Release). Remaining:
sanitizer/CI gates.

### R01c - Open (32-bit gate pending; strict evidence passes)

Fix: uncommitted (T01/T04/T05 tree). Tests: `jpl_largefile` binary
`largefile tests passed` (Windows + Linux; oversize skipped on 64-bit),
strict `-Werror=implicit-function-declaration` Linux configure/build
OK. Remaining: 32-bit multilib run + CI.

### R02 - Open (legacy-sweep blocked; mock/nominal evidence passes)

Fix: uncommitted (T12/T16 tree). Tests: `jpl_convenience`,
`jpl_numeric`, `jpl_testpo_440` (pass). Remaining: equivalent
centers-0/12 check on real DE441 data is covered by the pass, but the
local legacy sweep (`jpl_all_ephemerides_local`) fails on DE200 open -
see R05-blocker note below.

### R03a - Open (CI gate pending; local strict evidence passes)

Fix: uncommitted (T01/T22 tree). Evidence: Linux strict configure/build
OK with `-Werror=implicit-function-declaration` (GCC 15.2). Remaining:
Ubuntu GCC/Clang CI runs.

### R03b - Open (blocked: 32-bit gate not run)

Fix: uncommitted (T01/T22 tree). Evidence: `jpl_largefile` 64-bit
behavior verified; 32-bit addressability gate not run locally (no
multilib) and no CI run yet.

### R04 - Open (CI/docs gates pending; contract evidence passes)

Fix: uncommitted (T12/T16/T21 tree). Tests: `jpl_convenience`,
`jpl_numeric` pass; frame-source table in `docs/validation.md`.
Remaining: macOS/CI confirmation; legacy-file sweep blocked by DE200
open failure.

### R05a - Open (CI gate pending; license evidence passes)

Fix: uncommitted (T18/T21/T22 tree: `LICENSE`, `LICENSES.md`,
inventory). Tests: `jpl_licenses` pass Windows Debug+Release.
Remaining: Linux license run hung locally; CI required.

### R05b - Open (non-Windows consumer gates pending)

Fix: uncommitted (T18/T19 tree). Tests: `jpl_install` pass Windows
Debug (~144 s) + Release (~134 s); audit: exactly 3 headers,
config/version/targets present, no `m.lib`, no checkout-path leaks,
no `jpl_private` in consumer. `src/erfa` unchanged. Remaining:
clang-cl/macOS/32-bit consumers via CI.

### R06a - Open (unit evidence passes; sweep blocked)

Fix: uncommitted (T05 tree). Tests: `jpl_parser` pass Debug+Release
(header-only/trailing-width regressions). Remaining: full legacy sweep
blocked by DE200 open failure.

### R06b - Open (unit evidence passes; sweep blocked)

Fix: uncommitted (T05 tree). Tests: `jpl_parser` trailing-record case
passes. Remaining: same DE200 blocker.

### R07 - Open (TSan/CI gates pending; functional evidence passes)

Fix: uncommitted (T13/T14 tree). Tests: `jpl_mmap`, `jpl_threads` pass
Windows Debug+Release; Linux binaries print `mmap tests passed` /
`threads tests passed`. Remaining: TSan + CI.

### R08a - Open (unit evidence passes; sanitizer/CI pending)

Fix: uncommitted (T04 tree). Tests: `jpl_tests`, `jpl_api_errors`
pass. Remaining: sanitizer/CI gates.

### R08b - Open (unit evidence passes; sanitizer/CI pending)

Fix: uncommitted (T09/T11/T16 tree). Tests: `jpl_api_errors`,
`jpl_numeric` pass. Remaining: sanitizer/CI gates.

### R08c - Open (unit evidence passes; real-tier TT-TDB blocked)

Fix: uncommitted (T10 tree). Tests: `jpl_lighttime` passes; validator
negatives pass. Remaining: `jpl_testpo_440t` fails (see R10-blocker).

### R09 - Open (blocked: legacy big-endian DE200 fails to open)

Fix: uncommitted (T05 tree). Evidence: little-endian DE202 opens OK
(probe denum 202); big-endian DE200 repack
(`D:\ephem\jpl\bin\DE200.eph`, 42965216 B, BE header denum 200)
fails `jpl_open` with `JPL_ERROR_FORMAT` on both backends: the
trailing-record hypothesis reads last/trailing `[2513360.5,
2513392.5]` against scheduled trail `2513392.5` (32-day mismatch) and
the plain extra=2 hypothesis fails divisibility (`42965216 % 6501 =
107`). Full byte-order/layout diagnosis is future work; T23 attempted
no fix. `jpl_parser` synthetic BE regression still passes.

### R10 - Open (blocked: DE440t TT-TDB reports absent)

Fix: uncommitted (T06 tree; T10 runtime guard). Evidence: DE440t opens
OK (denum 440, `record_double_count` 1122, tail triple `(1019,13,8)`);
`jpl_get_tt_minus_tdb` at `{2451545.0, 0.5}` returns
`JPL_ERROR_UNSUPPORTED`, so `jpl_testpo_440t` fails at line 13 in both
configurations. Root cause: span end `(1019-1)+13*8 = 1122` equals the
record width and is rejected by the boundary-equality guard
(`span_end >= record_double_count`, `src/jpl.c:1801`) plus the matching
accept-then-absent comment at `src/jpl.c:590-601`. Whether the last
slot genuinely holds no coefficient bytes or the guard is off by one
is undecided; T23 attempted no fix.

### R11 - Open (unit evidence passes; real-tier blocked by R10)

Fix: uncommitted (T10/T16 tree). Tests: `jpl_lighttime`,
`jpl_numeric` pass. Remaining: emission-epoch coverage behavior on
real `*t` data cannot close while TT-TDB reads fail (see R10).

### R12a - Open (Windows evidence passes; other toolchains via CI)

Fix: uncommitted (T19 tree). Tests: `jpl_install` Windows Debug+Release
pass (exact-version + mismatch cases inside). Remaining: other
platform/toolchain runs.

### R12b - Open (Windows evidence passes; POSIX relocation via CI)

Fix: uncommitted (T19 tree). Tests: `jpl_install` covers custom
destinations + spaces prefix on Windows. Remaining: POSIX runs.

### R12c - Open (blocked: no local clang-cl; CI required)

Fix: uncommitted (T19/T22 tree). Evidence: VS-generator exports carry
no `m.lib` (checked inside `jpl_install`, passing). Remaining:
clang-cl + Linux/macOS consumer runs.

### R13a-e - Open (synthetic-validator evidence passes; real T#17 blocked)

Fix: uncommitted (T15/T03/T22 tree). Tests: all 15 `jpl_validator_*`
plus `jpl_fixture_read_error` pass Debug+Release; `jpl_testpo_parser`
passes. The `jpl_testpo_440t` failure is a library dataset-absent
result (`JPL_ERROR_UNSUPPORTED`), not a runner false-pass. Remaining:
real T#17 validation blocked by R10; CI sweep confirmation pending.

### R14a/b - Open (Windows docs evidence passes; POSIX/CI pending)

Fix: uncommitted (T21/T22 tree). Tests: `jpl_docs_symbols`,
`jpl_docs_paths` pass in `build/t23-docs` (4/4 with examples/licenses).
Remaining: POSIX docs run + CI docs job.

### R15 - Open (unit evidence passes; sweep blocked by DE200)

Fix: uncommitted (T12 tree). Tests: `jpl_convenience` passes.
Remaining: identical-body coverage on the legacy sweep blocked by the
R09 DE200 open failure.

### L01 - Verified in unit scope (CI gates still pending)

Fix: uncommitted (T08/T10/T11 tree). Evidence: `JPL_AU_KM`,
`JPL_C_KM_PER_SECOND`, derived `JPL_C_AU_PER_DAY` in
`src/jpl_private.h`; `jpl_lighttime`, `jpl_aberration`,
`jpl_numeric` pass Debug+Release. No live stale literal (remaining
`173.144632684` hits are historical plan docs + this log's quotation
of the old value).

### L02 - Open (mock/nominal evidence passes; real-tier blocked)

Fix: uncommitted (T08/T15/T16 tree). Tests: `jpl_numeric`,
`jpl_testpo_440` pass. Remaining: nonmodern-AU conversion on the
blocked files (R09/R10).

### L03 - Open (unit evidence passes; CI/sweep pending)

Fix: uncommitted (T07/T09 tree). Tests: `jpl_api_errors`,
`jpl_numeric` pass. Remaining: CI/sweep confirmation.

### L04 - Verified in docs scope (CI gate pending)

Fix: uncommitted (T07/T16/T21 tree). Evidence: `docs/validation.md`
scientific contracts state millisecond-scale TT-TDB, +/-3 ns ERFA note,
tolerances-as-tolerances; no live 70 s / 2 ms-floor claims (hits only
in labeled historical quotations). `jpl_numeric` passes.

### L05 - Verified in unit scope (sweep blocked by DE200)

Fix: uncommitted (T07/T12/T21 tree). Tests: `jpl_convenience`,
`jpl_api_errors` pass. Remaining: legacy sweep blocked (R09).

### L06 - Verified in unit scope (network-download path via CI only)

Fix: uncommitted (T20 tree). Tests: `jpl_fetch_cache` pass
Debug+Release (offline resolver). No re-download observed (local
per-file sources used; network downloads not exercised locally).

### G01/G02/G05/G06/G11/G12/G13 - Open (unit evidence passes; blocked files/CI pending)

Fix: uncommitted (T16/T02/T07/T10/T11/T12 tree). Tests: `jpl_numeric`,
`jpl_tt_tdb`, `jpl_lighttime` pass Debug+Release. Remaining: full
numerical/matrix closure needs R09/R10 files + platform/CI gates.

### G03/G04 - Open (functional evidence passes; TSan/CI pending)

Fix: uncommitted (T13/T14/T22 tree). Tests: `jpl_threads`,
`jpl_mmap` pass (Windows + Linux binaries). Remaining: TSan + CI.

### G07/G08 - Verified in unit scope (CI gates pending)

Fix: uncommitted (T17/T21 tree). Tests: `jpl_fixture`,
`jpl_fixture_integrity`, `jpl_fixture_integrity_negative` pass
Debug+Release.

### G09/G10 - Open (blocked: CI + unrun local gates)

Fix: uncommitted (T22/T19 tree). Evidence: Windows MSVC Debug+Release
full pass; Linux strict subset pass; workflows present. Missing:
clang-cl, macOS, 32-bit, sanitizers, Linux install/licenses/fetch,
full Linux suite, and actual CI runs. A skipped tier is never a pass.

### B01-B06 - Open (unit/docs evidence passes; blocked files/CI pending)

Fix: uncommitted (owning trees per map). Tests: `jpl_numeric`,
`jpl_lighttime`, `jpl_aberration`, `jpl_convenience` pass
Debug+Release; contracts in `docs/validation.md`. Remaining: R09/R10
files + platform/CI gates.

### S01-S03 - Verified in docs scope (POSIX/CI confirmation pending)

Fix: uncommitted (T21/T22 tree). Evidence: live README/validation
wording checked (fixture vs operational data, tier names, spaces
paths); `jpl_docs_paths`, `jpl_examples`, `jpl_licenses` pass.
Remaining: POSIX docs/CI confirmation.

### S04 - Verified in unit scope (CI gate pending)

Fix: uncommitted (T11/T16 tree). Tests: `jpl_aberration`,
`jpl_numeric` pass.

### S05-S07 - Verified in unit scope (CI gate pending)

Fix: uncommitted (T03/T15 tree). Tests: `jpl_testpo_parser`, all
`jpl_validator_*` pass Debug+Release.

## Prior log

No findings have been resolved by this report. Future sessions should append the finding ID, fix revision, regression test, and fresh verification result here. Do not mark an issue resolved solely because an implementation plan exists.
