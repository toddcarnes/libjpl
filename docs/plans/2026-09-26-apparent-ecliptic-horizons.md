<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Apparent ecliptic positions and Horizons references implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a one-call geocentric apparent tropical longitude/latitude/range query and independently verify it against pinned DE441 Horizons observer output.

**Architecture:** Share retarded-target and observer geometry privately in `jpl_convenience.c`, preserving the existing convenience function's correction policy. The new function adds finite-source solar deflection before aberration, then uses a pure IAU2006/2000A true-of-date rotation and spherical conversion. Store batch-acquired Horizons values and their provenance separately from offline numerical comparisons; verify target and center sources on every acquisition.

**Tech Stack:** C17, CMake/CTest, bundled ERFA v2.0.1, JPL DE441, Horizons file API; PowerShell 5.1 for commands.

**Spec:** `docs/specs/2026-09-26-apparent-ecliptic-horizons-design.md`

## Global Constraints

- Do not modify `src/erfa/`; use ERFA through internal library code only.
- Preserve TT/TDB as `double[2]` through date arithmetic and ERFA calls.
- Keep all production physics constants in `src/jpl_private.h`; preserve the existing `jpl_planet_position()` and `jpl_moon_position()` results, status precedence, and lazy Sun queries.
- The public output is geocentric IAU 2006 precession / IAU 2000A nutation **true ecliptic of date**: longitude `[0,360)` degrees, latitude degrees, range modern AU. Include Moon; reject Earth, SSB, EMB, and angular targets. Gate on `JPL_FRAME_ICRF`.
- Horizons observer availability is narrower than the local DE441 binary. Horizons quantities 45/31/20 respectively supply apparent ICRF RA/Dec, IAU76/80 ecliptic longitude/latitude, and apparent range. `COMMAND='499'` selects physical Mars from `mar099`; `COMMAND='4'` selects the DE441 Mars barycenter.
- One sequential Horizons file-API POST per target with a multi-date `TLIST`; every returned target and center must identify DE441. Never fetch during offline CTest. Report deep-time results separately from precision checks in the validated modern interval.
- Preserve existing uncommitted docs-landing-page and constant-name changes in the working tree. Do not stage or commit any files unless the user explicitly asks.
- Task and step identifiers belong only in this implementation plan. Do not copy them into public headers, Doxygen text, README, validation/provenance notes, or source and test comments. Document scientific contracts, file formats, and verified behavior directly.
- Compile and test with MSVC Debug/Release; Doxygen `WARN_AS_ERROR=YES`. Tests need no Python or network.

## File map

- `src/jpl_convenience.c`: private shared light-time/observer geometry, unchanged existing convenience path, new deflected apparent-position path.
- `src/jpl_frames.c` and `src/jpl_private.h`: pure internal true-of-date vector rotation and two-part TT use; public mean-of-date frame APIs remain untouched.
- `include/jpl_convenience.h`: append the documented new public query.
- `tests/test_convenience.c` and `tests/test_frames.c`: regression, synthetic, pure-frame, and error-contract tests.
- `tests/data/horizons_de441.txt` and `tests/data/PROVENANCE.md`: numeric reference rows and how each target/batch was acquired. `tests/data/MANIFEST.txt` pins the canonical text hash. `tests/license_inventory.cmake` accounts for the external reference file.
- `tests/test_horizons.c`: offline fixture reader and direction/range comparator; accepts the committed DE441 slice or the full DE441 file, and selects only rows within the open file's coverage.
- `tests/cmake/check_horizons_reference.cmake`: verifies pinned rows, count, recorded source labels, hash, and a negative control on a disposable copy; actual upstream source checks occur during acquisition.
- `CMakeLists.txt`: offline and local-only test registration. `docs/api.dox`, `tests/public_symbols.txt`, `tests/examples.c`, and `tests/consumer/main.c`: discoverability and installed C consumer.

---

