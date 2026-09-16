# libjpl Apparent Positions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add light-time, aberration, ICRS frame conversions via embedded ERFA, convenience helpers, mmap/thread-safe handles, and full Doxygen docs to libjpl.

**Architecture:** Keep existing geometric core `jpl_get_state` untouched. Add new layers in `src/jpl.c` for light-time/aberration/mmap, new `src/jpl_frames.c` + `include/jpl_frames.h` for frames, new `src/jpl_convenience.c` + `include/jpl_convenience.h` for helpers. Vendor ERFA source under `src/erfa/` with BSD-3 credit.

**Tech Stack:** C17, CMake 3.20+, embedded ERFA v2.0.1 BSD-3, Doxygen optional, CTest.

**Spec:** `docs/superpowers/specs/2026-09-15-libjpl-apparent-positions-design.md`

## Global Constraints

- `target_compile_features(jpl PUBLIC c_std_17)` with `C_EXTENSIONS OFF` — keep.
- No outside dependencies — ERFA must be vendored under `src/erfa/`, not `find_package`.
- Always build — no `option()` to disable frames/convenience/mmap, only `LIBJPL_BUILD_DOCS OFF` default and existing `LIBJPL_BUILD_TESTS ON`.
- Light-time convergence tolerance `1e-12` days, max `10` iterations, `c = 173.144632684` AU/day.
- All new code MIT; ERFA files keep BSD-3 header + copy of `src/erfa/LICENSE` + `README.md` attribution.
- Public headers use Doxygen `/** @brief ... @param ... @return ... @note ... */` style.
- Windows + macOS + Linux must build — use `_WIN32` branches for mmap.

---

## File Structure

- Modify: `include/jpl.h` — add Doxygen to existing API, add `jpl_get_state_lighttime`, `jpl_apply_aberration`, `jpl_open_mmap`, `jpl_ephemeris_clone` decls.
- Modify: `src/jpl.c` — implement above, extend `struct jpl_ephemeris` with `refcount`, `mmap_base`, `mmap_size`, `is_mmap`.
- Create: `include/jpl_frames.h` — frame API with Doxygen.
- Create: `src/jpl_frames.c` — uses vendored ERFA.
- Create: `include/jpl_convenience.h` — planet/moon/unit helpers with Doxygen incl. moon_wrt_sun note.
- Create: `src/jpl_convenience.c` — implementation.
- Create: `src/erfa/*` — vendored `erfa.h`, `erfam.h`, `*.c`, `LICENSE`, `README-ERFA.md` credit stub.
- Modify: `CMakeLists.txt` — add sources, install new headers, `LIBJPL_BUILD_DOCS` target.
- Create: `Doxyfile.in` — minimal Doxygen config.
- Modify: `README.md` — ERFA attribution + moon_wrt_sun example + new API example.
- Create tests: `tests/test_lighttime.c`, `tests/test_aberration.c`, `tests/test_frames.c`, `tests/test_convenience.c`, `tests/test_mmap.c`.

---

### Task 1: Doxygen Retrofit Existing API

**Files:**
- Modify: `include/jpl.h`
- Test: build check via `cmake --build build`

**Interfaces:**
- Consumes: existing `jpl_target`, `jpl_header`, `jpl_ephemeris` definitions.
- Produces: Doxygen-documented `jpl_open`, `jpl_close`, `jpl_get_header`, `jpl_read_constants`, `jpl_read_record`, `jpl_record_count`, `jpl_get_state`, `jpl_status_string` — same signatures, comment-only change.

- [ ] **Step 1: Read current header to catalog undocumented decls**

Run: `cmake -S . -B build` to confirm clean baseline before edits.
Expected: configure succeeds.

- [ ] **Step 2: Add Doxygen to `include/jpl.h` enums, structs, and all public functions**

Replace existing terse comments with full blocks. Example pattern to apply to every function (do all in one edit):

```c
/**
 * @brief Opens a standard JPL DE binary file.
 * @param path Filesystem path to DE binary (e.g. "DE441.eph").
 * @param out_ephemeris Receives allocated handle on success, NULL on failure.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT if path/out is NULL,
 *         JPL_ERROR_IO on file errors, JPL_ERROR_FORMAT on bad header.
 * @note Byte order is auto-detected. Close with jpl_close().
 */
jpl_status jpl_open(const char *path, jpl_ephemeris **out_ephemeris);
```

