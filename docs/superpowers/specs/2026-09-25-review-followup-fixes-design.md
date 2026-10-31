<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Review follow-up fixes: design

- Date: 2026-09-25.
- Source: independent fresh review of `main` at `d6af6d9` (v0.3.0, post
  two-part-date migration and full remediation of the 2026-09-17 review).
  Findings were produced by four parallel domain reviews (public
  headers/contracts, `src/jpl.c` core, frames/convenience layer, tests +
  build/CI) plus independent spot-verification of a sample of each
  review's claims against current code, real DE200/DE440t data, and
  fixture/manifest hashes.
- Scope: seven findings, none of which were present in the prior
  `docs/reviews/` rounds. No regression was found in any previously
  "resolved" R00-R15/L01-L06/F01-F18 item.
- Status: design approved by the maintainer; this document records the
  agreed approach for each finding before implementation. Saving this
  document fixes no code.

## Findings covered

| ID | Severity | Area | One-line summary |
| --- | --- | --- | --- |
| F1 | Medium/High | Build | `LIBJPL_TESTING` test hooks compile into the installed `jpl` library by default |
| F2 | Medium | Tests/CI | Undocumented hard Python 3 dependency in two default offline tests |
| F3 | Medium | Docs | `jpl_moon_position` Doxygen note misdescribes its own (correct) single-leg algorithm as an approximate multi-leg addition |
| F4 | Low | Core | Stale comment claims a flush TT-TDB span boundary is treated as absent; it isn't |
| F5 | Low | Core | Two internal `size_t` casts rely on caller discipline instead of their own upper-bound guard |
| F6 | Low | Tests | Two hash checkers canonicalize CRLF text with different, inconsistently-safe techniques |
| F7 | Informational | Convenience API | Unreachable `JPL_ERROR_NUMERIC` branch in `jpl_km_to_au` |

F2 and F6 share one root cause (no byte-safe canonicalization primitive without Python) and are fixed together with one new tool.

## F1: `LIBJPL_TESTING` leaks into the installed library

### Problem

`CMakeLists.txt:166` calls `target_compile_definitions(jpl PRIVATE LIBJPL_TESTING)` on the one `jpl` target that gets installed (`install(TARGETS jpl ...)` at line 104), the instant `LIBJPL_BUILD_TESTS=ON` (the default) configures any test needing the hook. This compiles a real linkable symbol (`jpl_test_set_parser_read_limit()`) and a process-wide mutable global (`jpl_parser_read_limit`, shared across every handle and thread) into the shipped static library, plus `jpl_test_lighttime_with_budget()`. This widens the documented thread-safety contract to a hidden, unaudited, non-thread-safe global affecting all handles, not just test-controlled ones, and ships an undocumented entry point from a "small, dependency-free" library that a plain README-quickstart build followed by `cmake --install` would distribute.

Exactly five test executables call these hooks (verified by grep): `jpl_lighttime_tests`, `jpl_api_errors_tests`, `jpl_largefile`, `jpl_numeric_tests`, `jpl_parser_tests`. No other target needs `LIBJPL_TESTING`.

### Design

Add a second CMake target, `jpl_testing`, built only when `LIBJPL_BUILD_TESTS=ON`:

```cmake
add_library(jpl_testing STATIC ${JPL_CORE_SOURCES} ${ERFA_SOURCES})
```

where `JPL_CORE_SOURCES` is a new variable capturing `src/jpl.c`, `src/jpl_frames.c`, `src/jpl_convenience.c`, `src/jpl_date.c` (the same list currently fed to `jpl` via individual `target_sources` calls) so both targets compile from one authoritative source list. `jpl_testing` gets:

- the same `target_include_directories` (public include dir, private ERFA include dir),
- the same `c_std_17` feature requirement and `C_EXTENSIONS OFF`,
- the same PIC setting (respecting `LIBJPL_PIC`),
- the same ERFA version compile definitions,
- the same MSVC (`/W4 /permissive-` plus `_CRT_SECURE_NO_WARNINGS`) / GCC-Clang (`-Wall -Wextra -Wpedantic`) warning flags,
- the same math-library linkage (`JPL_NEED_M_LIB`),
- **and** `target_compile_definitions(jpl_testing PRIVATE LIBJPL_TESTING)`.