### Task 1: Preserve existing convenience outputs while sharing geometry

**Files:** Modify `src/jpl_convenience.c`; use existing `tests/test_convenience.c` as the regression test.

**Interfaces:** Consumes `jpl_get_state_lighttime(e, tdb, target, center, state, &delay)` and `jpl_get_state(e, tdb, center, 0, observer_state)`. Produces one private helper for reception-epoch observer data, retarded target state, and the solved light-time; Task 3 uses it to evaluate source/Sun geometry at emission. No public signature changes.

- [ ] **Step 1: Inspect the existing behavior-pinning tests.** `tests/test_convenience.c:144-208` checks reception-epoch aberration, `:252-383` covers frame gating, identical-body and SSB no-Sun shortcuts, and `:453-502` compares against the independently pinned apparent-state reference. Keep these tests intact; don't add a second test that merely repeats their computations.

- [ ] **Step 2: Run the pre-refactor test.** `cmake --build build/docs-audit-merge --config Debug --target jpl_convenience_tests; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug -R '^jpl_convenience$' --output-on-failure }`. Expected: pass; this is the baseline for a behavior-preserving refactor.
- [ ] **Step 3: Extract only the common geometry for nontrivial center/target pairs.** Keep the existing input validation, TT-to-TDB conversion, coverage-then-frame ordering, and same-body/SSB early returns at their current call sites. Factor the shared sequence in `src/jpl_convenience.c` into a private helper shaped like:

  ```c
  static jpl_status observer_geometry(const jpl_ephemeris *e,
      const double tdb[2], int target, int center,
      double retarded[6], double observer[6], double sun_from_observer[6],
      double *light_time_days) {
      jpl_status s = jpl_get_state_lighttime(e, tdb, target, center,
                                              retarded, light_time_days);
      if (s != JPL_OK) return s;
      s = jpl_get_state(e, tdb, center, JPL_SSB, observer);
      if (s != JPL_OK) return s;
      return jpl_get_state(e, tdb, center, JPL_SUN, sun_from_observer);
  }
  ```

  Only call this helper where the original code already read all three legs; preserve the Sun-observer and SSB branches that skip the unnecessary leg. Do not change `jpl_apply_aberration_full()` calls or status precedence. The new query uses `*light_time_days` to form emission TDB with `jpl_date_add_days` and to request `jpl_get_state(e, emission_tdb, target, JPL_SUN, sun_to_target)` for ERFA's finite-source `q`; the existing function does not make that extra read.
- [ ] **Step 4: Rebuild and run the same test plus `jpl_lighttime` and `jpl_frames`.** `cmake --build build/docs-audit-merge --config Debug --target jpl_convenience_tests jpl_lighttime_tests jpl_frames_tests; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug -R '^(jpl_convenience|jpl_lighttime|jpl_frames)$' --output-on-failure }`. Expected: all pass with no new warnings. Check `git diff` for accidental user-work changes.

### Task 2: Add a private true-of-date rotation

**Files:** Modify `src/jpl_frames.c`, `src/jpl_private.h`, `tests/test_frames.c`, `CMakeLists.txt` (test include path).

**Interfaces:** Produces `jpl_status jpl_icrs_to_true_ecliptic_position(const double in[3], const double jd_tt[2], double out[3]);` in the *private* header. Consumes only the input ICRS vector and TT pair; Task 3 uses the rotated vector.

