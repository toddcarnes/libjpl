# TT-TDB Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the TT-TDB Chebyshev dataset from time-segmented (`*t`) DE binaries through a new public getter plus a presence query.

**Architecture:** Extend the header parse by two IPT entries (bytes 2856-2880), store the TT-TDB pointer in `jpl_header`, interpolate it with the existing Chebyshev engine (1 component, seconds natively), and route testpo T#17 lines to the new getter instead of skipping them.

**Tech Stack:** C17 (`c_std_17`, `C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`, vendored ERFA 2.0.1 (read-only).

**Spec:** docs/superpowers/specs/2026-09-18-tt-tdb-support-design.md

## Global Constraints

- C17 with `C_EXTENSIONS OFF`; MSVC `/W4 /permissive-` zero warnings; GCC/Clang `-Wall -Wextra -Wpedantic` zero warnings (lib AND tests).
- Physics constants live in `src/jpl_private.h`, never as literals in `src/*.c`.
- `src/erfa/` is read-only vendored code; never modify.
- Public headers accumulate Doxygen (`@brief/@param/@return/@note`); `WARN_AS_ERROR=YES` must stay clean; append, don't rewrite.
- Agent-run shell commands are PowerShell 5.1: chain with `;` (no `&&`/`||`), quote paths with spaces, use `workdir` instead of `cd`.
- Commit messages follow Conventional Commits with subject + body: `type(scope): imperative subject` (max 72 chars, lowercase after colon, no trailing period), blank line, body explaining what changed and why wrapped at 72 chars, `BREAKING CHANGE:` footer when applicable. Never commit with a bare one-line subject.
- JPLEPH=`D:\ephem\jpl\bin` and testpo at `D:\ephem\jpl\testpo` (`JPL_TESTPO` env override supported); verify with `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`, plus a WSL Linux Release build+test for the POSIX branch.
- Historical docs (`docs/superpowers/specs/*`, `docs/superpowers/plans/*`) are records: do NOT rewrite them. Only live docs (headers, README) change.
- Tests use explicit `JPL_TEST_CHECK` from `tests/jpl_test_util.h`, never `assert()`.

---

## File map

- `include/jpl.h` — gains `JPL_TT_TDB = 17` enum value, `tt_tdb_pointer` + `has_tt_tdb` header fields, `jpl_has_tt_tdb()` + `jpl_get_tt_minus_tdb()` decls with Doxygen, cross-reference note on `jpl_tt_to_tdb()`.
- `src/jpl.c` — `HEADER_SIZE` 2856 to 2880, parse IPT 14 (stored, unused) + 15 (stored, consumed), derive `has_tt_tdb`, implement both new functions next to `interpolate`.
- `src/jpl_private.h` — gains nothing (the engine yields seconds and seconds/day directly with no new constant).
- `tests/test_tt_tdb.c` (new) — absent-dataset error on non-`t` mock, NULL-arg checks, presence-flag checks both ways, real-data assertion via `%JPLEPH%` when set.
- `tests/test_testpo.c` — T#17 lines routed to the new getter (X#1 vs seconds, X#2 vs rate) instead of `skipped_unsupported`.
- `tests/jpl_test_util.h` — gains a TT-TDB mock helper if the implementer judges it reduces duplication across `test_tt_tdb.c` blocks (optional, not required).
- `CMakeLists.txt` — register `jpl_tt_tdb_tests` (+ warning flags via the existing loop pattern) so the suite counts 38.
- Untouched: `src/jpl_frames.c` (ERFA approximation stays as-is), `src/erfa/*`, `src/jpl_convenience.c`.

---

### Task 1: Header parse (IPT 14/15) plus presence flag

**Files:**
- Modify: `include/jpl.h:88-102` (`jpl_header` struct), `src/jpl.c:29` (`HEADER_SIZE`), `src/jpl.c:192-205` (`parse_header` tail)
- Test: `tests/test_jpl.c` (extend mock header to 2880 bytes, assert new fields)

**Interfaces:**
- Consumes: existing `parse_header` u32-triple pattern + overflow-guard idiom.
- Produces: `header->tt_tdb_pointer` (`jpl_pointer`), `header->has_tt_tdb` (`int`), `JPL_TT_TDB` enum value consumed by Task 2.

**Steps:**

- [ ] **Step 1: Extend the mock builders first (failing test).** In `tests/jpl_test_util.h`, the header mock writes only through byte 2695 (libration pointer at 2684+12). A real 2880-byte header has IPT-14 at 2856 and IPT-15 at 2868. Append a new block **inside `main()` in `tests/test_jpl.c`** (after the truncated-file block, before `puts("jpl tests passed")` — `path`, `i`, and `RECORD_DOUBLES` are already in scope there) that builds a header with a TT-TDB pointer at file offset 2868 (`start=300, coeff=11, sub=4`, mirroring DE430t's `(939, 11, 4)` shape) and checks `header->tt_tdb_pointer.start_index == 300` and `header->has_tt_tdb != 0`, plus a block with zeros there checking `has_tt_tdb == 0`:
```c
    /* TT-TDB pointer at IPT entry 15 (file offset 2868): present case. */
    {
        unsigned char rec2[JPL_TEST_RECORD_BYTES] = {0};
        FILE *f2;
        jpl_ephemeris *e2 = NULL;
        const jpl_header *h2;
        memcpy(rec2, "TT-TDB present", 14);
        test_mock_header(rec2, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec2, 11, 2, 178, 1);
        test_put_u32(rec2 + 2868, 300);
        test_put_u32(rec2 + 2872, 11);
        test_put_u32(rec2 + 2876, 4);
        f2 = fopen(path, "wb"); JPL_TEST_CHECK(f2 != NULL);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        memset(rec2, 0, sizeof rec2);
        test_put_f64(rec2, 1.0); test_put_f64(rec2 + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        for (i = 0; i < RECORD_DOUBLES; ++i) test_put_f64(rec2 + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(rec2, 1, sizeof rec2, f2) == sizeof rec2);
        JPL_TEST_CHECK(fclose(f2) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e2) == JPL_OK);
        h2 = jpl_get_header(e2);
        JPL_TEST_CHECK(h2 != NULL && h2->tt_tdb_pointer.start_index == 300);
        JPL_TEST_CHECK(h2->tt_tdb_pointer.coefficients_per_component == 11);
        JPL_TEST_CHECK(h2->tt_tdb_pointer.subintervals == 4);
        JPL_TEST_CHECK(h2->has_tt_tdb != 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e2) != 0);
        jpl_close(e2);
        JPL_TEST_CHECK(remove(path) == 0);
    }
```
Note: `JPL_TEST_RECORD_BYTES` stays 2856 for the mock builders in this task (mocks only need the first 2880 bytes readable; the 2856-byte `rec2` buffer already covers offset 2868+12=2880 exactly — `sizeof rec2` is 2856, so offsets 2868..2879 are the last 12 bytes. Verify by arithmetic: 2868 + 12 = 2880 > 2856. The buffer is TOO SMALL. The implementer must grow the local buffer for this block (declare `unsigned char rec2[2880]` instead of `[JPL_TEST_RECORD_BYTES]`, and write/read `sizeof rec2` bytes) while leaving `JPL_TEST_RECORD_BYTES` for other tests, and file the discrepancy: `jpl_open` reads `HEADER_SIZE` bytes, so after the HEADER_SIZE change a 2856-byte mock file would truncate. This is intentional RED: the test as written above FAILS until HEADER_SIZE becomes 2880 AND the mock buffer grows. Grow the block-local buffer to 2880 in the same edit as the src change (Step 3), not before.)
- [ ] **Step 2: Run to verify it fails.**
Run: `cmake -S . -B build; cmake --build build --config Debug --target jpl_tests; ctest --test-dir build -C Debug -R "^jpl_tests$" --output-on-failure`
Expected: FAIL (no `tt_tdb_pointer` field; also `jpl_has_tt_tdb` undeclared).
- [ ] **Step 3: Implement the header changes.** In `include/jpl.h`, add to the `jpl_target` enum after `JPL_LIBRATIONS`:
```c
    JPL_TT_TDB = 17,               /**< @brief TT-TDB time correction (17): seconds, from *t files. */
```
(The value 16 is deliberately skipped: T#16 lunar mantle angular velocity is not parsed or exposed; the explicit `= 17` documents the gap.) Add to `jpl_header` after `libration_pointer`:
```c
    jpl_pointer tt_tdb_pointer; /**< @brief Coefficient layout for TT-TDB (target 17, *t files only). */
    int has_tt_tdb; /**< @brief Nonzero iff the file carries the TT-TDB dataset. */
```
In `src/jpl.c`, change `enum { HEADER_SIZE = 2856 }` to `enum { HEADER_SIZE = 2880 }` and extend `parse_header` after the libration block (line ~204) with:
```c
    /* IPT entries 14/15 live past the legacy 2856-byte cursor: entry 15 is
     * the TT-TDB dataset (*t files only); entry 14 is parsed for symmetry
     * and ignored. T#16 has no header slot and is not supported. */
    { uint32_t u_t14s = read_u32(p, order); p += 4; uint32_t u_t14c = read_u32(p, order); p += 4; uint32_t u_t14l = read_u32(p, order); p += 4;
      uint32_t u_t15s = read_u32(p, order); p += 4; uint32_t u_t15c = read_u32(p, order); p += 4; uint32_t u_t15l = read_u32(p, order);
      if (u_t14s > INT_MAX || u_t14c > INT_MAX || u_t14l > INT_MAX ||
          u_t15s > INT_MAX || u_t15c > INT_MAX || u_t15l > INT_MAX) return JPL_ERROR_FORMAT;
      header->tt_tdb_pointer.start_index = (int)u_t15s;
      header->tt_tdb_pointer.coefficients_per_component = (int)u_t15c;
      header->tt_tdb_pointer.subintervals = (int)u_t15l;
      header->has_tt_tdb = (header->tt_tdb_pointer.start_index > 0 &&
                            header->tt_tdb_pointer.coefficients_per_component > 0 &&
                            header->tt_tdb_pointer.subintervals > 0) ? 1 : 0; }
```
Declare `int jpl_has_tt_tdb(const jpl_ephemeris *e) { return e ? e->header.has_tt_tdb : 0; }` next to `jpl_record_count`, with its Doxygen decl in `jpl.h`:
```c
/**
 * @brief Reports whether the open file carries the TT-TDB dataset.
 * @param ephemeris Open handle, or NULL.
 * @return Nonzero iff the TT-TDB coefficients are present (*t files);
 *         0 if ephemeris is NULL or the dataset is absent.
 * @note Content-derived, never filename-derived: DENUM carries no t suffix.
 */
int jpl_has_tt_tdb(const jpl_ephemeris *ephemeris);
```
- [ ] **Step 4: Grow ALL 2856-byte mock buffers that feed `jpl_open`.** `jpl_open` now reads 2880 bytes; every mock that writes `JPL_TEST_RECORD_BYTES` (2856) bytes then opens would truncate-read garbage. Grep `tests/` for `JPL_TEST_RECORD_BYTES` and `HEADER_BYTES`: for each mock file that calls `jpl_open`/`jpl_open_mmap` on its output, grow the header buffer to 2880 (add `#define JPL_TEST_HEADER_BYTES 2880` to `jpl_test_util.h` and use it for header buffers; keep `JPL_TEST_RECORD_BYTES` for data-record buffers which are sized by `record_double_count`). The new test block from Step 1 uses the 2880-byte header buffer. Data-record writes are unaffected (record size comes from the header layout, not HEADER_SIZE).
- [ ] **Step 5: Run tests.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS (all suites; testpo T#17 lines still skipped at this stage).
- [ ] **Step 6: Commit (Conventional Commits, subject + body).**
```bash
git add include/jpl.h src/jpl.c tests/test_jpl.c tests/jpl_test_util.h tests/test_mmap.c tests/test_lighttime.c tests/test_convenience.c
git commit -m "feat(jpl): parse TT-TDB header pointer and report dataset presence" -m "Extend the header read from 2856 to 2880 bytes to cover IPT entries 14/15, store the TT-TDB coefficient pointer (entry 15) in jpl_header, and expose jpl_has_tt_tdb so callers detect time-segmented (*t) datasets from file content instead of the filename." -m "Add JPL_TT_TDB = 17 to the target enum (16 deliberately skipped: lunar mantle angular velocity is unsupported) and grow mock header buffers to the new header size."
```
Stage only files actually touched; drop untouched test files from the `git add`.

### Task 2: TT-TDB getter plus testpo T#17 validation

**Files:**
- Modify: `include/jpl.h` (getter decl), `src/jpl.c` (getter impl), `tests/test_testpo.c` (T#17 routing)
- Create: `tests/test_tt_tdb.c`
- Modify: `CMakeLists.txt` (register `jpl_tt_tdb_tests`)
- Test: `ctest` full (expect 38 suites: 6 unit + `jpl_tt_tdb` + 2 integration + 29 testpo)

**Interfaces:**
- Consumes: `header->tt_tdb_pointer`, `header->has_tt_tdb`, `interpolate()` from Task 1; `JPL_TT_TDB == 17`.
- Produces: `jpl_get_tt_minus_tdb()` consumed by `tests/test_testpo.c` T#17 routing.

**Steps:**

- [ ] **Step 1: Write the unit test first (failing).** Create `tests/test_tt_tdb.c`:
```c
#include "jpl.h"
#include "jpl_test_util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "jpl_test_tttdb.bin";
    /* Absent dataset: header with zero TT-TDB pointer must report
     * has == 0 and RANGE on get. */
    {
        unsigned char rec[2880] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        const jpl_header *h;
        double sec = 0.0, rate = 0.0;
        size_t i;
        memcpy(rec, "no TT-TDB here", 14);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 11, 2, 178, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        for (i = 0; i < 357; ++i) test_put_f64(rec + i * 8, (double)i);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        h = jpl_get_header(e);
        JPL_TEST_CHECK(h != NULL && h->has_tt_tdb == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e) == 0);
        JPL_TEST_CHECK(jpl_has_tt_tdb(NULL) == 0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, &sec, &rate) == JPL_ERROR_RANGE);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(NULL, 2451545.5, &sec, &rate) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, NULL, &rate) == JPL_ERROR_ARGUMENT);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    /* Present dataset, NULL rate allowed; out-of-coverage is RANGE. */
    {
        unsigned char rec[2880] = {0};
        FILE *f;
        jpl_ephemeris *e = NULL;
        double sec = 0.0, rate = -1.0;
        size_t i;
        memcpy(rec, "TT-TDB present", 14);
        test_mock_header(rec, 2451545.0, 2451546.0, 1.0, 2);
        test_put_ptr(rec, 11, 2, 178, 1);
        test_put_u32(rec + 2868, 300);
        test_put_u32(rec + 2872, 2);
        test_put_u32(rec + 2876, 1);
        f = fopen(path, "wb"); JPL_TEST_CHECK(f != NULL);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec, 1.0); test_put_f64(rec + 8, 2.0);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        memset(rec, 0, sizeof rec);
        test_put_f64(rec + 0 * 8, 2451545.0);
        test_put_f64(rec + 1 * 8, 2451546.0);
        test_put_f64(rec + 299 * 8, 0.001);
        test_put_f64(rec + 300 * 8, 0.00002);
        JPL_TEST_CHECK(fwrite(rec, 1, sizeof rec, f) == sizeof rec);
        JPL_TEST_CHECK(fclose(f) == 0);
        JPL_TEST_CHECK(jpl_open(path, &e) == JPL_OK);
        JPL_TEST_CHECK(jpl_has_tt_tdb(e) != 0);
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, &sec, NULL) == JPL_OK);
        JPL_TEST_CHECK(isfinite(sec));
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.5, &sec, &rate) == JPL_OK);
        JPL_TEST_CHECK(isfinite(rate));
        JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, 2451545.0 - 10.0, &sec, &rate) == JPL_ERROR_RANGE);
        jpl_close(e);
        JPL_TEST_CHECK(remove(path) == 0);
    }
    puts("tt-tdb tests passed");
    return 0;
}
```
Note: the present-dataset mock puts TT-TDB coefficients at slots 300-301 (1 component, 2 coeffs); slot 300 must be < `record_double_count` or `interpolate` returns FORMAT — the 2+178-coeff nutation pointer at slot 2 already forces `record_double_count` = 357, so slots 300-301 are in range. The exact coefficient values are arbitrary (mock); finiteness + OK status is the assertion — real-data accuracy is proven by the testpo suites, not this mock.
- [ ] **Step 2: Register the suite in `CMakeLists.txt` (still failing: no impl).** After the `jpl_mmap_tests` block add:
```cmake
    add_executable(jpl_tt_tdb_tests tests/test_tt_tdb.c)
    target_link_libraries(jpl_tt_tdb_tests PRIVATE jpl)
    target_compile_features(jpl_tt_tdb_tests PRIVATE c_std_17)
    add_test(NAME jpl_tt_tdb COMMAND jpl_tt_tdb_tests)
```
Also append `jpl_tt_tdb_tests` to the warnings `foreach` list (Step: keep the existing `NOT TARGET` guard pattern).
- [ ] **Step 3: Run to verify it fails.**
Run: `cmake -S . -B build; cmake --build build --config Debug --target jpl_tt_tdb_tests; ctest --test-dir build -C Debug -R "^jpl_tt_tdb$" --output-on-failure`
Expected: FAIL at configure or link (`jpl_get_tt_minus_tdb` / `jpl_has_tt_tdb` undefined — `jpl_has_tt_tdb` exists from Task 1, so the failure is the missing getter).
- [ ] **Step 4: Implement the getter in `src/jpl.c` next to `interpolate`.** Add after `jpl_get_state_lighttime`:
```c
jpl_status jpl_get_tt_minus_tdb(const jpl_ephemeris *e, double jd_tdb, double *seconds, double *rate_per_day) {
    double state[6];
    jpl_status status;
    if (!e || !seconds) return JPL_ERROR_ARGUMENT;
    if (!e->header.has_tt_tdb) return JPL_ERROR_RANGE;
    /* One-component Chebyshev set: state[0] is TT-TDB in seconds and
     * state[3] its rate in seconds/day (span is in days, so the engine
     * derivative needs no unit conversion). Slots [1],[2],[4],[5] unused. */
    status = interpolate(e, jd_tdb, &e->header.tt_tdb_pointer, 1, state);
    if (status != JPL_OK) return status;
    *seconds = state[0];
    if (rate_per_day) *rate_per_day = state[3];
    return JPL_OK;
}
```
Add the Doxygen decl in `include/jpl.h` after `jpl_record_count` (time-domain API grouped with header access, not with state vectors):
```c
/**
 * @brief Interpolates TT minus TDB at a TDB Julian date (*t files).
 * @param ephemeris Open handle.
 * @param jd_tdb Julian date (TDB) within header coverage.
 * @param seconds Output TT-TDB in seconds. Must be non-NULL.
 * @param rate_per_day Optional output rate of change in seconds/day, may be NULL.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if ephemeris/seconds is NULL,
 *         JPL_ERROR_RANGE if the file carries no TT-TDB dataset (check
 *         jpl_has_tt_tdb()) or jd_tdb is outside coverage, JPL_ERROR_FORMAT
 *         on corrupt records, JPL_ERROR_IO/MEMORY from the record read.
 * @note Coefficients are stored in seconds and the record span is in days,
 *       so no unit conversion is applied. For files without the dataset use
 *       jpl_tt_to_tdb() (~2 ms ERFA approximation) instead.
 */
jpl_status jpl_get_tt_minus_tdb(const jpl_ephemeris *e, double jd_tdb,
                                double *seconds, double *rate_per_day);
```
Also append to `jpl_tt_to_tdb()`'s `@note` in `include/jpl_frames.h`: `For exact TT-TDB from *t files see jpl_get_tt_minus_tdb().`
- [ ] **Step 5: Route T#17 lines in `tests/test_testpo.c`.** Replace the skip block:
```c
        if (target == 17) {
            /* TT-TDB lines (only in *t testpo files); jpl_get_state has no
             * equivalent, so these cannot be validated here. */
            ++skipped_unsupported;
            continue;
        }
```
with:
```c
        if (target == 17) {
            /* TT-TDB lines (only in *t testpo files): X#1 is TT-TDB in
             * seconds, X#2 its rate in seconds/day. */
            double sec = 0.0, rate = 0.0, got, tol;
            if (component != 1 && component != 2) { ++skipped_unsupported; continue; }
            if (jd < header->start_jd || jd > header->end_jd) { ++skipped_range; continue; }
            if (jpl_get_tt_minus_tdb(ephemeris, jd, &sec, &rate) != JPL_OK) {
                fprintf(stderr, "tt-tdb failed for JD %.1f component %ld\n", jd, component);
                fclose(file);
                jpl_close(ephemeris);
                return 1;
            }
            got = component == 1 ? sec : rate;
            /* Absolute tolerance per testeph practice (values are ~1e-3 s
             * scale); pin the exact digits during this task: start at 1e-12
             * and only widen with a proven coefficient-rounding case. */
            tol = 1e-12 * fmax(1.0, fabs(expected));
            if (!isfinite(got) || fabs(got - expected) > tol) {
                fprintf(stderr, "MISMATCH JD %.1f TT-TDB component %ld: got %.17g, expected %.17g (diff %.3g, tol %.3g)\n",
                        jd, component, got, expected, fabs(got - expected), tol);
                fclose(file);
                jpl_close(ephemeris);
                return 1;
            }
            ++tested;
            continue;
        }
```
Update the file's top comment (lines 25-26): T#17 lines are now validated via `jpl_get_tt_minus_tdb`, not skipped.
- [ ] **Step 6: Run the full suite.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: 38/38 PASS (6 unit + `jpl_tt_tdb` + 2 integration + 29 testpo, with the five `*t` suites now validating 876-908 T#17 lines each instead of skipping). If any T#17 MISMATCH appears, diagnose (tolerance vs code) — do not silently widen: a proven coefficient-rounding case is required, recorded in the report.
- [ ] **Step 7: Commit (Conventional Commits, subject + body).**
```bash
git add include/jpl.h include/jpl_frames.h src/jpl.c tests/test_tt_tdb.c tests/test_testpo.c CMakeLists.txt
git commit -m "feat(jpl): expose TT-TDB dataset from time-segmented ephemerides" -m "Add jpl_get_tt_minus_tdb returning TT-minus-TDB in seconds and its rate in seconds/day, interpolated from the IPT-15 Chebyshev dataset with the existing engine (no unit conversion needed)." -m "Route testpo T#17 lines to the new getter so the five *t suites validate value and rate instead of skipping them; add a dedicated unit suite for absent-dataset, NULL-arg, and NULL-rate paths."
```
Stage only files actually touched; drop untouched entries from the `git add`.

### Task 3: Cross-platform verification

**Files:**
- Modify: none (verification only; fix what fails, commits per fix)
- Test: Windows Debug full + WSL Linux Release full

**Interfaces:**
- Consumes: Tasks 1-2 landed code.
- Produces: green-suite evidence on both platforms.

**Steps:**

- [ ] **Step 1: Windows verification.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: 38/38 PASS, zero MSVC warnings (grep the build log for `warning`, case-insensitive).
- [ ] **Step 2: Linux verification (WSL).** Fresh dir `build-wsl` (MUST delete after — not gitignored):
```
JPLEPH=/mnt/d/ephem/jpl/bin JPL_TESTPO=/mnt/d/ephem/jpl/testpo cmake -S /mnt/d/src/libjpl -B /mnt/d/src/libjpl/build-wsl -DCMAKE_BUILD_TYPE=Release -DLIBJPL_BUILD_TESTS=ON
cmake --build /mnt/d/src/libjpl/build-wsl
JPLEPH=/mnt/d/ephem/jpl/bin ctest --test-dir /mnt/d/src/libjpl/build-wsl --output-on-failure
```
Expected: all suites PASS (8 base + `jpl_tt_tdb` + 29 testpo incl. five `*t` with T#17 validation), ZERO gcc warnings (grep build log for `warning`). Then `rm -rf /mnt/d/src/libjpl/build-wsl` and confirm `git status --short` clean.
- [ ] **Step 3: Commit any fixes individually** (one commit per fix, Conventional subject + body). If nothing failed, no commit — record the evidence in the ledger/report instead.

---

## Execution order

Task 1 then Task 2 (Task 2's getter needs Task 1's pointer + flag; Task 2's testpo routing needs the getter), then Task 3 verification. Final gate: Windows 38/38 + Linux all-green, zero warnings both, `git status` clean, every commit carrying subject + body.