`jpl_testing` is never aliased, never added to `install(TARGETS ...)` or `install(EXPORT ...)`, and never referenced outside the `if(LIBJPL_BUILD_TESTS)` block.

Remove `target_compile_definitions(jpl PRIVATE LIBJPL_TESTING)` (current line 166) entirely — the installed `jpl` target never defines `LIBJPL_TESTING` again.

Change `target_link_libraries(... PRIVATE jpl)` to `target_link_libraries(... PRIVATE jpl_testing)` for exactly the five executables listed above. Their existing per-executable `target_compile_definitions(<exe> PRIVATE LIBJPL_TESTING)` lines stay unchanged — the test `.c` files themselves `#include "jpl_private.h"` and need `LIBJPL_TESTING` defined for their own translation unit to see the guarded declarations. All other test executables keep linking the real `jpl` target unchanged, so they continue to exercise the exact artifact that ships.

### Regression guard

Extend `tests/cmake/test_install.cmake` (the `jpl_install` suite, which already inspects the installed package for path/notice leaks) with a new check: after install, inspect the installed static library for any exported/defined symbol starting with `jpl_test_` and fail if found. Platform-appropriate symbol listing:
- Windows/MSVC: `dumpbin /symbols` (already an MSVC toolchain requirement) or a linker-map based approach — confirm exact invocation during implementation since `jpl_install` already builds and installs a real consumer per platform.
- POSIX (Linux/macOS/clang-cl via Ninja+MSVC tools if `nm`-equivalent is unavailable, prefer the same `dumpbin`-style tool already used): `nm` on the installed `.a`.

This closes the gap the current `jpl_install` suite has (it checks paths/notices, never symbols) and prevents silent reintroduction.

### Build-time cost

Test builds now compile `src/jpl.c` + ERFA + frames/convenience/date sources twice (once for `jpl`, once for `jpl_testing`) whenever `LIBJPL_BUILD_TESTS=ON`. This only affects developer/CI build time, never a production consumer who builds with `LIBJPL_BUILD_TESTS=OFF` or who only builds the `jpl` target. Acceptable trade-off for correctness; no attempt to avoid the duplicate compilation (e.g. via `OBJECT` libraries) because `OBJECT` libraries compile their sources once with one fixed set of compile definitions, which cannot serve two targets that need `LIBJPL_TESTING` on and off respectively.

## F2 + F6: Python dependency and inconsistent canonicalization

### Problem

Three test-support scripts need byte-safe CRLF-to-LF canonicalization or file mutation and currently do it two different ways:

- `tests/cmake/check_numerical_reference.cmake` requires Python 3 (`find_package(Python3 COMPONENTS Interpreter)`, hard `FATAL_ERROR` if absent) to run an inline `python -c` snippet that canonicalizes and rejects bare CR, because "CMake has no binary-safe read primitive in 3.20."
- `tests/cmake/test_fixture_integrity.cmake` requires Python 3 (`find_program(... python3 python)`, hard `FATAL_ERROR` if absent) to run `tests/cmake/mutate_fixture.py`, which has five subcommands: `flip`, `dup`, `retarget`, `recoord`, `crlf`.
- `tests/cmake/check_fixture.cmake` (used by the *default*, always-registered `jpl_fixture_integrity` test, and reused inside the negative-test driver above) instead uses plain CMake `file(READ)` + `string(REPLACE "\r\n" "\n" ...)` — the same technique the numerical-reference checker's own comment calls unsafe ("`file(READ)` alone decodes newlines per-platform").

Both `jpl_numerical_reference_integrity` and `jpl_fixture_integrity_negative` are registered unconditionally whenever `LIBJPL_BUILD_TESTS=ON` (the default). `AGENTS.md` and `docs/validation.md` list only CMake 3.20+, a C17 compiler, and Doxygen (docs job only) as prerequisites, and explicitly claim the default tier needs no network access — Python is undocumented and, on a minimal machine matching the stated prerequisites exactly, two default tests hard-fail. CI happens to mask this because hosted runners ship Python.