- [ ] **Step 1: Write failing fixed-date and error-contract tests.** In `tests/test_frames.c`, include `jpl_private.h` and add `${CMAKE_CURRENT_SOURCE_DIR}/src` to `jpl_frames_tests`'s private include directories in `CMakeLists.txt`. Pass `in={0,0,1}` and `tt={2451545.0,0}` to the new private helper, and compare the output against an independently composed ERFA matrix `eraPnm06a` followed by `eraRx(eraObl06 + deps)` where `deps` comes from `eraNut06a`. A zero vector rotates to a zero vector; nonfinite input is `JPL_ERROR_ARGUMENT` with unchanged output:

  ```c
  double out[3] = {7, 8, 9};
  const double tt[2] = {2451545.0, 0.0};
  JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(NULL, tt, out)
                 == JPL_ERROR_ARGUMENT);
  JPL_TEST_CHECK(out[0] == 7 && out[1] == 8 && out[2] == 9);
  const double invalid[3] = {INFINITY, 0.0, 0.0};
  JPL_TEST_CHECK(jpl_icrs_to_true_ecliptic_position(invalid, tt, out)
                 == JPL_ERROR_ARGUMENT);
  JPL_TEST_CHECK(out[0] == 7 && out[1] == 8 && out[2] == 9);
  ```

- [ ] **Step 2: Build to see a missing-symbol failure.** `cmake --build build/docs-audit-merge --config Debug --target jpl_frames_tests`. Expected: missing declaration or unresolved `jpl_icrs_to_true_ecliptic_position`, not a malformed test fixture.
- [ ] **Step 3: Implement the pure helper with a normalized two-part TT pair and transactional output.** Use `jpl_date_normalize(jd_tt, tt)`; then `eraPnm06a(tt[0],tt[1],m)`, `eraNut06a(tt[0],tt[1],&dpsi,&deps)`, `eps=eraObl06(tt[0],tt[1])+deps`, `eraRx(eps,m)`, and `eraRxp(m,in,local)`. Check finite inputs, matrix/output, and write to `out` only after success. Keep `jpl_icrs_to_ecliptic()` and `jpl_frame_rotation_matrix()` on their existing mean-of-date convention.
- [ ] **Step 4: Rebuild/run `jpl_frames` and `jpl_numeric`.** `cmake --build build/docs-audit-merge --config Debug --target jpl_frames_tests jpl_numeric_tests; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug -R '^(jpl_frames|jpl_numeric)$' --output-on-failure }`. Expected: both pass; a two-part TT pair with a nonzero low part must reach ERFA intact.

### Task 3: Deflected apparent directions and public one-stop API

**Files:** Modify `src/jpl_convenience.c`, `include/jpl_convenience.h`, `tests/test_convenience.c`, `tests/examples.c`, `tests/consumer/main.c`, `tests/public_symbols.txt`, `docs/api.dox`.

**Interfaces:** Consumes Task 1 geometry and Task 2 `jpl_icrs_to_true_ecliptic_position`. Produces `jpl_status jpl_apparent_ecliptic_position(const jpl_ephemeris*, const double jd_tt[2], int target, double lon_lat_range[3]);`. Returns degrees/degrees/AU, output unchanged on failure.

