<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Apparent ecliptic motion implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the existing uncommitted geocentric apparent-ecliptic helper to report signed longitude speed and radial range-rate, independently checked against DE441 Horizons.

**Architecture:** Retain the existing light-time, finite-source deflection, aberration, and modern true-of-date path as a private evaluator. Project its mixed-epoch relative velocity onto the light-time-corrected sight line to reproduce Horizons `deldot`; differentiate only unwrapped longitude at neighboring TT dates, using a one-sided difference at file edges. Return all five results through a named public struct. Supersede the 140-row Horizons reference with one batched request per target that includes `deldot` and nearby modern dates.

**Tech Stack:** C17, CMake/CTest, bundled ERFA v2.0.1, Horizons file API, MSVC Debug/Release, PowerShell 5.1.

**Spec:** `docs/specs/2026-09-26-apparent-ecliptic-horizons-design.md` (approved rate revision)

## Global Constraints

- Work inline on `feature/apparent-ecliptic-horizons`; preserve all uncommitted feature and historical plan/spec files. Do not commit, push, or merge without an explicit request.
- Keep task identifiers in this plan only. Public headers, README, Doxygen, validation/provenance notes, and source/test comments describe API behavior and science, never numbered implementation tasks.
- Public output uses TT `double[2]`, IAU2006/2000A true ecliptic/equinox of date, modern AU, signed longitude degrees/TT day and signed radial km/s. Longitude speed is a derivative of corrected apparent longitude; radial rate is the Horizons-style mixed-epoch velocity projected along the light-time-corrected sight line, *not* a derivative of corrected range. Outputs are transactional.
- Include Sun, Moon, and the eight supported planet barycenters; reject Earth and unknown/legacy frames as the current helper does. Preserve existing `jpl_planet_position()` and `jpl_moon_position()` outputs and error ordering.
- Retain `QUANTITIES='45,31,20'` and its Q20 signed `deldot` field from Horizons. Use one sequential file-API POST per target with a `TLIST`, verify both target and Earth center report DE441, and keep ordinary tests offline.
- Deep-time apparent rows remain diagnostics, distinct from justified modern-epoch numerical acceptance and the full-span geometric `testpo` sweep. Do not change ERFA sources.

## File map

- `include/jpl_convenience.h`, `src/jpl_convenience.c`: named result type and TT-based derivative around a private corrected-position evaluator.
- `tests/test_convenience.c`, `tests/examples.c`, `tests/consumer/main.c`, `README.md`, `docs/api.dox`, `tests/public_symbols.txt`, `docs/validation.md`: synthetic speed/edge checks, public usage and documentation.
- `tests/data/horizons_de441.txt`, `tests/data/MANIFEST.txt`, `tests/data/PROVENANCE.md`, `tests/cmake/check_horizons_reference.cmake`, `tests/cmake/test_horizons_compare.cmake`: superseded batched golden rows, hash, provenance and mutation probes.
- `tests/test_horizons.c`: signed-speed and Q20 range-rate comparisons, retaining Q45 position and Q31 diagnostic checks.

---

### Task 1: Named result and apparent motion

**Files:** Modify `include/jpl_convenience.h`, `src/jpl_convenience.c`, `tests/test_convenience.c`, `tests/test_horizons.c`, `tests/examples.c`, `tests/consumer/main.c`, `README.md`, `tests/public_symbols.txt`, `docs/validation.md`.

**Interfaces:** Produce `typedef struct jpl_apparent_ecliptic_result { double longitude_deg, latitude_deg, range_au, longitude_rate_deg_per_day, range_rate_km_per_s; } jpl_apparent_ecliptic_result;` and change `jpl_apparent_ecliptic_position(e, jd_tt, target, jpl_apparent_ecliptic_result *out)`. The private `apparent_position_at_tt(e, tt, target, double out[3], double *radial_km_per_s)` returns corrected longitude/latitude/range and, when requested for the center epoch, the Horizons-style sight-line projection.

- [ ] **Step 1: Add failing signed-rate and atomic-output tests.** Replace the three-double output in `tests/test_convenience.c` with a struct. Use the existing linear Chebyshev mock (Sun origin, Earth x=1 AU, target x=-0.5 AU/y=0.01 AU) to assert **negative** range-rate when the target's x coefficient increases toward Earth and **positive** longitude speed when y decreases with a negative observer-target x component. Build a second case crossing 0/360 degrees with `x>0`, `y(t)` changing sign and positive slope; the signed longitude speed must remain positive, not jump by nearly 360 degrees per step. Use a sentinel struct and check every field is unchanged on a NULL argument, invalid target, non-ICRF frame, or out-of-coverage date. For example:

  ```c
  jpl_apparent_ecliptic_result result = {7, 8, 9, 10, 11};
  JPL_TEST_CHECK(jpl_apparent_ecliptic_position(e, jd, JPL_EARTH, &result)
                 == JPL_ERROR_ARGUMENT);
  JPL_TEST_CHECK(result.longitude_deg == 7 && result.latitude_deg == 8 &&
                 result.range_au == 9 && result.longitude_rate_deg_per_day == 10 &&
                 result.range_rate_km_per_s == 11);
  ```