### Design

Add one new pure-C command-line tool, `tools/fixture_tool.c`, built as executable target `jpl_fixture_tool` whenever `LIBJPL_BUILD_TESTS=ON` (same pattern as the existing `jpl_validator_data` test-data generator: a plain C17 executable, no library link needed beyond the C runtime, always built so the default suite has zero optional dependencies).

Subcommands (argv[1] dispatch), all operating in binary mode with explicit byte-level I/O (no reliance on any CRT text-mode newline translation):

1. `canon-copy <src> <dst>` — read `src` as raw bytes, replace every `\r\n` with `\n`, reject (nonzero exit, message to stderr) if any bare `\r` remains, write the result to `dst`. Exit 0 on success. This is the single canonicalization primitive used everywhere CRLF-normalization is needed.
2. `flip <path>` — flip one bit of the fixed byte offset currently hardcoded in `mutate_fixture.py`'s `flip` case (offset 18855, documented in `test_fixture_integrity.cmake`'s comment), in place.
3. `dup <text_in> <text_out> <rows_in> <rows_out>` — reproduce the current duplicate-119-interior-rows mutation (endpoints and total row count preserved) on both the text and the paired identity-list files.
4. `retarget <text_in> <text_out> <rows_in> <rows_out>` — reproduce the current single target/component alteration (row 61: T1 C9 -> T2 C9) on both files.
5. `recoord <text_in> <text_out>` — reproduce the current single-coordinate alteration (row 90, fractional digit 6 -> 7).
6. `crlf <text_in> <text_out> <rows_in> <rows_out>` — reproduce the current LF-to-CRLF re-encoding on both files (the positive-control mutation: canonicalization must still make this pass).

Exact byte offsets, row indices, and mutation semantics are taken verbatim from the current `tests/cmake/mutate_fixture.py` (read in full during implementation) so behavior is preserved exactly — this is a mechanical re-implementation in C, not a redesign of what gets mutated.

Wiring changes:

- `check_numerical_reference.cmake`: delete the `find_package(Python3 ...)` block and the inline `python -c` command; replace `jpl_canonical_sha256()`'s canonicalization step with `execute_process(COMMAND "${FIXTURE_TOOL}" canon-copy "${src_path}" "${work_path}")`, checking the result the same way the current Python call is checked. Add a new required `FIXTURE_TOOL` variable, passed from `CMakeLists.txt` as `-DFIXTURE_TOOL=$<TARGET_FILE:jpl_fixture_tool>`.
- `check_fixture.cmake`: replace the `file(READ)` + `string(REPLACE)` + `string(SHA256)` steps for the text and rows fixtures with the same `canon-copy` call into a build-tree work file, then `file(SHA256)` on that file (matching the pattern already used for the numerical-reference checker). This requires `check_fixture.cmake` to also accept a `FIXTURE_TOOL` and a `WORK` directory variable; update both call sites (`jpl_fixture_integrity`'s `add_test`, and the calls inside `test_fixture_integrity.cmake`) to pass them. The binary fixture keeps `file(SHA256)` directly with no canonicalization (it was never canonicalized).
- `test_fixture_integrity.cmake`: delete the `PYTHON`/`find_program` discovery block entirely; replace every `"${PYTHON}" ".../mutate_fixture.py" <sub> ...` call with `"${FIXTURE_TOOL}" <sub> ...` using the same argument order. `CMakeLists.txt`'s `add_test(NAME jpl_fixture_integrity_negative ...)` gains a `-DFIXTURE_TOOL=$<TARGET_FILE:jpl_fixture_tool>` argument; the existing `-DCHECKER=...check_fixture.cmake` argument stays (now implicitly needing `FIXTURE_TOOL`/`WORK` too, forwarded the same way).
- Delete `tests/cmake/mutate_fixture.py` once the C tool is verified to reproduce identical outputs for all five subcommands against the current fixtures (byte-for-byte comparison during implementation, not just "tests still pass" — a subtly different mutation that happens to still trip the same hash mismatch would hide a porting bug).

### Outcome

Zero Python dependency anywhere in the default (`LIBJPL_FETCH_EPHEMERIDES=OFF`) test suite. No documentation update needed (nothing new to document once the dependency is gone) — this is preferred over documenting Python as a prerequisite, matching the project's dependency-free framing. `check_fixture.cmake` and `check_numerical_reference.cmake` now use the identical, byte-safe canonicalization technique, closing F6.

## F3: `jpl_moon_position` Doxygen note

### Problem

`include/jpl_convenience.h:79-85`:

```c
* @note Moon+Earth vector addition mixes legs with different emission
*       epochs and aberration corrections; it is approximate (km-level for
*       moving centers). Adding apparent legs is an approximate
*       convenience, not a consistent single-epoch apparent place: ...
```

`jpl_moon_position` (`src/jpl_convenience.c:117-119`) is a single delegation to `jpl_planet_position(MOON, EARTH)` — one light-time leg, one aberration call, structurally identical to every other body's call. It does not add two separately-computed apparent legs internally. The note, read as a description of the function's own behavior, is wrong; traced through git history it originated as advice for a *user* manually combining this function's output with another call's output (e.g. Moon-wrt-Sun via `jpl_moon_position()` + `jpl_planet_position()` summed by hand) and was compressed into its current, misleading form during header consolidation.

### Design

Replace the note with wording scoped to what's actually true of `jpl_moon_position` itself: it uses the same single-leg light-time-plus-aberration methodology as `jpl_planet_position`, and `state[3..5]` are retarded-epoch, unaberrated velocities (this half of the existing note is accurate and stays). Add a separate, explicitly-scoped note *for users who manually combine this call's output with another apparent-place call* (e.g. to build a third-body apparent vector), stating that summing two independently-corrected legs mixes epochs/aberration terms and is only an approximation — worded as user guidance, not as a description of `jpl_moon_position`'s internals.

## F4: Stale TT-TDB boundary comment

### Problem

`src/jpl.c` (~605-616) states that an accepted TT-TDB triple whose span ends exactly at the record width "still evaluates as absent downstream (no coefficient bytes to interpolate)." Current `interpolate()` bounds check (`start + coefficients_per_component > record_double_count`) accepts an exactly-flush span; the last subinterval's coefficients are read and interpolated normally, confirmed by the passing real-data DE440t testpo suite (`record_double_count=1122`, tail triple span end `1019-1+13*8=1122`, exactly flush, and it interpolates successfully).

### Design

Correct the comment to state that `end == record_double_count` is a valid, interior-consistent span — the final coefficient occupies the last slot — and is not treated as absent. No functional change; comment-only fix.

## F5: Defensive cast guards in `select_record`/`interpolate`

### Problem

`select_record` (`src/jpl.c` ~1235: `candidate = (size_t)quotient_d;`) and `interpolate` (~1434: `subinterval = (size_t)qd;`) cast a floating-point quotient to `size_t` after confirming finiteness and a lower bound, but before an explicit upper-bound check — the cast's safety currently depends entirely on the single caller (in each case) pre-restricting the query date to the coverage interval. Both functions are `static` with exactly one call site today, so this isn't currently reachable as a live bug, but it is the same defect class as the historical R01 (undefined behavior converting an out-of-range double to an integer type), and any future refactor that adds a second call site without the same precondition would silently reintroduce it.

### Design

Add an explicit upper-bound check immediately before each cast: if `quotient_d` (or `qd`) exceeds the function's own known valid upper bound (`covered_record_count` for `select_record`, the subinterval count `nsub` for `interpolate`), return the function's existing error path (`JPL_ERROR_RANGE`/`JPL_ERROR_NUMERIC`, matching whatever the function already returns for out-of-range input) instead of proceeding to the cast. This makes each function's own safety self-contained rather than caller-dependent, with no behavior change for any currently-valid input (the bound is never actually exceeded today, per F5's own analysis) — a regression test will exercise the new guard directly by calling the `static` function through whatever existing test seam is available (likely via the `LIBJPL_TESTING`-gated internal-test pattern already used elsewhere in this file, e.g. `jpl_test_lighttime_with_budget`), rather than only via already-bounded public API calls.

