<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Glossary

Domain language for libjpl. Names here are the approved terms for code comments, commit messages, and plans. Architecture vocabulary (module, interface, depth, seam, adapter, leverage, locality) is defined separately in the architecture review process.

## Ephemeris core

- **State**: a six-degree-of-freedom vector, position (AU) plus velocity (AU/day). Public type `jpl_state {x, y, z, vx, vy, vz}`.
- **Date**: a two-part Julian date, high plus low binary64 parts whose sum is the epoch. Public type `jpl_date {hi, lo}`; internal arithmetic uses normalized `double[2]` pairs.
- **Normalized date**: a two-part date in canonical form (low part bounded, pair sums to the epoch). All internal consumers assume normalized input; entry points normalize.
- **Reference frame**: the orientation family of a DE coefficient set (`JPL_FRAME_DYNAMICAL_1950`, `JPL_FRAME_DYNAMICAL_2000`, `JPL_FRAME_ICRF`, `JPL_FRAME_UNKNOWN`). Core readers accept every frame natively; ICRS convenience requires ICRF.
- **Light-time (delay)**: the retarded-minus-reception interval in days, solved iteratively to `1e-12`-day tolerance (a solver tolerance, not an accuracy claim).
- **Emission / Reception**: the two epochs in a light-time solve. Emission is the retarded target epoch; reception is the query epoch where the observer sits. Mixing them is a bug (see `v_target(emission) - v_center(reception)`).
- **Aberration**: stellar aberration plus the solar gravitational term (`jpl_apply_aberration`), driven by observer velocity and Sun distance.
- **Apparent place**: the observer-visible direction and rate (aberrated, deflected, true-ecliptic) as opposed to the geometric state.
- **TT-TDB**: the millisecond-scale Terrestrial Time minus Barycentric Dynamical Time correction, from file datasets or the geocentric model.
- **Subinterval**: a Chebyshev interpolation span inside a data record; record selection followed by subinterval selection locates the evaluating polynomial.

## Deepened modules (architecture review 2026-11-01)

- **RecordEvaluator**: the module that owns record selection, bounds verification, subinterval search, Chebyshev evaluation, and cache discipline behind a single evaluate interface (implemented as `interp_work` + `jpl_evaluate()` in `src/jpl.c`).
- **EphemerisGeometry**: the module returning one atomic light-time solution (`rel_state`, `light_time_days`, `observer_vel`, `sun_vec`, `emission`) behind a single seam (implemented as `jpl_geometry` + `jpl_compute_geometry()` in `src/jpl_convenience.c`).
- **ApparentPlace**: the pipeline threading `Position → Direction → Deflected → Aberrated → Ecliptic` stages through struct-typed seams (implemented as one newtype `jpl_direction` + stage functions and an extracted `jpl_estimate_longitude_rate` sampler policy in `src/jpl_convenience.c`, not five newtypes; see C3).
- **Direction**: a finite unit vector; the pipeline stage type. `jpl_direction_from_raw` normalizes finite nonzero input once and fails degenerate (NULL/nonfinite/zero) input at construction, so malformed vectors never reach `eraLd`.
- **LayoutDescriptor**: the single record-layout description parsed from header bytes by production code and consumed by test mocks to emit bytes (implemented as shared offset constants + `jpl_tail_offset()`, not a separate struct; see C5).
- **EpochFrame**: the cached frame transform. Implemented without a new type: the existing `jpl_frame_context` (single mean-of-date matrix plus `create`/`rotate_*`/`destroy`) already plays this role, backed by one static matrix computer; see C6 for why no separate struct was introduced.
- **Seam sketch**: the Step-0 design notes fixing what a consumer-facing interface will look like before it is implemented.

## Deepened modules (architecture followup 2026-11-24)

- **EntryGate**: the module owning validate → normalize → coverage at every query entry point (implemented as `jpl_entry_prepare` + `jpl_entry_coverage` in `src/jpl.c`, declared in `src/jpl_private.h`). Every query funnels its input epoch through prepare and checks coverage on the epoch the data reads use before any dataset or frame gating, so RANGE precedes UNSUPPORTED structurally rather than by per-site convention.
- **TtToTdbPolicy**: the module choosing file-interpolated vs model TT→TDB per query (implemented as `jpl_tt_to_tdb_policy` in `src/jpl_convenience.c`, declared in `src/jpl_private.h`). The model converts first as seed and guaranteed baseline; file data refines when present and in coverage; any file-path failure keeps the model result.
- **HandleReader**: the module owning all byte transport behind the byte-source seam (implemented as `read_doubles` over a per-handle `jpl_byte_source` in `src/jpl.c`, seam declared in `src/jpl_private.h`). mmap `memcpy` vs FILE `seek`+`fread` is a construction-time choice; no caller names a source kind, and both open paths share one layout validator.

## Deepened modules (architecture deepening 2026-11-27)

- **LightTimeSolution**: the atomic solver value `{rel_state, observer_state, light_time_days, emission}` returned by `jpl_get_lighttime_solution()` and consumed by `jpl_compute_geometry()`. The solver fills it from one accepted emission evaluation, so the observer state is not re-read and emission is not reconstructed from the delay.
- **FixtureBuilder**: the test-side DE fixture adapter (`tests/jpl_fixture.h` / `.c`). Behavior tests declare coverage, bodies, coefficient series, and optional TT-TDB/libration data; the builder owns slot assignment, constants-record layout, padding, and byte order. Parser-negative tests keep raw byte construction.
