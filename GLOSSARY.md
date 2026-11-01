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

- **RecordEvaluator**: the module that owns record selection, bounds verification, subinterval search, Chebyshev evaluation, and cache discipline behind a single evaluate interface.
- **EphemerisGeometry**: the module returning one atomic light-time solution (`rel_state`, `light_time_days`, `observer_vel`, `sun_vec`, `emission`) behind a single seam.
- **ApparentPlace**: the pipeline threading `Position → Direction → Deflected → Aberrated → Ecliptic` stage types, each constructor-enforced.
- **Direction**: a finite unit vector; the pipeline stage type that makes a non-unit deflection input a construction-time failure.
- **LayoutDescriptor**: the single record-layout description parsed from header bytes by production code and consumed by test mocks to emit bytes.
- **EpochFrame**: the module owning a normalized epoch plus cached mean and true rotation matrices, exposing one parameterized rotation.
- **Seam sketch**: the Step-0 design notes fixing what a consumer-facing interface will look like before it is implemented.