## F7: Remove unreachable branch in `jpl_km_to_au`

### Problem

`src/jpl_convenience.c:29-37`:

```c
jpl_status jpl_km_to_au(double km, double *au) {
    double tmp;
    if (!au) return JPL_ERROR_ARGUMENT;
    if (!isfinite(km)) return JPL_ERROR_ARGUMENT;
    tmp = km / JPL_AU_KM;
    if (!isfinite(tmp)) return JPL_ERROR_NUMERIC;
    *au = tmp;
    return JPL_OK;
}
```

Given `JPL_AU_KM ≈ 1.49597870700e8` and `km` already confirmed finite (so `|km| <= DBL_MAX`), `km / JPL_AU_KM` can never produce a nonfinite result — dividing by a value around `1.5e8` only *shrinks* magnitude relative to the largest finite double, it cannot overflow to infinity, and division of two finite non-NaN values can't produce NaN except `0/0`, which isn't reachable here (`JPL_AU_KM` is a nonzero compile-time constant). No test exercises the branch, consistent with it being unreachable given current constants. Per maintainer decision, remove rather than keep with an explanatory comment.

### Design

- Remove the `isfinite(tmp)` check and the `JPL_ERROR_NUMERIC` return from `jpl_km_to_au`; assign `*au` directly after computing the finite quotient.
- Update `include/jpl_convenience.h:96-104`'s `@return` documentation for `jpl_km_to_au` to drop the `JPL_ERROR_NUMERIC on unrepresentable output` clause, leaving only `JPL_OK` on success and `JPL_ERROR_ARGUMENT` for NULL output or nonfinite input.
- Verified no test asserts `JPL_ERROR_NUMERIC` from `jpl_km_to_au` (grep confirms `tests/test_convenience.c` only checks `JPL_OK`/`JPL_ERROR_ARGUMENT` for this function), so no test changes are required beyond confirming the existing positive/negative cases still pass.
- `jpl_au_per_day_to_km_per_s` (the sibling conversion function) is out of scope: its `isfinite(tmp)` check multiplies by `JPL_AU_KM / JPL_DAYSEC` (a much larger effective factor applied to a 3-vector of independently-supplied components), which is a different, still-reachable overflow case and must not be touched.

## Verification plan (all fixes)

- Fresh MSVC Debug + Release builds, full default `ctest` suite, must stay green (currently 41 tests per prior T17 evidence; count may shift slightly with the new install-symbol check).
- F1: confirm via the new install-symbol check that the installed library exports no `jpl_test_*` symbol; confirm the five switched test executables still pass linked against `jpl_testing`.
- F2/F6: confirm the default suite runs to completion on a machine/session with no Python interpreter on `PATH` (or with `PATH` temporarily stripped of it) — this is the actual regression test for "no Python dependency," not just "tests still pass" on a machine that happens to have Python. Confirm `jpl_fixture_tool`'s five mutation subcommands produce byte-identical output to the current `mutate_fixture.py` for each case before deleting the Python script.
- F3: no functional test; documentation-only, reviewed for accuracy against `src/jpl_convenience.c`.
- F4: no functional test; comment-only.
- F5: new regression test drives `select_record`/`interpolate` (or their nearest test-reachable seam) with an engineered out-of-range quotient to confirm the new guard actually triggers before the cast, plus full suite green to confirm no behavior change for valid input.
- F7: existing `jpl_km_to_au` tests in `tests/test_convenience.c` continue to pass unmodified.

## Out of scope

- No other historical or newly-found item beyond F1-F7 is in scope for this remediation round.
- No change to vendored `src/erfa/*` sources.
- No change to the public two-part-date contract, AU policy, or frame classification — this round is comment/build/test-infrastructure hygiene plus two narrow defensive/dead-code fixes, not a repeat of the architectural remediation.
