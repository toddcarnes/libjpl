# Refactor Improvements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement five independent code-quality improvements: (1) split large interpolation/solver functions, (2) extract ERFA version at CMake configure time, (3) optimize `read_f64` with compiler builtins, (4) move test-only hooks to a separate header, (5) document Doxygen/Graphviz build dependency.

**Architecture:** Each improvement is isolated — no public API changes, no behavioral changes. All modifications are internal refactors or build/documentation updates. Existing test suite (77 tests) serves as regression verification.

**Tech Stack:** C17, CMake 3.20+, MSVC/GCC/Clang, ERFA v2.0.1 (vendored)

**Spec:** This plan implements the recommendations from the code review dated 2026-09-26.

## Global Constraints

- C17 with `C_EXTENSIONS OFF` (enforced by CMake)
- MSVC: `/W4 /permissive-` zero warnings; GCC/Clang: `-Wall -Wextra -Wpedantic` zero warnings
- Public headers (`include/jpl.h`, `include/jpl_frames.h`, `include/jpl_convenience.h`) must not change
- ERFA vendored source (`src/erfa/*.c`, `src/erfa/*.h`) must not be modified
- Two-part Julian date arithmetic (`double[2]`) preserved through all paths
- Physics constants only from `src/jpl_private.h`
- Transactional outputs: caller buffers unchanged on failure
- Thread-safety model unchanged: mmap shareable, FILE requires `jpl_ephemeris_clone()`
- All 77 existing tests must continue to pass

---

### Task 1: Split `interpolate()` into Smaller Static Helpers

**Files:**
- Modify: `src/jpl.c:1385-1559` (the `interpolate` function)
- Test: `tests/test_jpl.c`, `tests/test_lighttime.c`, `tests/test_convenience.c` (existing tests cover behavior)

**Interfaces:**
- Consumes: existing `jpl_ephemeris`, `jpl_pointer`, `jpl_date_*` helpers
- Produces: new static functions (internal only, no header changes):
  - `static jpl_status interp_select_record(...)`
  - `static jpl_status interp_read_record_cached(...)`
  - `static jpl_status interp_verify_record_bounds(...)`
  - `static jpl_status interp_find_subinterval(...)`
  - `static jpl_status interp_chebyshev_evaluate(...)`
  - `static void interp_publish_state(...)`

- [ ] **Step 1.1: Verify current tests pass (baseline)**

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests|jpl_lighttime|jpl_convenience"
```
Expected: All tests PASS

- [ ] **Step 1.2: Extract `interp_select_record` — record index selection logic**

```c
/* Lines ~1412-1428 in current interpolate(): select_record() call + coverage check */
/* NEW static function at top of jpl.c (before interpolate) */
static jpl_status interp_select_record(const jpl_ephemeris *e, const double jd[2],
                                       size_t *record_index,
                                       double record_start[2], double record_end[2]) {
    /* Copy lines 1412-1428 from interpolate, adapting variable names */
    /* Return JPL_OK or error status */
}
```

- [ ] **Step 1.3: Run tests to verify no regression**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests|jpl_lighttime|jpl_convenience"
```
Expected: All tests PASS

- [ ] **Step 1.4: Extract `interp_read_record_cached` — FILE/mmap record read with cache**

```c
/* Lines ~1420-1443: cache check, malloc, read_doubles, cache update */
/* NEW static function */
static jpl_status interp_read_record_cached(const jpl_ephemeris *e, size_t record_index,
                                            double **out_record, int *cached_out) {
    /* Return allocated record in *out_record (caller frees), cache hit in *cached_out */
}
```

- [ ] **Step 1.5: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests|jpl_lighttime|jpl_convenience"
```
Expected: All tests PASS

- [ ] **Step 1.6: Extract `interp_verify_record_bounds` — file endpoint vs scheduled verification**

```c
/* Lines ~1444-1459: promote encoded bounds, normalize, compare vs scheduled */
/* NEW static function */
static jpl_status interp_verify_record_bounds(const jpl_ephemeris *e, const double norm[2],
                                              const double *record,
                                              double file_start[2], double file_end[2]) {
    /* Return JPL_OK or JPL_ERROR_FORMAT */
}
```

- [ ] **Step 1.7: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests|jpl_lighttime|jpl_convenience"
```
Expected: All tests PASS

- [ ] **Step 1.8: Extract `interp_find_subinterval` — subinterval index via partition**