- [ ] **Step 1: Add failing public tests before the declaration/implementation.** Extend the existing DE441 mock in `tests/test_convenience.c` with Moon `target=JPL_MOON` and Sun `target=JPL_SUN` cases using `jd={2451545.5,0}`. Check positive range and finite, bounded angles; `JPL_EARTH`, `JPL_SSB`, `JPL_EARTH_MOON_BARYCENTER`, `JPL_NUTATIONS`, `JPL_LIBRATIONS`, and NULL inputs return `JPL_ERROR_ARGUMENT` with `out={7,8,9}` unchanged. Verify an unknown-frame mock returns `JPL_ERROR_UNSUPPORTED` and an out-of-coverage date returns `JPL_ERROR_RANGE` first. For a pure x-axis mock with zero observer velocity, check range against `384400.0/JPL_TEST_AU_KM` for the Moon and compare orientation with Task 2's pure rotation. Add a second noncollinear mock with Sun at `{0,0,0}` AU, Earth at `{1,0,0}` AU and a stationary target at `{-0.5,0.01,0}` AU; compare a normalized deflected direction against the no-deflection reference while holding aberration at zero.
- [ ] **Step 2: Watch `jpl_convenience` fail on the absent API.** `cmake --build build/docs-audit-merge --config Debug --target jpl_convenience_tests`. Expected: new function unavailable.
- [ ] **Step 3: Append a complete Doxygen declaration.** Add `@brief`, all `@param`, `@return`, and `@note` for the new `double lon_lat_range[3]` API. Specify the exact accepted target set, TT pair, correction order, IAU2006/2000A true-of-date, modern AU, `[0,360)` longitude, geocenter, ICRF gate, error precedence, and transactional output. Add the name to `tests/public_symbols.txt`'s function section and the grouped `docs/api.dox` page.
- [ ] **Step 4: Implement the new path.** Reuse date/frame validation and Task 1's retarded geometry. Form `emission_tdb` with `jpl_date_add_days(tdb, -light_time_days, emission_tdb)` and read `sun_to_target` there. For any target other than the Sun, normalize the observer-target (`p`), Sun-target-at-emission (`q`), and Sun-observer-at-reception (`e`) vectors. Use `JPL_SOLAR_RADIUS_KM=695700.0` (Horizons-reported solar radius) in `src/jpl_private.h` and compute a solar-limb limiter at Sun-observer distance `em`:

  ```c
  double sun_radius_au = JPL_SOLAR_RADIUS_KM / JPL_AU_KM;
  double limb = atan2(sun_radius_au, em);
  double dlim = 0.5 * limb * limb;
  if (!(em > 0.0) || !isfinite(dlim) || !(dlim > 0.0))
      return JPL_ERROR_NUMERIC;
  eraLd(1.0, p, q, e, em, dlim, deflected);
  ```

  Skip self-deflection for the Sun. Apply existing `jpl_apply_aberration_full()` once with Earth's reception velocity and Sun-observer distance, then Task 2's modern true-ecliptic rotation to position, `eraP2s`/`eraAnp` to angles, and a checked distance norm. Return `JPL_ERROR_NUMERIC` for nonrepresentable finite arithmetic, and publish all three outputs only after success. If pilot Horizons cases show that ERFA's `eraLd` setup or limiter does not model finite-distance light rays adequately, stop and revise the correction model before proceeding; never silently use `eraLdsun` (distant-star assumption).
- [ ] **Step 5: Rebuild and run the focused tests and README consumer.** `cmake --build build/docs-audit-merge --config Debug --target jpl_convenience_tests jpl_examples; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug -R '^(jpl_convenience|jpl_examples)$' --output-on-failure }`. Expected: both pass. Add a compiler/link probe to installed `tests/consumer/main.c`:

  ```c
  double out[3] = {1.0, 2.0, 3.0};
  if (jpl_apparent_ecliptic_position(NULL, tt, JPL_MOON, out)
      != JPL_ERROR_ARGUMENT) return 1;
  ```

  Ensure the new API does not alter `jpl_planet_position()` and `jpl_moon_position()` golden cases.

### Task 4: Acquire reviewed Horizons batches, provenance, and integrity checks

**Files:** Create `tests/data/horizons_de441.txt`, `tests/cmake/check_horizons_reference.cmake`; modify `tests/data/PROVENANCE.md`, `tests/data/MANIFEST.txt`, `tests/license_inventory.cmake`, `CMakeLists.txt`.

**Interfaces:** Produces at least 12 reviewed TT rows per accepted target with raw Horizons quantity-45 RA/Dec (degrees), quantity-31 longitude/latitude (degrees), and quantity-20 range (AU). Task 5 consumes numeric rows keyed by TT JD and DE body code. Keep raw decimal tokens, not rounded C literals.

