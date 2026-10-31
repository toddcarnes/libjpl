# Review-Findings Fix Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all Critical/Important/Minor findings from the 4-reviewer audit of `b5b489c..c85dae9`.

**Architecture:** Surgical fixes only: correctness first (light-time epoch, SRS constant, overflow, thread-safety), then test-harness soundness (assert-to-check, test registration), then docs/headers. No new features, no API renames, no ERFA source edits.

**Tech Stack:** C17 (`c_std_17`, `C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`, vendored ERFA 2.0.1 (read-only).

**Spec:** docs/specs/2026-09-15-libjpl-apparent-positions-design.md (note: its light-time section lines 22-27 and ecliptic-J2000 line 82 are wrong and get corrected by this plan, not followed literally)

## Global Constraints

- C17 with `C_EXTENSIONS OFF`; MSVC `/W4 /permissive-` zero warnings; GCC/Clang `-Wall -Wextra -Wpedantic` zero warnings (applies to library AND test targets after this plan).
- Physics constants live in `src/jpl_private.h`, never as literals in `src/*.c`.
- `src/erfa/` is read-only vendored code; never modify.
- `jpl_apply_aberration` / `jpl_apply_aberration_full` and frame helpers stay pure math (no handle, no I/O).
- Public headers accumulate Doxygen (`@brief/@param/@return/@note`); `WARN_AS_ERROR=YES` must stay clean; append, don't rewrite.
- Agent-run shell commands are PowerShell 5.1: chain with `;` (no `&&`/`||`), quote paths with spaces, use `workdir` instead of `cd`.
- Verify each task with: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure` plus a Release-configure build where noted.
- JPL testpo reference files live at `D:\ephem\jpl\testpo\testpo.NNN` (text, format below); local DE binaries are in `%JPLEPH%` (=`D:\ephem\jpl\bin`, e.g. `DE441.eph`). Use them for verification.
- testpo format: 5 header lines, then `EOT`, then one record per line: `DE  DATE  JED  T# C# X#  COORDINATE` where T#=target body, C#=center body, X#=component 1-6 (x,y,z,vx,vy,vz), JED=TDB Julian date, COORDINATE=expected value in AU (pos) or AU/day (vel). Body codes match `jpl_target` (1=Mercury..13=Earth, 0=SSB; 14=nutations dpsi/deps rad, 15=librations rad).

---

## File map

- `src/jpl_private.h` — gains `JPL_SRS_AU`, `JPL_TEST`-visible nothing; consumes unused `JPL_HDR_*` macros (use or delete).
- `src/jpl.c` — light-time center-epoch fix, SRS use, overflow-checked offsets, `limits.h`, POSIX same-fd fstat, cache rework (mmap bypass + no-malloc hit + invalidate-only-on-success), `jpl.c:384` line split, dead-check cleanup.
- `src/jpl_convenience.c` — observer velocity at reception epoch, adopt `_full` with Sun-distance fetch, NULL-guard converters or documented contract.
- `src/jpl_frames.c` — `const`-qualify `rotate_state`, keep math identical.
- `include/jpl.h` — thread-safety note update (mmap shareable again), velocity-epoch note, stale `_full` rationale fix.
- `include/jpl_frames.h` — velocity R.v no-Rdot note, `jpl_tdb_to_tt` first-order-inverse note.
- `include/jpl_convenience.h` — planet==center zero-vector note, SSB-target note, Moon+Earth addition caveat.
- `tests/jpl_test_util.h` — gains `JPL_TEST_C_AU_PER_DAY`, `test_put_ptr_be`, `test_mock_header_be`, `jpl_test_fail.h` explicit-check helper (or per-file `CHECK` macro).
- `tests/test_jpl.c`, `test_lighttime.c`, `test_aberration.c`, `test_frames.c`, `test_convenience.c`, `test_mmap.c` — assert-to-check conversion, new regression tests (non-zero-velocity light-time, valid-handle 14/15, off-J2000 golden, eraAb differential, mmap thread-share).
- `tests/test_positions.c` — NULL-check `strrchr`; registration decision in CMake.
- `tests/test_all_ephemerides.c` — snprintf truncation fix.
- `CMakeLists.txt` — libm link, warning flags for tests, erfa include PRIVATE + test include path, `jpl_position_tests` register-or-guard, duplicate include fold, empty-JPLEPH guard.
- `README.md`, `AGENTS.md`, spec + plan docs — TT/TDB variables, Moon-addition caveat, self-contained snippet, Doxygen version typo, stale J2000 wording.

---

### Task 1: Correctness — light-time center epoch + SRS constant + overflow + includes

**Files:**
- Modify: `src/jpl_private.h` (add SRS constant, resolve unused HDR macros)
- Modify: `src/jpl.c:1-8` (add `#include <limits.h>`), `src/jpl.c:174,185` (addition overflow), `src/jpl.c:409-410` (offset overflow), `src/jpl.c:514-535` (light-time), `src/jpl.c:537-572` (aberration SRS use)
- Test: `tests/test_lighttime.c`, `tests/test_aberration.c`

**Interfaces:**
- Consumes: `barycentric_state()`, `jpl_get_state()`, `JPL_C_AU_PER_DAY` from `src/jpl_private.h`.
- Produces: corrected `jpl_get_state_lighttime(e, jd, target, center, ...)` = `target(jd-lt) - center(jd)`; `JPL_SRS_AU` macro consumed by Task 2 tests.

**Steps:**

- [ ] **Step 1: Write failing light-time center-epoch test.** Append to `tests/test_lighttime.c` a block with non-zero center velocity: EMB record where EMB x(t) has slope (2 Chebyshev coeffs: constant 1.0 AU + linear term giving e.g. 0.017 AU/day). Compute `geo_now = jpl_get_state(e, jd, EMB, SUN)`, call `jpl_get_state_lighttime`, assert `app[0..2]` equals `target_bary(jd-lt) - center_bary(jd)` (fetch both legs directly with `jpl_get_state` at split epochs) within 1e-12, and assert it does NOT equal the old wrong `target(jd-lt)-center(jd-lt)` when center velocity != 0:
```c
/* Non-zero center velocity: apparent must hold center at reception. */
{
    /* ... mock with EMB linear slope ... */
    double app[6], lt = 0.0, tgt_lt[6], ctr_now[6], expect[6];
    assert(jpl_get_state_lighttime(e, jd, JPL_EARTH_MOON_BARYCENTER, JPL_SUN, app, &lt) == JPL_OK);
    assert(jpl_get_state(e, jd - lt, JPL_EARTH_MOON_BARYCENTER, 0, tgt_lt) == JPL_OK);
    assert(jpl_get_state(e, jd, JPL_SUN, 0, ctr_now) == JPL_OK);
    for (int k = 0; k < 6; ++k) expect[k] = (tgt_lt[k] - ctr_now[k]) / JPL_TEST_AU_KM;
    for (int k = 0; k < 6; ++k) assert(fabs(app[k] - expect[k]) < 1e-9);
}
```
- [ ] **Step 2: Run to verify it fails.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug -R jpl_lighttime --output-on-failure`
Expected: FAIL on the new center-epoch assertions.
- [ ] **Step 3: Add `JPL_SRS_AU` to `src/jpl_private.h`, use or delete unused HDR macros.** Add after line 11:
```c
/* Schwarzschild radius of the Sun in AU (ERFA ERFA_SRS = 1.97412574336e-8). */
#define JPL_SRS_AU 1.97412574336e-8
```
Delete unused `JPL_HDR_START_JD` through `JPL_HDR_IPT` (lines 28-34), keeping `JPL_HDR_TITLES_LEN`/`JPL_HDR_TEXT_LEN` which `parse_header` uses. If you prefer to keep them, instead use them in `parse_header` — but deletion is the minimal fix.
- [ ] **Step 4: Add `#include <limits.h>` to `src/jpl.c`.** Change lines 4-8 to:
```c
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```
- [ ] **Step 5: Guard addition overflow in `parse_header`.** At `src/jpl.c:174` replace:
```c
size_t end = (size_t)pointer->start_index - 1 + component_count * (size_t)pointer->coefficients_per_component * (size_t)pointer->subintervals;
```
with:
```c
size_t product = component_count * (size_t)pointer->coefficients_per_component * (size_t)pointer->subintervals;
size_t end;
if (product > SIZE_MAX - ((size_t)pointer->start_index - 1)) return JPL_ERROR_FORMAT;
end = ((size_t)pointer->start_index - 1) + product;
```
Apply the identical guard at line 185 for the libration pointer (`3u * ...`). Also remove the dead `header->constant_count < 0` disjunct at line 158 (keep the rest of the condition) since `u_nconst > INT_MAX` already guards it.
- [ ] **Step 6: Checked offset arithmetic.** Add a static helper near `seek_to`:
```c
static int checked_record_offset(size_t index, size_t record_doubles, size_t *out) {
    size_t a, b;
    if (record_doubles > SIZE_MAX / 8) return -1;
    b = record_doubles * 8;
    if (index > (SIZE_MAX / b) - 2) return -1;
    a = (index + 2) * b;
    *out = a;
    return 0;
}
```
Use it in `jpl_read_constants` (offset `record_double_count * 8` — check via same helper with index `SIZE_MAX` sentinel is overkill; just check `record_double_count <= SIZE_MAX/8` before multiply, which `jpl_open` already enforces, plus keep the explicit check) and `jpl_read_record`:
```c
jpl_status jpl_read_record(const jpl_ephemeris *e, size_t index, double *values, size_t count) {
    size_t offset;
    if (!e || !values || count < e->header.record_double_count) return JPL_ERROR_ARGUMENT;
    if (index >= e->record_count) return JPL_ERROR_RANGE;
    if (checked_record_offset(index, e->header.record_double_count, &offset) != 0) return JPL_ERROR_IO;
    return read_doubles(e, offset, values, e->header.record_double_count);
}
```
- [ ] **Step 7: Fix light-time to hold center at reception.** Replace the loop in `jpl_get_state_lighttime` (`src/jpl.c:514-535`) with:
```c
jpl_status jpl_get_state_lighttime(const jpl_ephemeris *e, double jd, int target, int center, double state[6], double *out_lt) {
    double rel[6], tgt[6], ctr[6], lt = 0.0, prev = 0.0;
    jpl_status st;
    int i;
    size_t k;
    if (!e || !state) return JPL_ERROR_ARGUMENT;
    if (target < 0 || target > JPL_EARTH || center < 0 || center > JPL_EARTH) return JPL_ERROR_ARGUMENT;
    /* Center evaluated once at reception jd; only the target leg iterates. */
    if (center == 0) memset(ctr, 0, sizeof ctr);
    else { st = barycentric_state(e, jd, center, ctr); if (st != JPL_OK) return st; }
    st = jpl_get_state(e, jd, target, center, rel);
    if (st != JPL_OK) return st;
    lt = range_days(rel);
    for (i = 0; i < JPL_LIGHTTIME_MAX_ITER; ++i) {
        prev = lt;
        st = barycentric_state(e, jd - lt, target, tgt);
        if (st != JPL_OK) return st;
        for (k = 0; k < 6; ++k) rel[k] = (tgt[k] - ctr[k]) / e->header.astronomical_unit_km;
        lt = range_days(rel);
        if (fabs(lt - prev) < JPL_LIGHTTIME_TOL_DAYS) break;
    }
    if (fabs(lt - prev) >= JPL_LIGHTTIME_TOL_DAYS) return JPL_ERROR_RANGE;
    memcpy(state, rel, sizeof rel);
    if (out_lt) *out_lt = lt;
    return JPL_OK;
}
```
Note: `barycentric_state` returns km-based barycentric legs; the division by `astronomical_unit_km` matches `jpl_get_state`. SSB legs (target/center 0) are zero-filled by `barycentric_state`.
- [ ] **Step 8: Use `JPL_SRS_AU`, drop local literal.** In `jpl_apply_aberration_full` replace lines 545-546:
```c
    /* ERFA SRS constant (Schwarzschild radius of the Sun, AU): see erfa.h. */
    static const double SRS = 9.870730742e-9;
```
with:
```c
    /* Solar gravitational radius; ERFA ERFA_SRS, kept in jpl_private.h. */
    const double SRS = JPL_SRS_AU;
```
and line 564 `SRS/sun_distance_au` is then correct (2x old value). Update the `@note` in `include/jpl.h:237-238` only if it quotes the value (it doesn't — no change needed there).
- [ ] **Step 9: Split `src/jpl.c:384` onto two lines.**
```c
        if (!ephem->refcount) { free(c); return JPL_ERROR_ARGUMENT; }
        c->refcount = ephem->refcount;
```
- [ ] **Step 10: Tighten aberration differential test.** In `tests/test_aberration.c` after the existing `_full` block, add a direct `eraAb`-vs-`jpl_apply_aberration_full` comparison at finite Sun distance with tolerance `1e-12`:
```c
    /* Differential vs eraAb at finite Sun distance (catches SRS drift). */
    {
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*173.144632684, vc[1]*173.144632684, vc[2]*173.144632684};
        double got[6], be[3], exp[3];
        double s_au = 0.99980921395708788;
        int k;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        jpl_apply_aberration_full(s2, vo2, s_au, got);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) assert(fabs(got[k] - exp[k]) < 1e-12);
    }
```
- [ ] **Step 11: Run tests.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS (light-time + aberration suites).
- [ ] **Step 12: Commit.**
```bash
git add src/jpl.c src/jpl_private.h tests/test_lighttime.c tests/test_aberration.c
git commit -m "fix: correct light-time center epoch, SRS constant, overflow checks"
```

### Task 2: Thread-safety — lock-free shareable mmap reads

**Files:**
- Modify: `src/jpl.c:49-54,412-476` (cache path), `include/jpl.h:187-189,253-258` (thread notes)
- Test: `tests/test_mmap.c` (share-across-threads or sequential-share regression)

**Interfaces:**
- Consumes: `jpl_read_record()`, `read_doubles()` (already thread-safe on mmap: read-only mapping).
- Produces: mmap reads perform zero per-handle mutation; FILE path keeps cache; `jpl_get_state` doc promises restored.

**Steps:**

- [ ] **Step 1: Write mmap-share regression test.** Add to `tests/test_mmap.c` after line 21:
```c
    /* mmap handle must be shareable: interleaved state reads on one handle
     * equal reads on a distinct clone (pins no-per-handle-mutation). */
    {
        double a[6], b[6];
        int k, same = 1;
        JPL_TEST_CHECK(jpl_get_state(e, 2451545.5, JPL_NUTATIONS, 0, a) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(c, 2451545.5, JPL_NUTATIONS, 0, b) == JPL_OK);
        JPL_TEST_CHECK(jpl_get_state(e, 2451545.5, JPL_NUTATIONS, 0, a) == JPL_OK);
        for (k = 0; k < 6; ++k) if (a[k] != b[k]) same = 0;
        JPL_TEST_CHECK(same);
    }
```
(This needs `jpl_test_util.h` CHECK macro from Task 4; if Task 2 lands first, use `assert` temporarily and convert in Task 4. Functionally passes before and after — it pins behavior; the no-mutation property is verified by inspection plus ThreadSanitizer where available.)
- [ ] **Step 2: Bypass cache on mmap path; no-malloc hit; invalidate-only-on-success.** Replace `interpolate` cache prologue/epilogue (`src/jpl.c:422-434`, `465-474`):
```c
    int use_cache = !e->is_mmap;
    cached = use_cache && e->cache_valid && e->cache_record && e->cache_index == record_index;
    /* NOTE: no invalidation here; cache is only replaced after success. */
    if (cached) {
        record = malloc(...); if (!record) return JPL_ERROR_MEMORY;
        memcpy(record, e->cache_record, ...);
    } else {
        record = malloc(...); if (!record) return JPL_ERROR_MEMORY;
        status = jpl_read_record(...); if (status != JPL_OK) { free(record); return status; }
    }
```
and at the end:
```c
    if (use_cache) {
        if (!((jpl_ephemeris *)e)->cache_record) {
            ((jpl_ephemeris *)e)->cache_record = malloc(...);
            if (!((jpl_ephemeris *)e)->cache_record) { free(record); return JPL_OK; }
        }
        memcpy(...); ((jpl_ephemeris *)e)->cache_index = record_index;
        ((jpl_ephemeris *)e)->cache_valid = 1;
    }
    free(record);
    return JPL_OK;
```
Also remove the `((jpl_ephemeris *)e)->cache_valid = 0;` on entry (line 426). FILE-path signature keeps `const jpl_ephemeris *` with documented interior mutability (note already says so); do not change the signature.
- [ ] **Step 3: Restore doc contract.** In `include/jpl.h:187-189` replace:
```
 *       A single handle must not be shared across threads, on either the
 *       FILE or mmap path: reads update a per-handle record cache, so
 *       sharing is a data race. Use jpl_ephemeris_clone() per thread.
```
with:
```
 *       A single handle must not be shared across threads on the FILE
 *       path: reads update a per-handle record cache, so sharing is a
 *       data race. The mmap path performs no per-handle mutation and is
 *       safe for concurrent reads on one handle; either way,
 *       jpl_ephemeris_clone() per thread is always safe.
```
Apply the same correction to `jpl_open_mmap` note (`include/jpl.h:253-258`): mmap handle IS shareable for reads; clones still safe; clone-vs-close still needs external sync.
- [ ] **Step 4: Run tests.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS.
- [ ] **Step 5: Commit.**
```bash
git add src/jpl.c include/jpl.h tests/test_mmap.c
git commit -m "fix: keep mmap reads lock-free shareable, cache FILE path only"
```

### Task 3: Convenience correctness — observer epoch, _full adoption, NULL contracts

**Files:**
- Modify: `src/jpl_convenience.c`, `include/jpl_convenience.h`
- Test: `tests/test_convenience.c`

**Interfaces:**
- Consumes: corrected `jpl_get_state_lighttime` (Task 1), `jpl_apply_aberration_full`, `jpl_get_state`.
- Produces: `jpl_planet_position` evaluated at reception velocity with solar term.

**Steps:**

- [ ] **Step 1: Write failing observer-epoch test.** Add a non-zero-velocity mock block to `tests/test_convenience.c`: EMB with linear slope (velocity ~0.01 AU/day transverse to the line of sight so aberration shifts y). Assert `jpl_planet_position` output equals `jpl_apply_aberration_full(lighttime_state, v_center(tdb), s_sun)` composed manually with `v_center` fetched at `tdb` (not `tdb-lt`). Current code fetches at `tdb-lt` so the test fails by the velocity-delta amount; with small lt the delta is small, so choose slope large enough (e.g. 0.5 AU/day over 0.006 d lt gives ~0.003 AU position-equivalent lever — assert with tolerance 1e-9 on velocity input instead: capture via manual composition). Simplest robust assertion: mock where center velocity varies linearly (quadratic position term), then reception vs emission velocities differ; assert convenience output matches reception-velocity composition within 1e-12.
- [ ] **Step 2: Run to verify it fails.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug -R jpl_convenience --output-on-failure`
Expected: FAIL.
- [ ] **Step 3: Fix `src/jpl_convenience.c`.** Replace lines 25-31:
```c
    /* Aberration needs the observer (center-body) barycentric velocity at
     * reception (tdb); stellar aberration is a reception phenomenon.
     * Sun distance for the <=0.4uas Klioner term comes from one extra
     * evaluation (skipped when the observer is the Sun itself). */
    if (center != 0) {
        s = jpl_get_state(e, tdb, center, 0, cst);
        if (s != JPL_OK) return s;
        vobs[0] = cst[3]; vobs[1] = cst[4]; vobs[2] = cst[5];
    } else { vobs[0] = vobs[1] = vobs[2] = 0.0; }
    if (center != JPL_SUN) {
        double sun_obs[6];
        s = jpl_get_state(e, tdb, center, JPL_SUN, sun_obs);
        if (s == JPL_OK) {
            double sdist = sqrt(sun_obs[0]*sun_obs[0] + sun_obs[1]*sun_obs[1] + sun_obs[2]*sun_obs[2]);
            jpl_apply_aberration_full(tmp, vobs, sdist, tmp);
        } else return s;
    } else {
        jpl_apply_aberration(tmp, vobs, tmp);
    }
```
Add `#include <math.h>` at top for `sqrt`. Note: when `center == 0` (SSB), observer velocity is heliocentric SSB velocity — fetching `jpl_get_state(e, tdb, 0, 0)` is zeros; keep zeros with a comment.
- [ ] **Step 4: NULL-guard converters (keep header contract, add safety).** Replace:
```c
void jpl_au_per_day_to_km_per_s(const double v[3], double o[3]) {
    const double K = JPL_AU_KM/JPL_DAYSEC;
    if (!v || !o) return;
    o[0]=v[0]*K; o[1]=v[1]*K; o[2]=v[2]*K;
}
```
(`jpl_km_to_au` already NULL-guards `au`; leave it.)
- [ ] **Step 5: Document `planet==center` and SSB-target notes in `include/jpl_convenience.h:19-22`.** Append to the `@return` block:
```
 * @note If planet == center the result is the zero vector (JPL_OK).
 *       Planet SSB (12) is rejected here; use jpl_get_state() for SSB geometry.
```
- [ ] **Step 6: Run tests.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS.
- [ ] **Step 7: Commit.**
```bash
git add src/jpl_convenience.c include/jpl_convenience.h tests/test_convenience.c
git commit -m "fix: observer velocity at reception, use full aberration term"
```

### Task 4: Harness soundness — explicit checks (no assert), test registration, libm, warnings

**Files:**
- Modify: `CMakeLists.txt`, `tests/jpl_test_util.h`, all `tests/test_*.c`, `tests/test_positions.c`, `tests/test_all_ephemerides.c`
- Test: full `ctest` in Debug AND Release.

**Interfaces:**
- Consumes: nothing from Tasks 1-3 (independent build change; coordinate on test-file edits to avoid merge conflicts — do this task after 1-3 land, or on separate files only).
- Produces: Release-mode tests actually verify; `ctest -N` lists 7 suites.

**Steps:**

- [ ] **Step 1: Add explicit-check helper to `tests/jpl_test_util.h`.**
```c
#include <stdio.h>
#include <stdlib.h>
#define JPL_TEST_C_AU_PER_DAY 173.144632684
#define JPL_TEST_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)
```
Also add BE helpers to stop hand-rolled offsets:
```c
static inline void test_put_ptr_be(unsigned char *rec, int idx, uint32_t start, uint32_t coeff, uint32_t sub) {
    test_put_u32be(rec + 2696 + idx * 12, start);
    test_put_u32be(rec + 2696 + idx * 12 + 4, coeff);
    test_put_u32be(rec + 2696 + idx * 12 + 8, sub);
}
static inline void test_mock_header_be(unsigned char *rec, double start_jd, double end_jd,
                                       double step_days, uint32_t nconst) {
    test_put_f64be(rec + 2652, start_jd);
    test_put_f64be(rec + 2660, end_jd);
    test_put_f64be(rec + 2668, step_days);
    test_put_u32be(rec + 2676, nconst);
    test_put_f64be(rec + 2680, JPL_TEST_AU_KM);
    test_put_f64be(rec + 2688, JPL_TEST_MASS_RATIO);
}
```
- [ ] **Step 2: Convert every `assert(` to `JPL_TEST_CHECK(` in `tests/test_jpl.c`, `test_lighttime.c`, `test_aberration.c`, `test_frames.c`, `test_convenience.c`, `test_mmap.c`.** Keep `#include <assert.h>` removal: delete the include, add `#include "jpl_test_util.h"` where missing (`test_frames.c`, `test_aberration.c` need it for the macro + `test_norm3`). Replace float literals `173.144632684` with `JPL_TEST_C_AU_PER_DAY` in `test_lighttime.c:43`, `test_aberration.c:41,54`, `test_convenience.c` as applicable. In `test_jpl.c` BE block replace the six hand-rolled `test_put_f64be`/`test_put_u32be` header lines with `test_mock_header_be(be, ...)` + `test_put_ptr_be(be, 11, 2, 178, 1)`.
- [ ] **Step 3: Fix `tests/test_all_ephemerides.c:25` truncation check.**
```c
int n = snprintf(path, sizeof path, "%s/%s", argv[1], filenames[i]);
if (n < 0 || (size_t)n >= sizeof path) return 2;
```
- [ ] **Step 4: Fix `tests/test_positions.c:25` NULL deref.**
```c
jd = 0.0;
{ const char *dash = strrchr(label, '-'); if (dash) jd = strtod(dash, NULL); }
```
Also check `fopen(argv[2])` failure path already closes ephemeris (it does); add `JPL_TEST_CHECK`-style explicit returns (this file already returns codes, keep them, just fix the NULL bug).
- [ ] **Step 5: CMake fixes.** Apply all of:
  - Fold duplicate `target_include_directories(jpl PUBLIC .../include)` (lines 25-29 + 43) into one block.
  - Change line 36 `src/erfa` include from `PUBLIC` to `PRIVATE`; add `target_include_directories(jpl_frames_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa)` and same for `jpl_aberration_tests` (both include `erfa.h` directly).
  - Add math lib: `target_link_libraries(jpl PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)`.
  - Propagate warnings to tests: after the `if(MSVC)...else()` block add
    (only for targets that exist — omit `jpl_position_tests` if you deleted
    that target in the registration step below):
```cmake
foreach(tgt IN ITEMS jpl_tests jpl_lighttime_tests jpl_aberration_tests jpl_frames_tests jpl_convenience_tests jpl_mmap_tests jpl_position_tests)
    if(NOT TARGET ${tgt})
        continue()
    endif()
    if(MSVC)
        target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${tgt} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
```
  - Guard empty JPLEPH: replace line 113 `if(DEFINED ENV{JPLEPH} AND EXISTS ...)` with `if(NOT "$ENV{JPLEPH}" STREQUAL "" AND EXISTS "$ENV{JPLEPH}/DE441.eph")`.
  - Register or guard `jpl_position_tests` (lines 105-107): move those three lines inside the `if(...JPLEPH...)` block and add:
```cmake
        add_test(NAME jpl_positions COMMAND jpl_position_tests "$ENV{JPLEPH}/DE441.eph" "${CMAKE_CURRENT_SOURCE_DIR}/tests/de441_reference.txt")
```
only if `tests/de441_reference.txt` exists at configure time; otherwise leave the executable unbuilt (delete lines 105-107 from the unconditional section). NOTE: no reference file exists in the repo — instead wire testpo validation (see Task 8): either delete the `jpl_position_tests` target or repurpose it as the testpo runner. Check first: `tests/de441_reference.txt` is absent, so go with the Task 8 approach.
- [ ] **Step 6: Add valid-handle nutation rejection test.** In `tests/test_lighttime.c` after the NULL-handle asserts, inside the mock block add:
```c
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, jd, JPL_NUTATIONS, 0, app, &lt) == JPL_ERROR_ARGUMENT);
        JPL_TEST_CHECK(jpl_get_state_lighttime(e, jd, JPL_LIBRATIONS, 0, app, &lt) == JPL_ERROR_ARGUMENT);
```
(replaces the vacuous NULL-handle nutation check at line 11 — keep line 11, add these.)
- [ ] **Step 7: Run Debug + Release.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Then: `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release; cmake --build build-rel; ctest --test-dir build-rel --output-on-failure`
(Second command is single-config; on MSVC use `--config Release` + `-C Release` instead.)
Expected: PASS both; `ctest -N` shows the registered suites.
- [ ] **Step 8: Commit.**
```bash
git add CMakeLists.txt tests/
git commit -m "fix: explicit test checks, register position tests, libm and warnings"
```

### Task 5: Docs + headers — velocity semantics, TT/TDB, Moon caveat, stale wording

**Files:**
- Modify: `include/jpl.h`, `include/jpl_frames.h`, `include/jpl_convenience.h`, `src/jpl_frames.c`, `README.md`, `AGENTS.md`, spec + plan docs
- Test: Doxygen build if available; `ctest` unaffected.

**Interfaces:**
- Consumes: corrected semantics from Tasks 1-3 (documents them, changes no code except `const`).
- Produces: headers + docs match implementation.

**Steps:**

- [ ] **Step 1: `include/jpl.h` — velocity epoch + stale rationale.** In `jpl_get_state_lighttime` `@note` (lines 206-208) append: `State velocity is at the retarded emission epoch; aberration is not applied (use jpl_apply_aberration_full with the observer velocity at reception).` In `jpl_apply_aberration` `@note` (lines 218-224) replace the `Known limitation...` paragraph with: `For the <=0.4uas solar term use jpl_apply_aberration_full when the Sun distance is known.`
- [ ] **Step 2: `include/jpl_frames.h` + `src/jpl_frames.c` const.** Add to both `jpl_ecliptic_to_icrs` and `jpl_icrs_to_ecliptic` notes: `@note Velocity is rotated as a direction at fixed jd_tt (R.v); frame-rate dR/dt terms from precession are not included (~50"/yr x 1 AU ~= 1 m/s).` Change `jpl_tdb_to_tt` note (line 47) to: `@note First-order inverse (single dtdb evaluation at jd_tdb); round-trip <1e-9 d, far below the ~2 ms eraDtdb zero-observer floor.` In `src/jpl_frames.c:13` change signature to `static void rotate_state(const double R[3][3], const double s[6], double o[6])`.
- [ ] **Step 3: `include/jpl_convenience.h` Moon caveat.** Replace `@note To get Moon wrt another body...` (lines 31-32) with:
```
 * @note Moon+Earth vector addition mixes legs with different emission
 *       epochs and aberration corrections; it is approximate (km-level for
 *       moving centers). For consistent apparent Moon-Sun use geometric
 *       jpl_get_state() legs at a common epoch plus manual light-time/aberration,
 *       and note state[3..5] below are retarded-epoch, unaberrated velocities.
```
Add to `jpl_planet_position` notes: `state[3..5] is the retarded-epoch velocity, passed through aberration unchanged.`
- [ ] **Step 4: `README.md` fixes.** Change second snippet (lines 50-54) to use distinct variables:
```c
jpl_ephemeris *de; /* from jpl_open(...) */
double jd_tt = 2451545.0, jd_tdb = jpl_tt_to_tdb(jd_tt);
double geo[6], app[6], lt;
if (jpl_get_state_lighttime(de, jd_tdb, JPL_MARS, JPL_SUN, geo, &lt) != JPL_OK) { /* handle error */ }
if (jpl_planet_position(de, jd_tt, JPL_MARS, JPL_SUN, app) != JPL_OK) { /* handle error */ }
```
Add `#include <jpl_frames.h>`-adjacent note or the missing `#include <jpl_convenience.h>` + `de` declaration so the snippet is self-contained; add one-line caveat under the Moon snippet: `Note: adding apparent legs mixes emission epochs; approximate only (see jpl_convenience.h).`
- [ ] **Step 5: `AGENTS.md:38` Doxygen typo.** Replace `(1.17+ known good)` with `(1.9+ known good)`.
- [ ] **Step 6: Spec/plan stale J2000.** In `docs/specs/2026-09-15-libjpl-apparent-positions-design.md:82` change `Ecliptic (J2000)` to `Ecliptic (of date)`; in `docs/plans/2026-09-16-libjpl-apparent-positions.md` lines 415-418 same replacement. In the spec light-time section (lines 22-27) correct step 3 to `Re-evaluate TARGET leg at jd_tdb - light_time; center stays at jd_tdb`.
- [ ] **Step 7: `tests/test_frames.c:59-61` comment fix.** Replace `Independent velocity check: finite-difference of the rotation at two epochs must match...` with `Fixed-epoch rigid rotation: v_out == R * v_in exactly at one epoch.`
- [ ] **Step 8: Verify + commit.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS. Optionally `cmake -S . -B build-docs -DLIBJPL_BUILD_DOCS=ON` if Doxygen installed.
```bash
git add include/ src/jpl_frames.c README.md AGENTS.md docs/ tests/test_frames.c
git commit -m "docs: velocity/TT-TDB/Moon caveats, J2000 wording, Doxygen typo"
```

### Task 6: Frames test independence — off-J2000 golden vs independent composition

**Files:**
- Modify: `tests/test_frames.c`
- Test: `ctest -R jpl_frames`

**Interfaces:**
- Consumes: `jpl_frame_rotation_matrix`, `eraPmat06`/`eraObl06`/`eraRx` direct composition (independent of `eraEcm06` path under test only in the sense of different call sequence; golden constants checked in).
- Produces: regression pin for "wrong ERFA routine" class of bug.

**Steps:**

- [ ] **Step 1: Add off-J2000 absolute test at 2460000.5.** Append:
```c
    /* Off-J2000 absolute: independent composition (Pmat06 + Rx(obliquity))
     * must agree with the eraEcm06-based path to 1e-12. */
    {
        double jd = 2460000.5;
        double R[3][3], Rp[3][3], ecm[3][3], p[3] = {0.2, -0.5, 0.8}, a[3], b[3];
        double eps = eraObl06(JPL_DJM0_TEST, jd - JPL_DJM0_TEST);
        eraPmat06(JPL_DJM0_TEST, jd - JPL_DJM0_TEST, Rp);
        eraRx(eps, Rp); /* Rp = Rx(eps) * Pmat06: ICRS -> mean ecliptic of date */
        eraTr(Rp, ecm); /* transpose: ecliptic -> ICRS */
        jpl_frame_rotation_matrix(jd, R);
        eraRxp(R, p, a);
        eraRxp(ecm, p, b);
        for (int k = 0; k < 3; ++k) assert(fabs(a[k] - b[k]) < 1e-12);
    }
```
with `#define JPL_DJM0_TEST 2400000.5` at file top (do not include private header from tests). Note: `eraEcm06` also applies frame bias; `eraPmat06`-only composition differs by bias (~mas). If the 1e-12 check fails on bias, widen to `1e-7` with comment `bias-level agreement`, OR compose with `eraPfwp06`+bias per ERFA docs. Prefer: check agreement to `2e-7` rad (~40 mas) documenting bias as the floor, and ALSO check in a checked-in golden vector: record actual `jpl_ecliptic_to_icrs({0.2,-0.5,0.8,...}, 2460000.5)` output once, verify vs `eraObl06`-tilt sanity, then pin it with `1e-12` exact-match asserts so future routine swaps fail.
- [ ] **Step 2: Run.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug -R jpl_frames --output-on-failure`
Expected: PASS (adjust tolerance per bias finding above, documenting the choice).
- [ ] **Step 3: Commit.**
```bash
git add tests/test_frames.c
git commit -m "test: off-J2000 absolute frames check with golden vector"
```

### Task 7: POSIX mmap same-fd fstat + full verification

**Files:**
- Modify: `src/jpl.c:325-349` (POSIX `jpl_open_mmap`)
- Test: full suite + Linux Release build.

**Interfaces:**
- Consumes: POSIX `open/mmap/fstat` only; Windows path untouched (already same-handle re-check).
- Produces: TOCTOU narrowed to same-fd check.

**Steps:**

- [ ] **Step 1: Keep fd open across mmap, fstat same fd.** Replace:
```c
        file_size = (size_t)st.st_size;
        base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (base == MAP_FAILED) return JPL_ERROR_IO;
        /* Re-check size after mapping ... */
        {
            struct stat rest;
            int rfd = open(path, O_RDONLY);
            if (rfd >= 0) {
                if (fstat(rfd, &rest) != 0 || rest.st_size != st.st_size) {
                    close(rfd); munmap(base, file_size); return JPL_ERROR_IO;
                }
                close(rfd);
            }
        }
```
with:
```c
        file_size = (size_t)st.st_size;
        base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (base == MAP_FAILED) { close(fd); return JPL_ERROR_IO; }
        /* Re-check size on the SAME fd after mapping: renames/replacements
         * of path no longer validate the wrong file. Truncation after
         * mapping can still fault on access (documented). */
        {
            struct stat rest;
            if (fstat(fd, &rest) != 0 || rest.st_size != st.st_size) {
                close(fd); munmap(base, file_size); return JPL_ERROR_IO;
            }
        }
        close(fd);
```
- [ ] **Step 2: Full verification (Debug + Release + docs if available).**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS, all suites (6 unit + integration if JPLEPH set).
Then Release: `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release; cmake --build build-rel; ctest --test-dir build-rel --output-on-failure`
Expected: PASS (this is the configuration that previously ran vacuous asserts).
- [ ] **Step 3: Commit.**
```bash
git add src/jpl.c
git commit -m "fix: POSIX mmap same-fd size recheck"
```

---

## Execution order

Tasks 1-3 are correctness and touch overlapping files (`src/jpl.c`, tests) — run sequentially. Task 4 touches CMake + all test files; run after 1-3 to avoid conflicts (or coordinate). Tasks 5-7 are docs/independent and can follow in any order after 1-4. Task 8 is the testpo validation harness; it needs Tasks 1+4 done first (correct light-time + explicit checks), and it resolves the `jpl_position_tests` registration question — run it after Task 4. Final gate: full Debug + Release `ctest` green, `WARN_AS_ERROR=YES` Doxygen clean if enabled, MSVC `/W4` + GCC `-Wall -Wextra -Wpedantic` zero warnings on lib AND tests.

### Task 8: testpo validation harness — verify against JPL reference data

**Files:**
- Create: `tests/test_testpo.c` (or repurpose `tests/test_positions.c` — implementer decides; if repurposing, delete the old file)
- Modify: `CMakeLists.txt` (register `jpl_testpo` test wired to `%JPLEPH%` + `D:\ephem\jpl\testpo`; resolve `jpl_position_tests` per Task 4 note)
- Test: `ctest -R jpl_testpo`

**Interfaces:**
- Consumes: `jpl_open()`, `jpl_get_state()` (geometric, TDB); testpo text files.
- Produces: `jpl_testpo` ctest suite validating every testpo line for the DE441 binary (and every other DE binary present in `%JPLEPH%` with a matching `testpo.NNN`).

**Steps:**

- [ ] **Step 1: Inspect the fixtures.** Read the first 10 lines of `D:\ephem\jpl\testpo\testpo.441`, list `%JPLEPH%` (`D:\ephem\jpl\bin`) binaries, and confirm which `testpo.NNN` files have a matching `DENNN.eph` (441 definitely; others as available). Note the format: 5 header lines, `EOT` line, then `DE DATE JED T# C# X# COORDINATE`.
- [ ] **Step 2: Write `tests/test_testpo.c`.** Usage: `test_testpo <eph-path> <testpo-path>`. Behavior:
```c
#include "jpl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    /* argv[1] = DE binary, argv[2] = testpo text file.
     * For each data line: parse JED, target, center, component X# (1-based
     * into state[6]), expected coordinate. Call jpl_get_state(e, jed,
     * target, center, state) and compare state[X#-1] with tolerance
     * 5e-13 * fmax(1.0, fabs(expected)) (same rule as test_positions.c).
     * Skip lines whose jd is outside header coverage (some testpo files
     * span wider than a given binary). Lines with target 14/15 compare
     * against nutation/libration slots per jpl_get_state docs.
     * Return 0 only if >= 100 lines were tested and all passed; print the
     * count. Use explicit checks (fprintf + return 1), never assert().
     * Parse defensively: sscanf with field widths, check the return count,
     * never pass NULL from strrchr to strtod. */
}
```
Parsing detail: data lines look like `441  2000-JAN-01  2451545.0  3  12  1  -0.123...` — but the exact whitespace/columns vary by file; parse robustly with `sscanf(line, "%*s %*s %lf %d %d %d %lf", &jd, &target, &center, &comp, &expected)` requiring 5 conversions, `continue` otherwise. The leading `DE` number and date string are skipped, not validated.
- [ ] **Step 3: Run it manually first (RED).**
Run: `cmake -S . -B build; cmake --build build --config Debug --target jpl_testpo_tests`
then `.\build\Debug\jpl_testpo_tests.exe D:\ephem\jpl\bin\DE441.eph D:\ephem\jpl\testpo\testpo.441`
Expected: FAIL (target does not exist yet) — then implement and re-run, expecting PASS with thousands of lines validated.
- [ ] **Step 4: Register in CMakeLists.txt.** Guard on both env vars / files existing at configure time:
```cmake
if(NOT "$ENV{JPLEPH}" STREQUAL "" AND EXISTS "$ENV{JPLEPH}/DE441.eph" AND EXISTS "D:/ephem/jpl/testpo/testpo.441")
    add_executable(jpl_testpo_tests tests/test_testpo.c)
    target_link_libraries(jpl_testpo_tests PRIVATE jpl)
    target_compile_features(jpl_testpo_tests PRIVATE c_std_17)
    if(MSVC)
        target_compile_definitions(jpl_testpo_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(jpl_testpo_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(jpl_testpo_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME jpl_testpo_441 COMMAND jpl_testpo_tests "$ENV{JPLEPH}/DE441.eph" "D:/ephem/jpl/testpo/testpo.441")
endif()
```
Also loop over other `testpo.NNN` files with matching `DENNN.eph` in `%JPLEPH%` and register one test each (implementer: enumerate at configure time with `file(GLOB)` on the testpo dir + `if(EXISTS)` on the binary, or hardcode the pairs found in Step 1 — hardcoding is fine and clearer).
- [ ] **Step 5: Resolve `jpl_position_tests`.** `tests/de441_reference.txt` does not exist, so do NOT wire `jpl_position_tests` to it. Either (a) delete the target + `tests/test_positions.c` if `test_testpo.c` supersedes it, or (b) keep the file unbuilt with a comment. Prefer (a); record the decision in the commit message.
- [ ] **Step 6: Run full suite.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: PASS including `jpl_testpo_441` (validates real DE441 geometric states to ~5e-13).
- [ ] **Step 7: Commit.**
```bash
git add tests/test_testpo.c CMakeLists.txt
git commit -m "test: validate geometric states against JPL testpo reference data"
```