```c
/* Lines ~1460-1490: substep calc, quotient, neighbor correction, bounded binary search */
/* NEW static function */
static jpl_status interp_find_subinterval(const jpl_ephemeris *e, const jpl_pointer *pointer,
                                          const double norm[2], const double file_start[2],
                                          const double file_end[2],
                                          size_t *subinterval, double sub_origin[2],
                                          double *normalized) {
    /* Return JPL_OK, set *subinterval, *sub_origin, *normalized */
}
```

- [ ] **Step 1.9: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests|jpl_lighttime|jpl_convenience"
```
Expected: All tests PASS

- [ ] **Step 1.10: Extract `interp_chebyshev_evaluate` — per-component Chebyshev sum + derivative**

```c
/* Lines ~1491-1541: component loop, coefficient access, recurrence, finiteness guards */
/* NEW static function */
static jpl_status interp_chebyshev_evaluate(const jpl_ephemeris *e, const jpl_pointer *pointer,
                                            size_t subinterval, double normalized,
                                            size_t components, double local_state[6]) {
    /* Return JPL_OK or JPL_ERROR_FORMAT/JPL_ERROR_NUMERIC, fill local_state[6] */
}
```

- [ ] **Step 1.11: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests|jpl_lighttime|jpl_convenience"
```
Expected: All tests PASS

- [ ] **Step 1.12: Extract `interp_publish_state` — copy locals to caller output, update cache**

```c
/* Lines ~1542-1558: memcpy state, cache refresh on FILE path */
/* NEW static function */
static void interp_publish_state(const jpl_ephemeris *e, size_t record_index,
                                 const double local_state[6], double state[6],
                                 const double *record, int cached) {
    /* memcpy state, conditionally update cache if !cached && !e->is_mmap */
}
```

- [ ] **Step 1.13: Run full test suite**

```bash
ctest --test-dir build -C Debug --output-on-failure
```
Expected: All 77 tests PASS

- [ ] **Step 1.14: Commit**

```bash
git add src/jpl.c
git commit -m "refactor: split interpolate() into focused static helpers"
```

---

### Task 2: Split `lighttime_solve()` into Smaller Static Helpers

**Files:**
- Modify: `src/jpl.c:1713-1835` (the `lighttime_solve` function)
- Test: `tests/test_lighttime.c` (existing tests cover behavior, including `jpl_test_lighttime_with_budget` hook)

**Interfaces:**
- Consumes: existing `lighttime_evaluate`, `jpl_date_*` helpers, `barycentric_state`
- Produces: new static functions (internal only):
  - `static jpl_status lt_initial_estimate(...)`
  - `static jpl_status lt_fast_iterate(...)`
  - `static jpl_status lt_bisection(...)`
  - `static jpl_status lt_check_bracket(...)`

- [ ] **Step 2.1: Verify current tests pass**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_lighttime"
```
Expected: PASS

- [ ] **Step 2.2: Extract `lt_initial_estimate` — reception-epoch geometric delay**

```c
/* Lines ~1740-1753: lighttime_evaluate at reception, zero-residual check */
/* NEW static function */
static jpl_status lt_initial_estimate(const jpl_ephemeris *e, int target,
                                      const double norm[2], const double observer_km[6],
                                      double *delay, double *residual, double rel[6]) {
    /* Return JPL_OK with *delay, *residual, rel filled; or error */
}
```

- [ ] **Step 2.3: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_lighttime"
```
Expected: PASS

- [ ] **Step 2.4: Extract `lt_fast_iterate` — up to 10 fixed-point iterations**

```c
/* Lines ~1764-1777: loop with jpl_date_add_days + lighttime_evaluate + settle check */
/* NEW static function */
static jpl_status lt_fast_iterate(const jpl_ephemeris *e, int target,
                                  const double norm[2], const double observer_km[6],
                                  int fast_budget, double *delay, double *residual,
                                  double rel[6], double state[6]) {
    /* Return JPL_OK if converged (state filled), JPL_ERROR_RANGE if crossed coverage,
       JPL_ERROR_CONVERGENCE if budget exhausted, other errors propagated */
}
```

- [ ] **Step 2.5: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_lighttime"
```
Expected: PASS

- [ ] **Step 2.6: Extract `lt_check_bracket` — validate bracket at coverage start**

```c
/* Lines ~1787-1799: evaluate at coverage start, check residual sign */
/* NEW static function */
static jpl_status lt_check_bracket(const jpl_ephemeris *e, int target,
                                   const double norm[2], const double observer_km[6],
                                   const double cov_start[2],
                                   double *lo_delay, double *lo_res, double candidate[6]) {
    /* Return JPL_OK if bracket valid (lo_res > -tol), JPL_ERROR_RANGE if lo_res < -tol,
       JPL_OK with candidate filled if |lo_res| <= tol (root at boundary) */
}
```

- [ ] **Step 2.7: Run tests**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_lighttime"
```
Expected: PASS