- [ ] **Step 1: Prepare one file-API request per target.** Make each `!$$SOF`/`!$$EOF` request use the spec's exact options with a multi-epoch `TLIST`, for example the Moon request:

  ```text
  !$$SOF
  COMMAND='301'
  OBJ_DATA='NO'
  MAKE_EPHEM='YES'
  EPHEM_TYPE='OBSERVER'
  CENTER='500@399'
  TIME_TYPE='TT'
  TLIST_TYPE='JD'
  TLIST='-1894917.5' '-1500000.5' '-1000000.5' '0'
        '1000000.5' '2000000.5' '2451545.0' '2453005.0'
        '2453166.5' '2455197.5' '3000000.5' '4000000.5'
        '5000000.5' '5370000.5'
  CAL_FORMAT='JD'
  QUANTITIES='45,31,20'
  REF_SYSTEM='ICRF'
  ANG_FORMAT='DEG'
  RANGE_UNITS='AU'
  APPARENT='AIRLESS'
  CSV_FORMAT='YES'
  EXTRA_PREC='YES'
  !$$EOF
  ```

  Use Horizons target IDs `1,2,4,5,6,7,8,9,10,301`; body code mapping uses `JPL_MOON=10` and `JPL_SUN=11` in the reference data. The dates include one near the 2004 Venus transit; inspect the resulting elongation and add a visible near-Sun row if the candidate is occulted. Keep each `TLIST` assignment line under the Horizons 80-character recommendation.
- [ ] **Step 2: Submit at most one POST per target sequentially.** Put the Moon request above at `build/de441-moon-301.horizons` using `apply_patch` (the `build/` parent already exists), then call `curl.exe -sS -F "format=text" -F "input=@build/de441-moon-301.horizons" "https://ssd.jpl.nasa.gov/api/horizons_file.api"`. Give each of the nine remaining targets an analogous owned build-tree request path. Do not use multiple `COMMAND` targets per file: that is email-only. Inspect each complete response before accepting rows: API version, body IDs, center `399`, both `{source: DE441}`, `TIME_TYPE=TT`, `QUANTITIES=45/31/20` columns, `$$SOE`/`$$EOE`, and exact count of returned JDs. A 200 response with `No ephemeris` is a failure. Do not proceed to the next batch if one fails.
- [ ] **Step 3: Save verbatim numeric tokens with explicit provenance.** `tests/data/horizons_de441.txt` contains JPL-sourced numbers, so exempt it from the first-party MIT notice in `tests/license_inventory.cmake` as with JPL testpo. Start with a source comment, a schema comment naming the nine fields, and a `# target 301: source=DE441 center=DE441` marker for each of the ten inspected response headers. Follow with records like the probed J2000 Moon row:

  ```text
  441 10 301 2451545.000000000 222.447119268 -10.900128597 223.3148557 5.1708744 0.00268997545281
  ```

  The fields are DENUM, libjpl body code, Horizons target ID, TT JD, Q45 RA/Dec, Q31 longitude/latitude, and Q20 range. Preserve target ID mapping and unrounded row values. Record request template, actual responses' API version, acquisition time, each source label, range/epoch coverage, and links to [Horizons file API](https://ssd-api.jpl.nasa.gov/doc/horizons_file.html) and [Swiss comparison Appendix C](https://ephe.scryr.io/swisseph/doc/swisseph.htm#how-to-compare-the-swiss-ephemeris-with-ephemerides-of-the-jpl-horizons-system) in `tests/data/PROVENANCE.md`.
- [ ] **Step 4: Write the failing integrity check and register it.** Add a standalone CMake script that reads `tests/data/horizons_de441.txt`, rejects missing/duplicate target-JD identities, wrong target mapping or declared source markers, fewer than 12 per target, nonfinite/missing values, and a SHA-256 mismatch with its canonical-LF entry in `tests/data/MANIFEST.txt`. Test a disposable copy with one altered numeric field; it must fail. Register `jpl_horizons_integrity` in `CMakeLists.txt` under tests ON; add new first-party text paths to `tests/license_inventory.cmake` as appropriate. Run `ctest --test-dir build/docs-audit-merge -C Debug -R '^(jpl_horizons_integrity|jpl_licenses)$' --output-on-failure`: expect red before pinning the reviewed fixture, green after. The hash and declarations pin the reviewed file; they do not by themselves prove upstream provenance, which Step 2 checks against actual Horizons responses. Do not rewrite hashes from the file under test during the check.