- [ ] **Step 2: Watch the test fail for the missing struct/changed contract.** `cmake --build build/docs-audit-merge --config Debug --target jpl_convenience_tests -- /verbosity:quiet`. Expected: unknown `jpl_apparent_ecliptic_result` or incompatible signature, not a malformed mock.
- [ ] **Step 3: Define the public struct and factor existing position calculation.** Append Doxygen `@brief`/member comments to the struct, document all five units/sign conventions and the radial projection convention, and replace `double lon_lat_range[3]` with `jpl_apparent_ecliptic_result *out` in the public declaration. Move the old three-value body into `static jpl_status apparent_position_at_tt(const jpl_ephemeris *e, const double jd_tt[2], int target, double position[3], double *radial_km_per_s)` in `src/jpl_convenience.c`; keep the code path, correction order, frame gate, and numerical errors unchanged. At the center evaluation, before deflection or aberration changes the direction, project the retarded state velocity along the retarded state position and convert to km/s:

  ```c
  double radial_au_per_day = (state[0]*state[3] + state[1]*state[4] +
                              state[2]*state[5]) / distance;
  double projected_km_per_s = radial_au_per_day * (JPL_AU_KM / JPL_DAYSEC);
  if (!isfinite(projected_km_per_s)) return JPL_ERROR_NUMERIC;
  /* Publish *radial_km_per_s only after the entire position succeeds. */
  ```

  Keep the original nine-field `tests/data/horizons_de441.txt` position comparisons working by changing `tests/test_horizons.c` to read `got.longitude_deg`, `got.latitude_deg` and `got.range_au`. Add `jpl_apparent_ecliptic_result` to `tests/public_symbols.txt`'s type section.
- [ ] **Step 4: Implement signed longitude speed without rounded-scalar date inputs.** Use `jpl_date_add_days(jd_tt, ±0.01, sample_tt)` and evaluate the same private position helper at both sides with a NULL radial output. For central samples use a wrap-safe angular difference:

  ```c
  double h_days = 0.01;
  double delta = remainder(plus[0] - minus[0], 360.0);
  result.longitude_rate_deg_per_day = delta / (2.0 * h_days);
  result.range_rate_km_per_s = projected_km_per_s;
  ```

  If one side is `JPL_ERROR_RANGE`, use the center and available side; prefer the second-order same-side formula `(-3*f0+4*f1-f2)/(2*h)` for forward or `(3*f0-4*fm1+fm2)/(2*h)` for backward, with longitude samples unwrapped relative to the center. If both sides report RANGE, halve `h_days` at most 12 times; report RANGE if neither neighbor succeeds. Propagate any non-RANGE error. Reject nonfinite/unrepresentable rates as `JPL_ERROR_NUMERIC`, then publish the whole struct once. A projected range-rate that is negative must mean approaching, independent of the longitude-rate sign.
- [ ] **Step 5: Run focused tests and update usage.** `cmake --build build/docs-audit-merge --config Debug --target jpl_convenience_tests jpl_examples -- /verbosity:quiet; if ($?) { ctest --test-dir build/docs-audit-merge -C Debug -R '^(jpl_convenience|jpl_examples)$' --output-on-failure }`. Change the README example and `tests/examples.c` to use named members, and update the installed consumer's NULL-argument link probe to pass the new struct. Record finite-difference sign/step and boundary behavior in `docs/validation.md` without implementation task references.

### Task 2: Supersede Horizons data with radial rates and near-time samples

**Files:** Modify `tests/data/horizons_de441.txt`, `tests/data/MANIFEST.txt`, `tests/data/PROVENANCE.md`, `tests/cmake/check_horizons_reference.cmake`, `tests/cmake/test_horizons_compare.cmake`.

**Interfaces:** Produce `220` source-checked rows with exactly ten fields: DENUM, libjpl body code, Horizons target, TT JD, Q45 RA/Dec (deg), Q31 longitude/latitude (deg), Q20 range (AU), Q20 deldot (km/s). Four modern center epochs per target have `±0.01`-day companions. Task 3 consumes the signed Q20 rates and pairs.

- [ ] **Step 1: Prepare ten revised batch requests.** Use each existing ignored `build/de441-horizons-*.horizons` request as the base. Keep all 14 previously sampled TT dates and add eight dates per target:

  ```text
  '2451544.99' '2451545.01' '2453004.99' '2453005.01'
  '2453166.49' '2453166.51' '2455197.49' '2455197.51'
  ```

  `QUANTITIES='45,31,20'` already returns `deldot` as the last CSV field. Use `apply_patch` for each request; keep `TLIST` lines below Horizons' suggested 80-character limit. The ten targets and source checks stay identical to the current batches.