- [ ] **Step 2.8: Extract `lt_bisection` — 128-step sign-checked bisection**

```c
/* Lines ~1800-1833: partition midpoint, evaluate, update bracket, check convergence */
/* NEW static function */
static jpl_status lt_bisection(const jpl_ephemeris *e, int target,
                               const double norm[2], const double observer_km[6],
                               const double cov_start[2], double lo_delay, double lo_res,
                               double hi_delay, double hi_res,
                               int safe_budget, double *delay, double state[6]) {
    /* Return JPL_OK with state/delay filled if converged,
       JPL_ERROR_CONVERGENCE if budget exhausted */
}
```

- [ ] **Step 2.9: Run full test suite**

```bash
ctest --test-dir build -C Debug --output-on-failure
```
Expected: All 77 tests PASS

- [ ] **Step 2.10: Commit**

```bash
git add src/jpl.c
git commit -m "refactor: split lighttime_solve() into focused static helpers"
```

---

### Task 3: Extract ERFA Version at CMake Configure Time

**Files:**
- Modify: `CMakeLists.txt:44-50` (replace hardcoded `target_compile_definitions` for `jpl` and `jpl_testing`)

**Interfaces:**
- Consumes: `src/erfa/README-ERFA.md` (read-only parse — contains "ERFA v2.0.1")
- Produces: CMake variables `ERFA_VERSION`, `ERFA_VERSION_MAJOR`, `ERFA_VERSION_MINOR`, `ERFA_VERSION_MICRO` used in `target_compile_definitions`; `SOFA_VERSION` stays hardcoded with comment

**Ruling:** `erfaversion.c` only references PACKAGE_VERSION/SOFA_VERSION macros — it doesn't define them. The version string is in `README-ERFA.md`. SOFA_VERSION (21031011) is a SOFA release date not present in ERFA sources.

- [ ] **Step 3.1: Verify current build works**

```bash
cmake -S . -B build
cmake --build build --config Debug
```
Expected: Build succeeds

- [ ] **Step 3.2: Add CMake parse logic before `jpl` target definitions**

```cmake
# In CMakeLists.txt, after ERFA_SOURCES glob (line ~35), before target_compile_definitions (line ~44)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/erfa/README-ERFA.md" ERFA_README_CONTENT)

string(REGEX MATCH "ERFA v([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${ERFA_README_CONTENT}")
if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Failed to parse ERFA version from README-ERFA.md")
endif()
set(ERFA_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
set(ERFA_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(ERFA_VERSION_MINOR "${CMAKE_MATCH_2}")
set(ERFA_VERSION_MICRO "${CMAKE_MATCH_3}")

# SOFA_VERSION is a SOFA release date code (YYYYMMDD) associated with the
# vendored ERFA release; it is not discoverable from ERFA source files.
# Keep it hardcoded here, linked to the version above.
set(ERFA_SOFA_VERSION "20231011")

message(STATUS "ERFA version: ${ERFA_VERSION} (SOFA ${ERFA_SOFA_VERSION})")
```

- [ ] **Step 3.3: Replace hardcoded definitions for `jpl` target**

```cmake
# Replace lines 44-50 (target_compile_definitions for jpl) with:
target_compile_definitions(jpl PRIVATE
    PACKAGE_VERSION="${ERFA_VERSION}"
    PACKAGE_VERSION_MAJOR=${ERFA_VERSION_MAJOR}
    PACKAGE_VERSION_MINOR=${ERFA_VERSION_MINOR}
    PACKAGE_VERSION_MICRO=${ERFA_VERSION_MICRO}
    SOFA_VERSION="${ERFA_SOFA_VERSION}"
)
```

- [ ] **Step 3.4: Replace hardcoded definitions for `jpl_testing` target**

```cmake
# Replace lines 176-182 (target_compile_definitions for jpl_testing) with:
target_compile_definitions(jpl_testing PRIVATE
    PACKAGE_VERSION="${ERFA_VERSION}"
    PACKAGE_VERSION_MAJOR=${ERFA_VERSION_MAJOR}
    PACKAGE_VERSION_MINOR=${ERFA_VERSION_MINOR}
    PACKAGE_VERSION_MICRO=${ERFA_VERSION_MICRO}
    SOFA_VERSION="${ERFA_SOFA_VERSION}"
    LIBJPL_TESTING
)
```

- [ ] **Step 3.5: Clean rebuild and test**

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
Expected: All 77 tests PASS

- [ ] **Step 3.6: Verify version macros in compiled objects**