### Task 5: Compare independent positions offline and on local DE441

**Files:** Create `tests/test_horizons.c`; modify `CMakeLists.txt`, `tests/data/PROVENANCE.md`.

**Interfaces:** Consumes Task 3 public API and Task 4 exact decimal text rows. Accepts `argv[1]` path to the DE441 slice or full DE441 and `argv[2]` to the committed Horizons reference file; returns nonzero on invalid/mismatched modern-epoch rows.

- [ ] **Step 1: Add a failing fixture runner.** Parse each noncomment row, require DENUM 441, target mapping, finite angles/range, unique target-JD identities, and within-file coverage. Use an exact nine-field schema; reject any extra tokens. Split the *original decimal JD token* into an integer and signed fractional piece rather than splitting a rounded `double`:

  ```c
  char jd_token[64];
  int denum, target, horizons_target;
  double ra, dec, old_lon, old_lat, range, got[3];
  char extra;
  int fields = sscanf(line, " %d %d %d %63s %lf %lf %lf %lf %lf %c",
                      &denum, &target, &horizons_target, jd_token,
                      &ra, &dec, &old_lon, &old_lat, &range, &extra);
  JPL_TEST_CHECK(fields == 9);
  int expected_id = target == JPL_MOON ? 301 :
                    target == JPL_SUN ? 10 : target;
  JPL_TEST_CHECK((target == JPL_MOON || target == JPL_SUN ||
                  (target >= JPL_MERCURY && target <= JPL_PLUTO &&
                   target != JPL_EARTH)) && horizons_target == expected_id);
  char *fraction;
  double tt[2];
  tt[0] = (double)strtoll(jd_token, &fraction, 10);
  tt[1] = *fraction == '.' ? strtod(fraction, NULL) : 0.0;
  if (jd_token[0] == '-') tt[1] = -tt[1];
  JPL_TEST_CHECK(denum == 441 && isfinite(tt[0]) && isfinite(tt[1]));
  double tdb[2];
  const jpl_header *h = jpl_get_header(ephem);
  JPL_TEST_CHECK(h != NULL && h->ephemeris_number == 441);
  JPL_TEST_CHECK(jpl_tt_to_tdb(tt, tdb) == JPL_OK);
  if (jpl_date_compare(tdb, h->start_jd) < 0 ||
      jpl_date_compare(tdb, h->end_jd) > 0) continue;
  JPL_TEST_CHECK(jpl_apparent_ecliptic_position(ephem, tt, target, got)
                 == JPL_OK);
  ```

  Validate the decimal token contains only an optional minus, digits, and an optional fractional part; validate all numeric fields with `isfinite`. Compare normalized TT/TDB pairs through the private `jpl_date_compare` helper; add `${CMAKE_CURRENT_SOURCE_DIR}/src` to the runner's private include path. Require at least 2 in-span rows for each of 10 bodies with the committed 2000–2010 slice; reject an accidentally empty test.
- [ ] **Step 2: Validate the reference frame conversion separately.** Treat quantity 45 as an ICRF direction; form a unit Cartesian vector via ERFA `eraS2c(ra_deg * ERFA_DD2R, dec_deg * ERFA_DD2R, p)`, then apply the test-only IAU2006/2000A true-of-date matrix at the TT pair. Compare using a stable angular separation of unit directions, and quantity-20 range independently:

  ```c
  double separation = atan2(sqrt(cx*cx + cy*cy + cz*cz), dot);
  JPL_TEST_CHECK(separation <= 1e-6);      /* radians, modern-epoch rows */
  JPL_TEST_CHECK(fabs(got[2] - range) <= 1e-7); /* AU, modern-epoch rows */
  ```

  Here `(cx,cy,cz)` is the cross product of two unit ecliptic directions, `dot` their clamped dot product, and the comparison is only for the documented 1950–2050 precision tier. These limits are initial acceptance bounds (~0.206 arcsec and 15 km), not claims about external models; on a failure, investigate model assumptions rather than inflating them. For quantity 31, test the independent IAU76/80 path as a consistency diagnostic rather than as the new helper's expected modern longitude. Do not compare Q31 directly with the modern output.