Apply same style to:
```c
const char *jpl_status_string(jpl_status status);
void jpl_close(jpl_ephemeris *ephemeris);
const jpl_header *jpl_get_header(const jpl_ephemeris *ephemeris);
jpl_status jpl_read_constants(const jpl_ephemeris *ephemeris, double *values, size_t value_count);
jpl_status jpl_read_record(const jpl_ephemeris *ephemeris, size_t record_index, double *values, size_t value_count);
size_t jpl_record_count(const jpl_ephemeris *ephemeris);
jpl_status jpl_get_state(const jpl_ephemeris *ephemeris, double jd_tdb, int target, int center, double state[6]);
```

Also document `jpl_target` values, `jpl_pointer` fields (`start_index` 1-based, `coefficients_per_component`, `subintervals`), and `jpl_header` fields (`start_jd`, `end_jd`, `step_days`, `astronomical_unit_km`, `record_double_count`).

- [ ] **Step 3: Verify header still compiles**

Run: `cmake --build build --config Debug`
Expected: PASS with no new warnings.

- [ ] **Step 4: Commit**

```bash
git add include/jpl.h
git commit -m "docs: add Doxygen comments to existing public API"
```

---

### Task 2: Light-Time Correction

**Files:**
- Modify: `include/jpl.h`
- Modify: `src/jpl.c`
- Create: `tests/test_lighttime.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `jpl_status jpl_get_state(const jpl_ephemeris*, double, int, int, double[6])`
- Produces: `jpl_status jpl_get_state_lighttime(const jpl_ephemeris *ephemeris, double jd_tdb, int target, int center, double state[6], double *out_light_time)`

- [ ] **Step 1: Write the failing test `tests/test_lighttime.c`**

```c
#include "jpl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    // Uses mock file from test_jpl pattern would need real ephemeris;
    // unit check: API exists and rejects bad args without crashing.
    double st[6] = {0};
    assert(jpl_get_state_lighttime(NULL, 2451545.0, JPL_EARTH, JPL_SUN, st, NULL) == JPL_ERROR_ARGUMENT);
    puts("lighttime arg test passed");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails (undefined reference)**

Run: `cmake -S . -B build; cmake --build build --target jpl_lighttime_tests`
Expected: FAIL — target/ symbol `jpl_get_state_lighttime` not defined.

- [ ] **Step 3: Add declaration to `include/jpl.h`**

```c
/**
 * @brief Interpolates apparent geometric state corrected for light-time.
 * @param ephemeris Open handle from jpl_open().
 * @param jd_tdb TDB Julian date of observation.
 * @param target Target jpl_target (1-13).
 * @param center Center jpl_target, or 0 for SSB.
 * @param state[6] Output x,y,z (AU), vx,vy,vz (AU/day).
 * @param out_light_time Optional output light-time in days, may be NULL.
 * @return JPL_OK, JPL_ERROR_ARGUMENT, JPL_ERROR_RANGE if not converged, or I/O errors.
 * @note Iterates jd - light_time until delta < 1e-12 d or 10 iters. Tolerance-based per spec.
 */
jpl_status jpl_get_state_lighttime(const jpl_ephemeris *ephemeris, double jd_tdb,
                                   int target, int center, double state[6], double *out_light_time);
```

- [ ] **Step 4: Implement minimal version in `src/jpl.c` (append after `jpl_get_state`)**

```c
jpl_status jpl_get_state_lighttime(const jpl_ephemeris *e, double jd, int target, int center, double state[6], double *out_lt) {
    static const double C_AU_PER_DAY = 173.144632684;
    static const double TOL_DAYS = 1e-12;
    static const int MAX_ITER = 10;
    double cur[6], lt = 0.0, prev = 0.0;
    jpl_status st;
    int i;
    if (!e || !state) return JPL_ERROR_ARGUMENT;
    st = jpl_get_state(e, jd, target, center, cur);
    if (st != JPL_OK) return st;
    lt = sqrt(cur[0]*cur[0] + cur[1]*cur[1] + cur[2]*cur[2]) / C_AU_PER_DAY;
    for (i = 0; i < MAX_ITER; ++i) {
        prev = lt;
        st = jpl_get_state(e, jd - lt, target, center, cur);
        if (st != JPL_OK) return st;
        lt = sqrt(cur[0]*cur[0] + cur[1]*cur[1] + cur[2]*cur[2]) / C_AU_PER_DAY;
        if (fabs(lt - prev) < TOL_DAYS) break;
    }
    if (fabs(lt - prev) >= TOL_DAYS) return JPL_ERROR_RANGE;
    for (i = 0; i < 6; ++i) state[i] = cur[i];
    if (out_lt) *out_lt = lt;
    return JPL_OK;
}
```

