# JPL Body-Numbering Renumber Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Renumber the public `jpl_target` enum so Earth=3 and Earth-Moon barycenter=13, matching JPL PLEPH/testpo convention.

**Architecture:** Swap the two enum values, decouple file-slot indexing from body codes with one named constant, fix the two range bounds that reference the moved symbols, recenter `jpl_moon_position` on `JPL_EARTH`, and delete the 3↔13 translation in the testpo harness so it calls body codes directly. No math changes; the full testpo suite (24 DEs, ~1M lines) is the proof.

**Tech Stack:** C17 (`c_std_17`, `C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`, vendored ERFA 2.0.1 (read-only).

**Spec:** `D:\ephem\jpl\fortran\testeph.f`, `SUBROUTINE PLEPH` header comment — the numbering authority: 1=Mercury, 2=Venus, 3=Earth, 4=Mars, 5=Jupiter, 6=Saturn, 7=Uranus, 8=Neptune, 9=Pluto, 10=Moon, 11=Sun, 12=SSB, 13=Earth-Moon barycenter, 14=Nutations, 15=Librations. The plan argues from this; executors read that header block before coding.

## Global Constraints

- C17 with `C_EXTENSIONS OFF`; MSVC `/W4 /permissive-` zero warnings; GCC/Clang `-Wall -Wextra -Wpedantic` zero warnings (lib AND tests).
- Physics constants live in `src/jpl_private.h`, never as literals in `src/*.c`.
- `src/erfa/` is read-only vendored code; never modify.
- Public headers accumulate Doxygen (`@brief/@param/@return/@note`); `WARN_AS_ERROR=YES` must stay clean.
- Agent-run shell commands are PowerShell 5.1: chain with `;` (no `&&`/`||`), quote paths with spaces, use `workdir` instead of `cd`.
- Commit messages follow Conventional Commits with subject + body: `type(scope): imperative subject` (max 72 chars, lowercase after colon, no trailing period), blank line, body explaining what changed and why wrapped at 72 chars, `BREAKING CHANGE:` footer when applicable. Never commit with a bare one-line subject.
- JPLEPH=`D:\ephem\jpl\bin` and testpo at `D:\ephem\jpl\testpo` (`JPL_TESTPO` env override supported); verify with `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure` (expect 32/32), plus a WSL Linux Release build+test for the POSIX branch.
- Historical docs (`docs/superpowers/specs/*`, `docs/superpowers/plans/*`) are records: do NOT rewrite them. Only live docs (headers, README) change.

---

## File map

- `include/jpl.h` — swap `JPL_EARTH`↔`JPL_EARTH_MOON_BARYCENTER` values, delete the transposition `@note` (it becomes false), correct the `(3)`/`(13)` Doxygen tags.
- `src/jpl_private.h` — gains `JPL_FILE_SLOT_EMB 2` (0-based file-slot index of the EMB coefficients in `header.pointers`, which no longer equals any body-code-minus-one).
- `src/jpl.c` — `barycentric_state()` excludes Earth from the direct-slot path and uses the slot macro; `jpl_get_state_lighttime()` upper bound becomes `JPL_EARTH_MOON_BARYCENTER`.
- `src/jpl_convenience.c` — `jpl_moon_position()` centers on `JPL_EARTH` via symbols (behavioral fix); numeric planet/center range checks are UNCHANGED (same numeric set `{1..11,13}`).
- `include/jpl_convenience.h` — body-code numbers in `jpl_planet_position` docs rewritten (Earth 3, EMB 13).
- `tests/test_testpo.c` — delete the 3↔13 mapping, call `jpl_get_state` with testpo codes directly, rewrite the translation comment (keep the nutation-rate paragraph).
- Untouched (verify by grep, do not edit): `src/jpl_frames.c`, `src/erfa/*`, all `test_put_ptr` slot indices (file slots, not body codes), README (symbols only), historical spec/plan docs.

---

### Task 1: Renumber enum, core paths, moon center, testpo direct call

**Files:**
- Modify: `include/jpl.h` (enum), `src/jpl_private.h` (slot macro), `src/jpl.c` (`barycentric_state`, light-time bounds), `src/jpl_convenience.c` (`jpl_moon_position`), `tests/test_testpo.c` (unswap)
- Test: full `ctest` (6 unit + 2 integration + 24 testpo)

**Interfaces:**
- Consumes: PLEPH numbering from `testeph.f` (3=Earth, 13=EMB).
- Produces: `JPL_EARTH == 3`, `JPL_EARTH_MOON_BARYCENTER == 13`; `JPL_FILE_SLOT_EMB == 2` in `src/jpl_private.h`; testpo codes passed through unmapped.

**Steps:**

- [ ] **Step 1: Read the authority.** Read the `SUBROUTINE PLEPH` numbering comment in `D:\ephem\jpl\fortran\testeph.f` (search for `3 = EARTH`). Confirm 3=Earth, 13=Earth-Moon barycenter before touching code.

- [ ] **Step 2: Swap the enum in `include/jpl.h`.** Replace lines 51-75 with:
```c
/**
 * @brief JPL target identifiers used by jpl_get_state().
 * @note Numbering follows the JPL PLEPH/testpo convention (see the
 *       SUBROUTINE PLEPH header in JPL testeph.f): 3 is Earth and
 *       13 is the Earth-Moon barycenter.
 */
typedef enum jpl_target {
    JPL_MERCURY = 1,             /**< @brief Mercury (1). */
    JPL_VENUS,                   /**< @brief Venus (2). */
    JPL_EARTH,                   /**< @brief Earth (3), derived from barycenter and geocentric Moon. */
    JPL_MARS,                    /**< @brief Mars (4). */
    JPL_JUPITER,                 /**< @brief Jupiter (5). */
    JPL_SATURN,                  /**< @brief Saturn (6). */
    JPL_URANUS,                  /**< @brief Uranus (7). */
    JPL_NEPTUNE,                 /**< @brief Neptune (8). */
    JPL_PLUTO,                   /**< @brief Pluto (9). */
    JPL_MOON,                    /**< @brief Moon (10), derived from barycenter and geocentric Moon. */
    JPL_SUN,                     /**< @brief Sun (11). */
    JPL_SOLAR_SYSTEM_BARYCENTER, /**< @brief Solar-system barycenter (12). */
    JPL_EARTH_MOON_BARYCENTER = 13, /**< @brief Earth-Moon barycenter (13). */
    JPL_NUTATIONS,               /**< @brief Nutations (14): longitude and obliquity, radians. */
    JPL_LIBRATIONS,              /**< @brief Librations (15): three lunar libration angles, radians. */
    JPL_SSB = 0                  /**< @brief Center alias: 0 means the solar-system barycenter. */
} jpl_target;
```
The old transposition `@note` is deleted (it would now be false); the new note cites the authority. No other values change: 14/15/0 keep their auto-incremented values (13+1=14, 14+1=15).

- [ ] **Step 3: Add the file-slot macro to `src/jpl_private.h`.** After the `JPL_DJM0` block add:
```c
/* File-slot index (0-based) of the EMB coefficients in header.pointers.
 * File slots follow coefficient order (Mercury..Sun, EMB third), while body
 * codes follow PLEPH (Earth 3, EMB 13): the two coincide for every body
 * except Earth/EMB, so the EMB slot is named explicitly. */
#define JPL_FILE_SLOT_EMB 2
```

- [ ] **Step 4: Rework `barycentric_state()` in `src/jpl.c`.** The old direct path `pointers[target - 1]` assumed body code == file slot + 1, which breaks for Earth (3, whose slot holds EMB data) and EMB (13, off the end). Replace the function body with:
```c
static jpl_status barycentric_state(const jpl_ephemeris *e, double jd, int target, double state[6]) {
    double moon[6]; size_t i; jpl_status status;
    if (target == JPL_SOLAR_SYSTEM_BARYCENTER) { memset(state, 0, 6 * sizeof *state); return JPL_OK; }
    /* Direct file slots cover Mercury..Sun except Earth (3, derived below)
     * and the Moon (10, needs the EMB leg too). */
    if (target >= JPL_MERCURY && target <= JPL_SUN && target != JPL_MOON && target != JPL_EARTH)
        return interpolate(e, jd, &e->header.pointers[target - 1], 3, state);
    if (target != JPL_EARTH && target != JPL_MOON && target != JPL_EARTH_MOON_BARYCENTER) return JPL_ERROR_ARGUMENT;
    status = interpolate(e, jd, &e->header.pointers[JPL_FILE_SLOT_EMB], 3, state);
    if (status != JPL_OK) return status;
    status = interpolate(e, jd, &e->header.pointers[JPL_MOON - 1], 3, moon);
    if (status != JPL_OK) return status;
    for (i = 0; i < 6; ++i) state[i] -= moon[i] / (1.0 + e->header.earth_moon_mass_ratio);
    if (target == JPL_MOON) for (i = 0; i < 6; ++i) state[i] += moon[i];
    return JPL_OK;
}
```
`JPL_MOON - 1` (= 9) is still the correct Moon file slot; only EMB needed the macro.

- [ ] **Step 5: Fix the light-time bounds in `src/jpl.c`.** `JPL_EARTH` is now 3, so `target > JPL_EARTH` would reject bodies 4-13. Replace:
```c
    if (target < 0 || target > JPL_EARTH || center < 0 || center > JPL_EARTH) return JPL_ERROR_ARGUMENT;
```
with:
```c
    /* Bodies 0-13; JPL_EARTH_MOON_BARYCENTER (13) is the highest body code. */
    if (target < 0 || target > JPL_EARTH_MOON_BARYCENTER || center < 0 || center > JPL_EARTH_MOON_BARYCENTER) return JPL_ERROR_ARGUMENT;
```

- [ ] **Step 6: Recenter `jpl_moon_position()` in `src/jpl_convenience.c`.** Center 13 was Earth and is now EMB. Replace:
```c
    return jpl_planet_position(e, jd_tt, 10, 13, st);
```
with:
```c
    return jpl_planet_position(e, jd_tt, JPL_MOON, JPL_EARTH, st);
```
Symbols (not raw numbers) so this class of bug cannot recur. Do NOT touch the numeric planet/center range checks on lines 21/23: the accepted numeric set `{1..11, 13}` is identical before and after (only the meaning of 3 and 13 changed).

- [ ] **Step 7: Delete the testpo 3↔13 swap in `tests/test_testpo.c`.** Replace the translation comment (lines 132-137) with:
```c
        /* Body codes match the JPL PLEPH/testpo convention directly (Earth 3,
         * EMB 13), so target/center pass through to jpl_get_state unmapped.
         * SSB (12), nutations (14), librations (15) likewise need no mapping.
```
Delete `mapped_target`/`mapped_center`; use `target`/`center` in the range check, the `jpl_get_state` call, and the `target == 14 || target == 15` skip classification. Keep the nutation-rate skip paragraph and all tolerance logic untouched.

- [ ] **Step 8: Sweep for raw 3/13 literals.** Grep `src/` and `tests/` for `, 3,` / `, 13` / `== 13` / `== 3` outside file-slot helpers (`test_put_ptr` indices are file slots — leave them). Every hit must use a `JPL_*` symbol or be justified in the commit message. Convert any stragglers to symbols.

- [ ] **Step 9: Run the full suite.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: 32/32 PASS (6 unit + 2 integration + 24 testpo with NO swap — this is the proof the renumber is correct). Any MISMATCH line is a real failure, not a tolerance issue: stop and diagnose, do not loosen tolerances.

- [ ] **Step 10: Commit (Conventional Commits, subject + body).**
```bash
git add include/jpl.h src/jpl_private.h src/jpl.c src/jpl_convenience.c tests/test_testpo.c
git commit -m "refactor(api): renumber Earth/EMB body codes to JPL PLEPH convention" -m "Swap JPL_EARTH to 3 and JPL_EARTH_MOON_BARYCENTER to 13 so the public enum matches JPL PLEPH/testpo numbering (see testeph.f). The library is pre-release and private, so no compatibility shim is kept." -m "Map the EMB coefficient file slot explicitly (JPL_FILE_SLOT_EMB) since body codes and file slots no longer coincide, fix the light-time body range bound, center jpl_moon_position on JPL_EARTH, and call testpo body codes directly (drop the 3<->13 swap in the harness)." -m "BREAKING CHANGE: JPL_EARTH is now 3 and JPL_EARTH_MOON_BARYCENTER is now 13; any caller using raw integers 3 or 13 must swap them. Symbolic callers are unaffected."
```

### Task 2: Comment accuracy sweep + cross-platform verification

**Files:**
- Modify: `include/jpl_convenience.h` (body-code numbers), `src/jpl_convenience.c` (comments only)
- Test: full `ctest` Windows Debug + WSL Linux Release (with testpo)

**Interfaces:**
- Consumes: Task 1's renumber (symbols now resolve to JPL codes).
- Produces: no stale code numbers in live docs; green suites on both platforms.

**Steps:**

- [ ] **Step 1: Rewrite body-code numbers in `include/jpl_convenience.h`.** Replace the `jpl_planet_position` doc lines:
```
 * @param planet Target body: 1-11 or 13 (planets 1-9, EMB 3, Moon 10,
 *        Sun 11, Earth 13; rejects SSB 12, nutations 14, librations 15,
 *        and out-of-range values).
```
with:
```
 * @param planet Target body: 1-11 or 13 (planets incl. Earth 3, Moon 10,
 *        Sun 11, EMB 13; rejects SSB 12, nutations 14, librations 15,
 *        and out-of-range values).
```
Leave `jpl_moon_position` ("Earth-centered") as-is: still true.

- [ ] **Step 2: Fix `src/jpl_convenience.c` comments (code untouched).** Replace lines 19-20:
```c
    /* Planets 1-9, Moon 10, Sun 11, Earth 13. Reject SSB 12 and the
     * nutation (14) / libration (15) sensitivities, 0, and out-of-range. */
```
with:
```c
    /* Targets 1-11 (Mercury..Sun, incl. Earth 3) or EMB 13. Reject SSB 12
     * and the nutation (14) / libration (15) sensitivities, 0, and
     * out-of-range. */
```

- [ ] **Step 3: Grep live docs for stale numbers.** Search `README.md`, `AGENTS.md`, `include/*.h` for `EMB 3`, `Earth 13`, `3 = barycenter`, `13 = Earth`, `3<->13`, `swap 3`. Every live hit gets corrected; historical `docs/superpowers/*` hits are records and stay. Report the grep output (hits + disposition) in your report.

- [ ] **Step 4: Windows verification.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`
Expected: 32/32 PASS, zero MSVC warnings.

- [ ] **Step 5: Linux verification (WSL).** Fresh dir `build-wsl` (MUST delete after — not gitignored):
```
JPLEPH=/mnt/d/ephem/jpl/bin JPL_TESTPO=/mnt/d/ephem/jpl/testpo cmake -S /mnt/d/src/libjpl -B /mnt/d/src/libjpl/build-wsl -DCMAKE_BUILD_TYPE=Release -DLIBJPL_BUILD_TESTS=ON
cmake --build /mnt/d/src/libjpl/build-wsl
JPLEPH=/mnt/d/ephem/jpl/bin ctest --test-dir /mnt/d/src/libjpl/build-wsl --output-on-failure
```
Expected: all suites PASS (8 base + 24 testpo), ZERO gcc warnings (grep the build log for `warning`). Then `rm -rf /mnt/d/src/libjpl/build-wsl` and confirm `git status --short` clean.

- [ ] **Step 6: Commit (Conventional Commits, subject + body).**
```bash
git add include/jpl_convenience.h src/jpl_convenience.c
git commit -m "docs(api): update body-code comments for Earth/EMB renumber" -m "Rewrite code-number comments in the convenience header and source to the JPL PLEPH numbering (Earth 3, EMB 13). No code change; verified green on Windows/MSVC and Linux/GCC including the testpo reference suites."
```

---

## Execution order

Task 1 then Task 2, strictly sequential (same files, Task 2 documents Task 1's semantics). Final gate: Windows 32/32 + Linux all-green with zero warnings on both, `git status` clean, each commit carrying subject + body per Global Constraints.