- [ ] **Step 3: Demonstrate the comparison fails meaningfully before trusting tolerances.** Use a disposable text copy with one modern-era RA shifted by 1 degree; the offline runner must fail for that row while leaving other rows intact. Set physically justified modern-epoch angular/range limits *before* accepting generated fixture numbers; do not automatically widen them from observed differences. For far-date rows report angular/range residuals and enforce valid finite output and provenance, without calling a loose residual bound an accuracy pass.
- [ ] **Step 4: Register both tiers.** Link `jpl_horizons_tests` to `jpl`, include `${CMAKE_CURRENT_SOURCE_DIR}/src` and `${CMAKE_CURRENT_SOURCE_DIR}/src/erfa` for private dates and ERFA reference declarations, and compile as C17. Add `tests/test_horizons.c` to `tests/license_inventory.cmake`. Add `jpl_horizons_offline` against `tests/data/de441_2000_2010.eph` and the same committed reference file unconditionally when tests are ON. Add `jpl_horizons_de441_local` only if `JPLEPH` is nonempty and `$ENV{JPLEPH}/DE441.eph` exists, using the same runner and reference text. Neither test fetches data. Run `ctest --test-dir build -C Release -N` to inspect registrations; then run both with `ctest --test-dir build -C Release -R '^jpl_horizons_' --output-on-failure` where the local file is configured. Existing `jpl_testpo_441` remains the full-span geometric check.

### Task 6: Documentation, installed use, and final verification

**Files:** Modify `include/jpl_convenience.h`, `docs/api.dox`, `tests/public_symbols.txt`, `tests/examples.c`, `tests/consumer/main.c`, `docs/validation.md`, `tests/data/PROVENANCE.md` if not done in earlier tasks.

**Interfaces:** New helper appears in installed headers and Doxygen, without exposing ERFA types or requiring the user to supply an already-computed state.

- [ ] **Step 1: Finish the public docs.** Detail TT two-part inputs, allowable body codes, source frame gating, correction order, modern true-of-date orientation, output degree/AU units, zero-radius rejection, deep-time model limits, and failure transactional behavior. Ensure every declaration has `@brief`, `@param`, applicable `@return` and `@note`. The grouped Doxygen homepage and `tests/public_symbols.txt` include `jpl_apparent_ecliptic_position`.
- [ ] **Step 2: Rebuild docs and run focused coverage.** `cmake --build build/docs-audit-merge --config Debug --target docs; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug -R '^(jpl_docs_symbols|jpl_docs_paths|jpl_docs_only|jpl_licenses)$' --output-on-failure }`. Expected: Doxygen warning-free and all focused tests pass.
- [ ] **Step 3: Build Debug and Release and run appropriate tiers.** `cmake --build build/docs-audit-merge --config Debug --parallel; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug --output-on-failure }`; then `cmake --build build --config Release --parallel; if ($?) { ctest --test-dir build -C Release --output-on-failure }`. Report actual registrations and results, including that local Horizons and `jpl_testpo_441` require full local DE441. `jpl_install` must confirm the new symbol links for a consumer while the installed production archive still defines no `jpl_test_*` symbols.
- [ ] **Step 4: Inspect final worktree.** `git diff --check; git status --short --branch; git diff --stat`. Review every intended file, including the preexisting uncommitted docs and constant-name work, before reporting results. Scan new public and internal documentation/comment lines for implementation task or step references; write scientific and API descriptions instead. Leave historical planning records alone. No staging or commits without an explicit user request.