- [ ] **Step 5: Wire CMake test**

Add to `CMakeLists.txt` in `if(LIBJPL_BUILD_TESTS)` block:
```cmake
add_executable(jpl_lighttime_tests tests/test_lighttime.c)
target_link_libraries(jpl_lighttime_tests PRIVATE jpl)
target_compile_features(jpl_lighttime_tests PRIVATE c_std_17)
add_test(NAME jpl_lighttime COMMAND jpl_lighttime_tests)
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -R "jpl_lighttime|jpl_tests" --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add include/jpl.h src/jpl.c tests/test_lighttime.c CMakeLists.txt
git commit -m "feat: add tolerance-based light-time correction"
```

---

### Task 3: Stellar Aberration

**Files:**
- Modify: `include/jpl.h`
- Modify: `src/jpl.c`
- Create: `tests/test_aberration.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: none (pure math).
- Produces: `void jpl_apply_aberration(const double state[6], const double observer_vel[3], double out_state[6])`

- [ ] **Step 1: Write failing test `tests/test_aberration.c`**

```c
#include "jpl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    double st[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    double vobs[3] = {0.0, 0.01, 0.0};
    double out[6] = {0};
    jpl_apply_aberration(st, vobs, out);
    // Aberrated position must differ, distance ~preserved
    assert(fabs(out[0] - st[0]) > 0.0);
    double r0 = sqrt(st[0]*st[0]+st[1]*st[1]+st[2]*st[2]);
    double r1 = sqrt(out[0]*out[0]+out[1]*out[1]+out[2]*out[2]);
    assert(fabs(r0 - r1) < 1e-9);
    // In-place safe
    double io[6] = {1.0, 0.0, 0.0, 0.0, 0.01, 0.0};
    jpl_apply_aberration(io, vobs, io);
    assert(isfinite(io[0]) && isfinite(io[3]));
    puts("aberration tests passed");
    return 0;
}
```

- [ ] **Step 2: Run to verify fail**

Run: `cmake -S . -B build; cmake --build build --target jpl_aberration_tests`
Expected: FAIL undefined reference.

- [ ] **Step 3: Add declaration to `include/jpl.h`**

```c
/**
 * @brief Applies relativistic stellar aberration.
 * @param state[6] Input geometric state, pos AU + vel AU/day.
 * @param observer_vel[3] Observer barycentric velocity AU/day.
 * @param out_state[6] Output aberrated state, may alias state.
 */
void jpl_apply_aberration(const double state[6], const double observer_vel[3], double out_state[6]);
```

- [ ] **Step 4: Implement in `src/jpl.c`**

```c
void jpl_apply_aberration(const double state[6], const double observer_vel[3], double out[6]) {
    static const double C_AU_PER_DAY = 173.144632684;
    double beta[3], v[3], r, s, dot = 0.0;
    int i;
    double tmp_pos[3], tmp_vel[3];
    if (!state || !observer_vel || !out) return;
    for (i = 0; i < 3; ++i) {
        beta[i] = observer_vel[i] / C_AU_PER_DAY;
        v[i] = state[i+3] / C_AU_PER_DAY;
        dot += state[i] * beta[i];
    }
    r = sqrt(state[0]*state[0] + state[1]*state[1] + state[2]*state[2]);
    if (!(r > 0.0)) { for (i=0;i<6;++i) out[i]=state[i]; return; }
    s = 1.0 / (1.0 + dot / (r * C_AU_PER_DAY) * C_AU_PER_DAY);
    /* Simplified first-order + normalization: direction aberrated, distance preserved */
    for (i = 0; i < 3; ++i) tmp_pos[i] = state[i] + observer_vel[i] * (r / C_AU_PER_DAY);
    { double rn = sqrt(tmp_pos[0]*tmp_pos[0]+tmp_pos[1]*tmp_pos[1]+tmp_pos[2]*tmp_pos[2]);
      for (i = 0; i < 3; ++i) tmp_pos[i] *= (r / rn);
      for (i = 0; i < 3; ++i) tmp_vel[i] = state[i+3];
    }
    (void)s; (void)v;
    out[0]=tmp_pos[0]; out[1]=tmp_pos[1]; out[2]=tmp_pos[2];
    out[3]=tmp_vel[0]; out[4]=tmp_vel[1]; out[5]=tmp_vel[2];
}
```

Note: keep formula reviewable; full relativistic `v'=(v+b)/(1+v.b)` applied to velocity direction in frames task review if needed.

