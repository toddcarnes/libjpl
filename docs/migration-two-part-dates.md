<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Migration to two-part Julian dates (0.3.0)

This guide covers the 0.3.0 breaking changes also summarized in
`CHANGELOG.md`. No scalar compatibility wrapper is offered: replace
scalar-date call sites directly.

## Before / after signatures

| Before (scalar) | After (two-part) |
| --- | --- |
| `jpl_get_state(e, double jd, ...)` | `jpl_get_state(e, const double jd_tdb[2], ...)` |
| `jpl_get_state_lighttime(e, double jd, ..., double *lt)` | `jpl_get_state_lighttime(e, const double jd_tdb[2], ..., double *lt)` |
| `jpl_get_tt_minus_tdb(e, double jd, ...)` | `jpl_get_tt_minus_tdb(e, const double jd_tdb[2], ...)` |
| `double jpl_tt_to_tdb(double jd)` | `jpl_status jpl_tt_to_tdb(const double jd_tt[2], double jd_tdb[2])` |
| `double jpl_tdb_to_tt(double jd)` | `jpl_status jpl_tdb_to_tt(const double jd_tdb[2], double jd_tt[2])` |
| `void jpl_ecliptic_to_icrs(state, double jd, out)` | `jpl_status jpl_ecliptic_to_icrs(state, const double jd_tt[2], out)` |
| `void jpl_icrs_to_ecliptic(state, double jd, out)` | `jpl_status jpl_icrs_to_ecliptic(state, const double jd_tt[2], out)` |
| `void jpl_frame_rotation_matrix(double jd, R)` | `jpl_status jpl_frame_rotation_matrix(const double jd_tt[2], double R[3][3])` |
| `void jpl_apply_aberration(state, vobs, out)` | `jpl_status jpl_apply_aberration(state, vobs, out)` |
| `void jpl_apply_aberration_full(state, vobs, dist, out)` | `jpl_status jpl_apply_aberration_full(state, vobs, dist, out)` |
| `double jpl_au_per_day_to_km_per_s(...)` / scalar `jpl_km_to_au` | `jpl_status jpl_au_per_day_to_km_per_s(const double[3], double[3])`, `jpl_status jpl_km_to_au(double, double *)` |
| `double start_jd, end_jd` coverage fields | `double start_jd[2], end_jd[2]` coverage fields |
| planet/moon convenience with scalar dates and unchecked frames | `jpl_planet_position(e, const double jd_tt[2], ...)` / `jpl_moon_position(e, const double jd_tt[2], ...)` returning `jpl_status`, requiring `JPL_FRAME_ICRF` |

Time conversions, frame math, aberration, and unit helpers that could
not previously fail now return `jpl_status`. Check the status before
using any output: computed outputs are transactional and unchanged on
failure.

## Constructing two-part dates

Build the pair directly as a day part plus a fraction part:

```c
const double tt[2] = {2451545.0, 0.125};
double tdb[2];
if (jpl_tt_to_tdb(tt, tdb) != JPL_OK) return 1;
```

Never compute a scalar full epoch first and split it afterward:
splitting an already-rounded scalar cannot restore the lost low bits.
Both parts travel through date arithmetic and ERFA calls (`eraEcm06`,
`eraDtdb`) without collapsing to a scalar first. Only the on-disk
record format stays scalar: raw record fields still expose the encoded
scalar epochs, decoded internally as `{scalar, 0.0}`.

## Low-part-preserving arithmetic expectations

Equivalent splits of the same epoch (for example day/fraction versus
reference-epoch/offset, or `2^-40`-day steps of about 79 ns) compare
equal after normalization, and changes carried only by the low part
select the correct record, subinterval, and coverage endpoint. Interior
boundaries select the following interval; the final advertised coverage
endpoint uses the last covered interval. A permitted trailing physical
record does not extend query coverage.

## Modern AU semantics

All public AU-valued results and conversion helpers use the exact
modern definition `1 AU = 149597870.7 km`. The original
`header.astronomical_unit_km` is preserved as metadata only and no
longer selects the output unit; raw record values are unchanged.
Light speed in AU/day is derived from exact `299792.458 km/s`,
`86400 s/day`, and the modern AU.

## Frame restrictions

Core readers (`jpl_get_state`, `jpl_get_state_lighttime`) accept every
frame natively. `jpl_header.reference_frame` exposes the orientation
family: `JPL_FRAME_DYNAMICAL_1950` (DE102), `JPL_FRAME_DYNAMICAL_2000`
(DE200/DE202), `JPL_FRAME_ICRF` (explicit allowlist: DE403, 405, 406,
410, 413, 421, 422, 423, 430, 431, 432, 440, 441), or
`JPL_FRAME_UNKNOWN` otherwise. ICRS convenience
(`jpl_planet_position`, `jpl_moon_position`) requires
`JPL_FRAME_ICRF` and returns `JPL_ERROR_UNSUPPORTED` for legacy or
unknown frames, even for identical-body calls. No historical-frame
transformation is applied. Target 0 is always invalid; center 0 is the
SSB alias, equivalent to 12.

## Error and output behaviour

- `JPL_ERROR_ARGUMENT`: NULL pointers, invalid body combinations,
  nonfinite user input, or physically invalid standalone aberration
  velocity (observer speed at/above light speed).
- `JPL_ERROR_RANGE`: finite date outside coverage, raw record index
  out of bounds, or no in-coverage emission solution for light-time.
- `JPL_ERROR_FORMAT`: malformed header/record, invalid file constants
  or layout, nonfinite file coefficients, nonphysical file motion.
- `JPL_ERROR_UNSUPPORTED`: in-coverage query for an absent dataset, or
  non-ICRF frame in convenience calls.
- `JPL_ERROR_CONVERGENCE`: bounded solve exhausts its budget
  (TDB-to-TT: 10 iterations to `1e-15` day; light-time: 10 fast plus
  128 safeguarded steps to `1e-12` day).
- `JPL_ERROR_NUMERIC`: finite validated inputs with an unrepresentable
  result. `JPL_ERROR_IO` / `JPL_ERROR_MEMORY` keep their usual meaning.
- Computed outputs (states, dates, rotations, aberration, units,
  light-time/delay, TT-TDB seconds/rate) are transactional: unchanged
  on failure. Exact input/output aliasing is supported where the relevant
  API function documents it. Raw record/constant destination buffers may
  be partially written after I/O failure.
- Tolerances such as `1e-15`-day conversion residuals and `1e-12`-day
  light-time residuals are solver stopping criteria, not astronomical
  accuracy claims. The TT-TDB correction amplitude is roughly 1.7 ms;
  ERFA cites better than +/-3 ns model accuracy over 1950-2050
  relative to DE405 integrations, omitting topocentric terms.
  Ephemeris-interpolated TT-TDB is not mathematically exact, and file
  coverage is not a model-accuracy guarantee.