```bash
# On Windows (MSVC):
dumpbin /SYMBOLS build/Debug/jpl.lib | findstr PACKAGE_VERSION
# On GCC/Clang:
nm build/libjpl.a | grep PACKAGE_VERSION
```
Expected: Symbols show correct version (2.0.1)

- [ ] **Step 3.7: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: extract ERFA version at configure time from erfaversion.c"
```

---

### Task 4: Optimize `read_f64()` with Compiler Builtins

**Files:**
- Modify: `src/jpl.c:754-762` (the `read_f64` function)

**Interfaces:**
- Consumes: `jpl_byte_order` enum, 8-byte input buffer
- Produces: `double` return value (same signature, faster implementation)

- [ ] **Step 4.1: Verify current tests pass**

```bash
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests"
```
Expected: PASS (tests LE and BE mock files)

- [ ] **Step 4.2: Replace `read_f64` implementation**

```c
/* REPLACE lines 754-762 entirely with: */
static double read_f64(const unsigned char *p, jpl_byte_order order) {
    uint64_t bits;
    memcpy(&bits, p, 8);
    if (order == JPL_BYTE_ORDER_BIG_ENDIAN) {
#if defined(_MSC_VER)
        bits = _byteswap_uint64(bits);
#else
        bits = __builtin_bswap64(bits);
#endif
    }
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
```

- [ ] **Step 4.3: Verify `read_u32` unchanged (line 748-752)**

```c
/* Ensure read_u32 still uses byte-by-byte (correct, no change needed) */
static uint32_t read_u32(const unsigned char *p, jpl_byte_order order) {
    if (order == JPL_BYTE_ORDER_BIG_ENDIAN)
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}
```

- [ ] **Step 4.4: Build and run LE/BE tests**

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -R "jpl_tests"
```
Expected: PASS (big-endian mock in test_jpl.c:58-90 must decode identically)

- [ ] **Step 4.5: Run full test suite**

```bash
ctest --test-dir build -C Debug --output-on-failure
```
Expected: All 77 tests PASS

- [ ] **Step 4.6: Commit**

```bash
git add src/jpl.c
git commit -m "perf: optimize read_f64 with compiler byte-swap intrinsics"
```

---

### Task 5: Move Test Hooks to `include/jpl_testing.h`

**Files:**
- Create: `include/jpl_testing.h` (new public-but-test-only header)
- Modify: `src/jpl_private.h:95-109` (remove test declarations)
- Modify: `src/jpl.c` (add `#include "jpl_testing.h"` under `#ifdef LIBJPL_TESTING`)
- Modify: `tests/test_lighttime.c:14-16` (update include)
- Modify: `tests/test_parser.c` (update include)
- Modify: `tests/test_api_errors.c` (update include)
- Modify: `tests/test_largefile.c` (update include)
- Modify: `tests/test_numeric.c` (update include)
- Modify: `CMakeLists.txt` (ensure `jpl_testing.h` not installed, or install to test-only location)

**Interfaces:**
- Consumes: existing `LIBJPL_TESTING` macro
- Produces: `include/jpl_testing.h` with declarations:
  - `jpl_test_lighttime_with_budget`
  - `jpl_test_select_record`
  - `jpl_test_set_parser_read_limit`

- [ ] **Step 5.1: Create `include/jpl_testing.h`**

```c
/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#ifndef LIBJPL_TESTING_H
#define LIBJPL_TESTING_H

#include "jpl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Private deterministic light-time iteration-budget hook for tests.
 * Defined in the non-installed jpl_testing archive under LIBJPL_TESTING;
 * absent from the installed jpl archive. The public solver
 * calls the same internal implementation with JPL_LIGHTTIME_FAST_MAX_ITER
 * and JPL_LIGHTTIME_SAFE_MAX_ITER. Budgets count that iteration family
 * only (zero fast + zero safeguarded forces CONVERGENCE on non-root
 * cases). Negative budgets are rejected as JPL_ERROR_ARGUMENT with no
 * output written. */
#ifdef LIBJPL_TESTING
jpl_status jpl_test_lighttime_with_budget(const jpl_ephemeris *ephemeris,
    const double jd[2], int target, int center, double state[6], double *delay,
    int fast_iterations, int safeguarded_iterations);

/* Exercise the selector's own range checks without interpolate's outer
 * coverage guard. This hook exists only in the private testing archive. */
jpl_status jpl_test_select_record(const jpl_ephemeris *ephemeris,
    const double jd[2], size_t *index, double start[2], double end[2]);

/* Bounded-read test hook: caps parser source reads at max_bytes when
 * nonzero (0 disables). Production builds without LIBJPL_TESTING bypass it
 * entirely; the hook is test-only and never declared in installed headers. */
void jpl_test_set_parser_read_limit(size_t max_bytes);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LIBJPL_TESTING_H */
```

- [ ] **Step 5.2: Remove test declarations from `src/jpl_private.h`**

```c
/* DELETE lines 95-109 in jpl_private.h (the entire #ifdef LIBJPL_TESTING block) */
/* Keep lines 1-94 and 110-111 (endif) */
```

- [ ] **Step 5.3: Add include in `src/jpl.c`**

```c
/* In src/jpl.c, after line 17 (#include "jpl_private.h"), add: */
#ifdef LIBJPL_TESTING
#include "jpl_testing.h"
#endif
```

- [ ] **Step 5.4: Update test files to include new header**

```c
/* In each test file, REPLACE:
   #include "jpl_private.h"
   WITH:
   #include "jpl_private.h"
   #include "jpl_testing.h"
*/
/* Files to update:
   tests/test_lighttime.c
   tests/test_parser.c
   tests/test_api_errors.c
   tests/test_largefile.c
   tests/test_numeric.c
*/
```

- [ ] **Step 5.5: Build and run all tests**

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
Expected: All 77 tests PASS

- [ ] **Step 5.6: Verify `jpl_testing.h` is NOT installed**

```bash
# Configure install prefix
cmake -S . -B build_install -DCMAKE_INSTALL_PREFIX=./install
cmake --build build_install --config Debug --target install
# Check install tree
ls install/include/
```
Expected: Only `jpl.h`, `jpl_frames.h`, `jpl_convenience.h` present (no `jpl_testing.h`)

- [ ] **Step 5.7: Commit**

```bash
git add include/jpl_testing.h src/jpl_private.h src/jpl.c tests/test_lighttime.c tests/test_parser.c tests/test_api_errors.c tests/test_largefile.c tests/test_numeric.c
git commit -m "refactor: move test-only hooks to include/jpl_testing.h"
```

---

### Task 6: Document Doxygen/Graphviz Build Dependency

**Files:**
- Create: `BUILDING.md` (new file at repo root) OR modify `README.md`

**Interfaces:** Documentation only — no code changes

- [ ] **Step 6.1: Create `BUILDING.md`**

```markdown
# Building libjpl

## Prerequisites

- CMake 3.20+
- C17 compiler (MSVC 19.30+, GCC 11+, Clang 13+)
- No external runtime dependencies (ERFA v2.0.1 vendored)

## Standard Build

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Building Documentation

Documentation requires Doxygen. On Linux, Graphviz `dot` is also required for call graphs.

```sh
cmake -S . -B build -DLIBJPL_BUILD_DOCS=ON
cmake --build build --config Debug --target docs
```

### Platform Notes

- **Windows:** Install Doxygen via `choco install doxygen` or download from doxygen.nl. Graphviz optional (call graphs omitted if `dot` not found).
- **macOS:** `brew install doxygen graphviz`
- **Linux:** `apt-get install doxygen graphviz` (or distro equivalent)

The docs build generates:
- HTML: `build/docs/html/index.html`
- XML: `build/docs/xml/` (used by `jpl_docs_symbols` test)
```

- [ ] **Step 6.2: Verify docs build works (optional, if Doxygen installed)**

```bash
cmake -S . -B build_docs -DLIBJPL_BUILD_DOCS=ON
cmake --build build_docs --config Debug --target docs
```
Expected: HTML/XML generated without errors (if Doxygen/Graphviz present)

- [ ] **Step 6.3: Commit**

```bash
git add BUILDING.md
git commit -m "docs: add BUILDING.md with Doxygen/Graphviz dependency notes"
```

---

## Final Verification

- [ ] **Step F1: Full clean build + test**

```bash
rm -rf build
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
Expected: All 77 tests PASS, zero compiler warnings

- [ ] **Step F2: Release build verification**

```bash
cmake -S . -B build_rel -DCMAKE_BUILD_TYPE=Release
cmake --build build_rel --config Release
ctest --test-dir build_rel -C Release --output-on-failure
```
Expected: All 77 tests PASS

- [ ] **Step F3: Verify public headers unchanged**

```bash
git diff include/jpl.h include/jpl_frames.h include/jpl_convenience.h
```
Expected: No output (no changes)

- [ ] **Step F4: Verify ERFA source unchanged**

```bash
git diff src/erfa/
```
Expected: No output (no changes)

---

## Execution Notes

- Tasks 1-4 are independent and can be executed in any order
- Task 5 depends on Task 4 (test files must compile after header move)
- Task 6 is independent documentation
- Each task ends with a commit — review each commit before proceeding
- If any test fails, STOP and debug before continuing