- [ ] **Step 2: Reacquire one batch per target, sequentially.** Use the documented `curl.exe -sS -F 'format=text' -F 'input=@build/de441-horizons-moon.horizons' 'https://ssd.jpl.nasa.gov/api/horizons_file.api'` shape, substituting each target's owned request path. Require 22 rows bounded by `$$SOE`/`$$EOE`, exact TT JDs, both source labels `DE441`, and ten numeric columns including `deldot`. Check the four older J2000/Q45/Q31/range anchors against the existing fixture before replacing it; if the source solution changed, investigate rather than mixing versions. Do not run requests concurrently or automatically retry a failing request.
- [ ] **Step 3: Replace the old 140-row file with the 220 reviewed rows.** Retain all source comments and add `deldot20_km_s` as the final field, preserving each published decimal token. Update `tests/data/PROVENANCE.md` to describe the superseding batch acquisition, signed rate units and sample epochs without claiming the original output included values it did not. Update `horizons.de441.rows=220` and its canonical-LF SHA-256 in `tests/data/MANIFEST.txt` from the reviewed new file. Do not assign first-party MIT copyright to the JPL reference data.
- [ ] **Step 4: Make the integrity check reflect the new schema.** In `tests/cmake/check_horizons_reference.cmake`, require 10 rather than 9 fields, `220` total, 22 per target, and a finite signed rate at index 9. Keep the altered-Moon-RA negative control and add a second mutation of a Q20 deldot token to prove its pinned hash detects a changed rate. Run `ctest --test-dir build/docs-audit-merge -C Debug -R '^jpl_horizons_integrity$' --output-on-failure`. The comparison runner still accepts nine fields and is updated with its mutation test together in the next task. No network in CTest.

### Task 3: Compare both signed rates and verify the revised API

**Files:** Modify `tests/test_horizons.c`, `include/jpl_convenience.h`, `tests/examples.c`, `tests/consumer/main.c`, `README.md`, `docs/validation.md`, `tests/data/PROVENANCE.md`.

**Interfaces:** Consume Task 1's public struct and Task 2's ten-field reference. The existing runner still accepts an ephemeris path and the reference path, checks the committed 2000–2010 DE441 slice offline and the full local DE441 file when `JPLEPH` is set. Preserve independent Q45 modern-frame directions and Q31 legacy-frame diagnostics.

- [ ] **Step 1: Watch the previous parser fail on the superseding corpus.** Rebuild the existing `jpl_horizons_tests`, then run `ctest --test-dir build/docs-audit-merge -C Debug -R '^jpl_horizons_offline$' --output-on-failure`. Expected: the previously valid nine-field runner rejects ten-field rows. This is the red test for accepting the new source data, not a production code failure.
- [ ] **Step 2: Parse and compare both signed rates.** Extend the parser in `tests/test_horizons.c` to reject a missing tenth column, parse `deldot_km_s` with `isfinite`, and select the four center dates per target. Use Q45 neighbor positions rotated at each TT date by independent ERFA precession/nutation to compute a signed wrap-safe reference longitude speed:

  ```c
  double ref_speed = remainder(ref_plus_lon_deg - ref_minus_lon_deg, 360.0)
                       / 0.02;
  JPL_TEST_CHECK(fabs(got.longitude_rate_deg_per_day - ref_speed)
                 < 1e-4); /* degrees per TT day, modern-era rows */
  JPL_TEST_CHECK(fabs(got.range_rate_km_per_s - deldot_km_s)
                 < 1e-4); /* km/s, modern-era rows */
  ```

  Determine modern center rows by the four literal JDs from Task 2, not by a value generated by the helper. For other TT dates, retain position/range comparison and report signed rate residuals as deep-time diagnostics. Continue to reject duplicated/missing rows, wrong target IDs, nonfinite outputs and vacuous slice coverage.
- [ ] **Step 3: Run the updated comparison and mutation test.** Update `tests/cmake/test_horizons_compare.cmake` if its reference parser or error text depends on the old field count. Build `jpl_horizons_tests` and run `ctest --test-dir build/docs-audit-merge -C Debug -R '^jpl_horizons_(offline|reference_mutation|de441_local)$' --output-on-failure`. Expect 12 in-slice rows per body (120 total), 22 full-file rows per body (220 total), and that the shifted-RA negative control still fails as designed. Investigate any actual model mismatch rather than inflating a tolerance.
- [ ] **Step 4: Verify Release and finish public docs.** `cmake --build build --config Release --parallel -- /verbosity:quiet; if ($?) { ctest --test-dir build -C Release -R '^jpl_horizons_' --output-on-failure }`. Run `ctest --test-dir build -C Release -N` to inspect registration. The public struct and function need Doxygen `@brief`, field comments, every `@param`, `@return` and numerical-rate caveats. Update `tests/public_symbols.txt`, the README example, and validation/provenance prose without implementation task or step references. Rebuild `docs` with `WARN_AS_ERROR=YES`; run the docs-enabled Debug suite, Release suite, installed-package check, `jpl_licenses` and `git diff --check`. Keep all feature changes uncommitted until expressly requested.