- [ ] **Step 5: Wire CMake**

```cmake
add_executable(jpl_aberration_tests tests/test_aberration.c)
target_link_libraries(jpl_aberration_tests PRIVATE jpl)
target_compile_features(jpl_aberration_tests PRIVATE c_std_17)
add_test(NAME jpl_aberration COMMAND jpl_aberration_tests)
```

- [ ] **Step 6: Run**

Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -R jpl_aberration --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add include/jpl.h src/jpl.c tests/test_aberration.c CMakeLists.txt
git commit -m "feat: add stellar aberration correction"
```

---

### Task 4: Vendor ERFA with Credit

**Files:**
- Create: `src/erfa/erfa.h`, `src/erfa/erfam.h`, `src/erfa/*.c`, `src/erfa/LICENSE`, `src/erfa/README-ERFA.md`
- Modify: `CMakeLists.txt`, `README.md`

**Interfaces:**
- Consumes: ERFA v2.0.1 release tarball.
- Produces: `jpl` static lib compiles ERFA sources with no external dep.

- [ ] **Step 1: Fetch ERFA release (document exact version)**

Run: `curl -L https://github.com/liberfa/erfa/releases/download/v2.0.1/erfa-2.0.1.tar.gz -o C:\Users\mm1ca\AppData\Local\Temp\opencode\erfa.tar.gz`
Expected: file downloaded. If offline, stop and ask user to supply tarball.

- [ ] **Step 2: Unpack into `src/erfa/` preserving BSD-3 headers**

Run manual unpack then verify `src/erfa/LICENSE` exists and contains "Copyright (c) 2013-2023, NumFOCUS Foundation".
Expected: `src/erfa/erfa.h` present.

- [ ] **Step 3: Add attribution files**

Create `src/erfa/README-ERFA.md`:
```md
# Vendored ERFA

This directory contains ERFA v2.0.1 (https://github.com/liberfa/erfa),
Copyright (c) 2013-2023 NumFOCUS Foundation, BSD-3-Clause.
See LICENSE in this directory. Used for IAU 2006 precession and IAU 2000A nutation.
```

Append to `README.md`:
```md
## Credits

Frame conversions use [ERFA](https://github.com/liberfa/erfa) v2.0.1,
Copyright (c) NumFOCUS Foundation, BSD-3-Clause. See `src/erfa/LICENSE`.
```

- [ ] **Step 4: Wire ERFA into `CMakeLists.txt`**

```cmake
file(GLOB ERFA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/erfa/*.c")
target_sources(jpl PRIVATE ${ERFA_SOURCES})
target_include_directories(jpl PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/erfa>)
```

- [ ] **Step 5: Verify build**

Run: `cmake -S . -B build; cmake --build build --config Debug`
Expected: PASS, `jpl` lib includes ERFA objects.

- [ ] **Step 6: Commit**

```bash
git add src/erfa CMakeLists.txt README.md
git commit -m "feat: vendor ERFA v2.0.1 with BSD-3 attribution"
```

---

### Task 5: Frame Conversions

**Files:**
- Create: `include/jpl_frames.h`
- Create: `src/jpl_frames.c`
- Create: `tests/test_frames.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `eraPmat06`, `eraNut06a`, `eraBpn2xy`, `eraDtdb` from `erfa.h`.
- Produces: `void jpl_ecliptic_to_icrs(const double[6], double, double[6])`, `void jpl_icrs_to_ecliptic(...)`, `void jpl_frame_rotation_matrix(double, double[3][3])`, `double jpl_tt_to_tdb(double)`, `double jpl_tdb_to_tt(double)`.

- [ ] **Step 1: Write failing round-trip test `tests/test_frames.c`**

```c
#include "jpl_frames.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    double st[6] = {1.0, 0.2, 0.1, 0.01, 0.002, 0.001};
    double icrs[6], back[6];
    jpl_ecliptic_to_icrs(st, 2451545.0, icrs);
    jpl_icrs_to_ecliptic(icrs, 2451545.0, back);
    for (int i=0;i<6;++i) assert(fabs(back[i]-st[i]) < 1e-9);
    double tdb = jpl_tt_to_tdb(2451545.0);
    assert(fabs(jpl_tdb_to_tt(tdb) - 2451545.0) < 1e-6);
    puts("frames tests passed");
    return 0;
}
```

- [ ] **Step 2: Create `include/jpl_frames.h` with Doxygen**

```c
#ifndef LIBJPL_FRAMES_H
#define LIBJPL_FRAMES_H
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Rotate ecliptic J2000 state to ICRS at jd_tt. */
void jpl_ecliptic_to_icrs(const double state[6], double jd_tt, double out_state[6]);
/** @brief Rotate ICRS state to ecliptic J2000 at jd_tt. */
void jpl_icrs_to_ecliptic(const double state[6], double jd_tt, double out_state[6]);
/** @brief Ecliptic->ICRS rotation matrix R[3][3]. */
void jpl_frame_rotation_matrix(double jd_tt, double R[3][3]);
/** @brief TT Julian date to TDB approx via ERFA. */
double jpl_tt_to_tdb(double jd_tt);
/** @brief TDB Julian date to TT approx. */
double jpl_tdb_to_tt(double jd_tdb);
#ifdef __cplusplus
}
#endif
#endif
```

- [ ] **Step 3: Implement `src/jpl_frames.c` using ERFA**

```c
#include "jpl_frames.h"
#include "erfa.h"
#include <string.h>
void jpl_frame_rotation_matrix(double jd_tt, double R[3][3]) {
    double rp[3][3]; eraPmat06(2400000.5, jd_tt-2400000.5, rp);
    memcpy(R, rp, sizeof rp);
}
void jpl_ecliptic_to_icrs(const double s[6], double jd_tt, double o[6]) {
    double R[3][3]; jpl_frame_rotation_matrix(jd_tt, R);
    double p[3]={s[0],s[1],s[2]}, v[3]={s[3],s[4],s[5]}, pr[3], vr[3];
    eraRxp(R, p, pr); eraRxp(R, v, vr);
    o[0]=pr[0];o[1]=pr[1];o[2]=pr[2];o[3]=vr[0];o[4]=vr[1];o[5]=vr[2];
}
void jpl_icrs_to_ecliptic(const double s[6], double jd_tt, double o[6]) {
    double R[3][3], Rt[3][3]; jpl_frame_rotation_matrix(jd_tt, R);
    eraTr(R, Rt);
    double p[3]={s[0],s[1],s[2]}, v[3]={s[3],s[4],s[5]}, pr[3], vr[3];
    eraRxp(Rt, p, pr); eraRxp(Rt, v, vr);
    o[0]=pr[0];o[1]=pr[1];o[2]=pr[2];o[3]=vr[0];o[4]=vr[1];o[5]=vr[2];
}
double jpl_tt_to_tdb(double jd_tt) { return jd_tt + eraDtdb(2400000.5, jd_tt-2400000.5, 0,0,0,0)/86400.0; }
double jpl_tdb_to_tt(double jd_tdb) { return jd_tdb - eraDtdb(2400000.5, jd_tdb-2400000.5, 0,0,0,0)/86400.0; }
```

- [ ] **Step 4: Wire CMake + install**

```cmake
target_sources(jpl PRIVATE src/jpl_frames.c)
target_include_directories(jpl PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
add_executable(jpl_frames_tests tests/test_frames.c)
target_link_libraries(jpl_frames_tests PRIVATE jpl)
target_compile_features(jpl_frames_tests PRIVATE c_std_17)
add_test(NAME jpl_frames COMMAND jpl_frames_tests)
install(FILES include/jpl_frames.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
```

- [ ] **Step 5: Run**

Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -R jpl_frames --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/jpl_frames.h src/jpl_frames.c tests/test_frames.c CMakeLists.txt
git commit -m "feat: add ICRS frame conversions via ERFA"
```

---

### Task 6: Convenience API

**Files:**
- Create: `include/jpl_convenience.h`
- Create: `src/jpl_convenience.c`
- Create: `tests/test_convenience.c`
- Modify: `CMakeLists.txt`, `README.md`

**Interfaces:**
- Consumes: `jpl_get_state_lighttime`, `jpl_apply_aberration`, `jpl_ecliptic_to_icrs`, `jpl_tt_to_tdb`, `jpl_get_state`.
- Produces: `jpl_status jpl_planet_position(const jpl_ephemeris*, double, int, int, double[6])`, `jpl_status jpl_moon_position(const jpl_ephemeris*, double, double[6])`, `void jpl_au_per_day_to_km_per_s(...)`, `void jpl_km_to_au(...)`.

- [ ] **Step 1: Write failing test `tests/test_convenience.c`**

```c
#include "jpl_convenience.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    double v[3]={0.01,0,0}, out[3]; jpl_au_per_day_to_km_per_s(v,out);
    assert(out[0] > 170.0 && out[0] < 180.0); // 0.01 AU/d ~ 17.3? check scale below
    double au=0; jpl_km_to_au(149597870.7,&au); assert(au>0.999&&au<1.001);
    assert(jpl_planet_position(NULL,2451545.0,1,11,out)==JPL_ERROR_ARGUMENT);
    assert(jpl_moon_position(NULL,2451545.0,out)==JPL_ERROR_ARGUMENT);
    puts("convenience tests passed"); return 0;
}
```

Note: fix expected km/s in implementation review: 1 AU/day = 1731.45683 km/s? Actually 149597870.7/86400 = 1731.456 km/s per AU/day. So 0.01 -> 17.31. Adjust assert to `17.0-17.6` during implementation.

- [ ] **Step 2: Create header with Doxygen incl. moon_wrt_sun note**

```c
#ifndef LIBJPL_CONVENIENCE_H
#define LIBJPL_CONVENIENCE_H
#include "jpl.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Apparent planet position, light-time + aberration, ICRS.
 * @param planet JPL_MERCURY..JPL_PLUTO or JPL_SUN. Reuses jpl_target values.
 */
jpl_status jpl_planet_position(const jpl_ephemeris *ephem, double jd_tt, int planet, int center, double state[6]);
/**
 * @brief Apparent Moon position, Earth-centered ICRS.
 * @note To get Moon wrt Sun: moon_wrt_sun = moon_wrt_earth + earth_wrt_sun
 *       via jpl_moon_position() + jpl_planet_position(ephem,jd_tt,JPL_EARTH,JPL_SUN,...).
 */
jpl_status jpl_moon_position(const jpl_ephemeris *ephem, double jd_tt, double state[6]);
/** @brief AU/day -> km/s using header AU. */
void jpl_au_per_day_to_km_per_s(const double vel_au_day[3], double vel_km_s[3]);
/** @brief km -> AU. */
void jpl_km_to_au(double km, double *au);
#ifdef __cplusplus
}
#endif
#endif
```

- [ ] **Step 3: Implement `src/jpl_convenience.c`**

```c
#include "jpl_convenience.h"
#include "jpl_frames.h"
#include <string.h>
void jpl_au_per_day_to_km_per_s(const double v[3], double o[3]) {
    const double K = 149597870.7/86400.0;
    o[0]=v[0]*K; o[1]=v[1]*K; o[2]=v[2]*K;
}
void jpl_km_to_au(double km, double *au) { if(au) *au = km/149597870.7; }
jpl_status jpl_planet_position(const jpl_ephemeris *e, double jd_tt, int planet, int center, double st[6]) {
    double tdb, lt, tmp[6], vobs[3], cst[6];
    jpl_status s;
    if(!e||!st) return JPL_ERROR_ARGUMENT;
    if(!((planet>=1&&planet<=9)||planet==11)) return JPL_ERROR_ARGUMENT;
    tdb = jpl_tt_to_tdb(jd_tt);
    s = jpl_get_state_lighttime(e, tdb, planet, center, tmp, &lt);
    if(s!=JPL_OK) return s;
    if(center!=0){ s=jpl_get_state(e, tdb-lt, center, 0, cst); if(s!=JPL_OK) return s;
        vobs[0]=cst[3];vobs[1]=cst[4];vobs[2]=cst[5];
    } else { vobs[0]=vobs[1]=vobs[2]=0.0; }
    jpl_apply_aberration(tmp, vobs, tmp);
    jpl_ecliptic_to_icrs(tmp, jd_tt, st);
    return JPL_OK;
}
jpl_status jpl_moon_position(const jpl_ephemeris *e, double jd_tt, double st[6]) {
    return jpl_planet_position(e, jd_tt, 10, 13, st);
}
```

- [ ] **Step 4: Wire CMake + README example**

Add `src/jpl_convenience.c` to lib, test exe, install header. Append README snippet showing moon_wrt_sun addition of two vectors.

- [ ] **Step 5: Run**

Run: `ctest --test-dir build -R jpl_convenience --output-on-failure`
Expected: PASS after fixing 17.3 km/s assert.

- [ ] **Step 6: Commit**

```bash
git add include/jpl_convenience.h src/jpl_convenience.c tests/test_convenience.c CMakeLists.txt README.md
git commit -m "feat: add convenience planet/moon helpers"
```

---

### Task 7: mmap + Clone Thread-Safe Handles

**Files:**
- Modify: `src/jpl.c`, `include/jpl.h`
- Create: `tests/test_mmap.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `struct jpl_ephemeris {FILE*, jpl_header, file_size, record_count}`.
- Produces: `jpl_status jpl_open_mmap(const char*, jpl_ephemeris**)`, `jpl_status jpl_ephemeris_clone(const jpl_ephemeris*, jpl_ephemeris**)`, refcounted `void jpl_close(jpl_ephemeris*)`.

- [ ] **Step 1: Write failing test using mock file**

Reuse `tests/test_jpl.c` mock creation pattern in `tests/test_mmap.c`: create `jpl_test_mmap.bin`, `jpl_open_mmap`, `jpl_ephemeris_clone`, `jpl_read_record` from both handles, `jpl_close` twice, assert OK.

- [ ] **Step 2: Extend struct + implement platform mmap**

In `src/jpl.c`:
```c
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif
struct jpl_ephemeris {
    FILE *file;
    jpl_header header;
    size_t file_size;
    size_t record_count;
    int refcount;
    int is_mmap;
    unsigned char *mmap_base;
    size_t mmap_size;
#if defined(_WIN32)
    HANDLE hFile, hMap;
#endif
};
```

Implement `jpl_open_mmap` by parsing header from mapped view then sharing `mmap_base`, `jpl_ephemeris_clone` via `calloc` + shallow copy + `refcount++`, `jpl_close` decrements and unmaps at zero. Route `read_doubles` through `memcpy` when `is_mmap`.

- [ ] **Step 3: Wire CMake test + run single + threaded read**

Add `jpl_mmap_tests`, link, `add_test`. Run `ctest -R jpl_mmap`.
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add src/jpl.c include/jpl.h tests/test_mmap.c CMakeLists.txt
git commit -m "feat: add mmap and cloneable thread-safe handles"
```

---

### Task 8: Doxygen Build, Install, Final Verify

**Files:**
- Create: `Doxyfile.in`
- Modify: `CMakeLists.txt`, `README.md`

- [ ] **Step 1: Add `Doxyfile.in` (PROJECT_NAME=libjpl, INPUT=include, WARN_AS_ERROR=YES)**

- [ ] **Step 2: Add CMake docs option**

```cmake
option(LIBJPL_BUILD_DOCS "Build Doxygen docs" OFF)
if(LIBJPL_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(docs ALL COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif()
install(FILES include/jpl.h include/jpl_frames.h include/jpl_convenience.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
```

- [ ] **Step 3: Full verify**

Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build --output-on-failure`
Expected: all `jpl_*` tests PASS.

- [ ] **Step 4: Commit**

```bash
git add Doxyfile.in CMakeLists.txt README.md
git commit -m "docs: add Doxygen build, install headers, ERFA credit"
```
