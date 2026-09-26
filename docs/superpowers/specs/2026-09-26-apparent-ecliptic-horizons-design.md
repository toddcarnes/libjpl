<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Geocentric apparent ecliptic positions and DE441 references

## Goal and boundaries

Provide a single libjpl query for the apparent geocentric tropical position
and motion of the Sun, Moon, or a planet in the true ecliptic and equinox
of date. Use an already-open DE handle and a two-part TT date. Return
longitude, latitude, range, signed longitude speed, and radial range-rate.
Longitude speed differentiates the corrected apparent output. Radial
range-rate instead uses the Horizons quantity-20 convention: the
line-of-sight projection of the mixed-epoch target-minus-observer
velocity. It is not the derivative of the corrected range. Preserve
the current output and error behavior of
`jpl_planet_position()` and
`jpl_moon_position()`. Full-file geometric DE441 coverage remains separate
from the limited range where Horizons offers apparent observer ephemerides.
Write public docs, source comments, and reference-data provenance in terms
of API behavior and verified science, with no implementation task or step
identifiers. Numbered tasks belong only in the historical implementation plan.

## Public contract

Add a documented public result type and function in
`include/jpl_convenience.h` with the conceptual signatures

```c
typedef struct jpl_apparent_ecliptic_result {
    double longitude_deg;
    double latitude_deg;
    double range_au;
    double longitude_rate_deg_per_day;
    double range_rate_km_per_s;
} jpl_apparent_ecliptic_result;

jpl_status jpl_apparent_ecliptic_position(const jpl_ephemeris *ephem,
    const double jd_tt[2], int target, jpl_apparent_ecliptic_result *out);
```

The caller opens the file once, then supplies a finite TT `double[2]` and
one of Mercury, Venus, Mars, Jupiter, Saturn, Uranus, Neptune, Pluto, Sun,
or Moon. Earth is not an Earth-centered direction; the SSB, EMB,
nutations, librations, and invalid codes are also excluded. Position is
longitude in `[0, 360)` degrees, latitude in degrees, and apparent range
in the modern AU. The signed longitude rate is degrees per TT day:
negative means retrograde. The signed range-rate is km/s: negative means
the body approaches Earth's center along the light-time-corrected
line of sight. Range-rate alone does not identify
retrograde motion. It is geocentric, not topocentric; no refraction is
applied. Results and errors use the existing `jpl_status` conventions,
and the output is unchanged on failure, including invalid inputs,
missing bodies, out-of-coverage emission times, and numerical failure.
Only files classified `JPL_FRAME_ICRF` are supported; legacy or unknown
frames return `JPL_ERROR_UNSUPPORTED` after the same date/range policy as
the existing convenience API. The documentation distinguishes ICRF-aligned
releases from an assertion that all DE realizations are identical.

The current feature branch already contains a tested position-only helper
and a 140-row Horizons reference. This revision changes its result contract
before any feature commit and supersedes that reference with signed rate
columns and paired nearby epochs. The shared geometry and existing
planet/Moon convenience functions remain as already verified.

The coordinate convention is an IAU 2006 precession / IAU 2000A nutation
true ecliptic and equinox of date at reception TT. It is not the existing
`eraEcm06` mean-ecliptic rotation. State that astronomical model validity
is narrower than the DE441 file's date coverage; accepting a far-date
query does not claim modern-epoch physical accuracy there. Keep all epoch
arithmetic as two-part dates. Sidereal choices remain outside this
contract.

## Calculation and code placement

Refactor `src/jpl_convenience.c` around a private shared geometry step:
normalize and convert TT to TDB, enforce file coverage and frame gating,
solve the target's light-time from Earth's center, and obtain the observer
velocity and Sun geometry needed by subsequent corrections. Reuse existing
`jpl_get_state_lighttime()` and `jpl_get_state()`; do not duplicate their
solvers. Keep the current shortcuts and avoid extra Sun I/O in existing
`jpl_planet_position()` branches that do not need it. The old function
continues to apply exactly its existing aberration policy. Its Moon wrapper
continues to delegate to it.

For the new query, apply finite-distance solar gravitational deflection
where meaningful, followed by stellar aberration. Handle the Sun itself
without self-deflection. ERFA `eraLd` can contribute to the finite-source
deflection calculation; `eraLdsun` assumes a distant star and cannot simply
be applied to nearby planets or the Moon. Validate the geometric inputs,
deflection limiter, Sun/observer singular cases, and the order of corrections
against independent apparent-position references. The existing solar
potential term in `jpl_apply_aberration_full()` is not light deflection;
avoid counting either correction twice. Keep physical constants in
`src/jpl_private.h` and leave vendored ERFA sources untouched.

Place pure true-of-date frame math in `src/jpl_frames.c`, with any shared
internal declaration in `src/jpl_private.h`; it does not need a file handle.
Use ERFA's precession-nutation and true obliquity consistently at the TT
reception epoch, then turn the corrected Cartesian direction into longitude
and latitude with ERFA vector/angle operations. Preserve the corrected
range. In particular, do not interpret the existing mixed-epoch,
unaberrated velocity as an apparent longitude derivative.

Implement one private corrected-position evaluator at a TT pair, reused
by the public query at the requested epoch and nearby epochs. At the
requested epoch, compute the signed radial range-rate by projecting
`jpl_get_state_lighttime()`'s mixed-epoch relative velocity on the
*unaberrated, light-time-corrected* observer-to-target direction and
converting AU/day to km/s with the private modern AU and seconds-per-day
constants. Horizons quantity 20 uses that definition; a finite difference
of corrected ranges differs by about 0.00157 km/s for a sampled Mercury
epoch and must not be compared to `deldot` as if identical. The public
query uses a central difference at TT offsets `-0.01` and `+0.01` day
only for signed longitude speed. Unwrap longitude across `0/360` before
dividing by elapsed TT days. If only one neighbor is in file/emission
coverage, use a
one-sided difference (second-order with two same-side samples where
available); halve the step up to twelve times if neither neighbor can
be evaluated. Report `JPL_ERROR_RANGE` when no distinct usable neighbor
exists. Propagate I/O, format, and numerical failures rather than
silently treating them as coverage failures. Longitude speed is a numerical
estimate of the derivative of the fully corrected position; document its
finite step and near-limb limits. The radial output follows the
Horizons-style projected mixed-epoch convention, not that derivative.
Publish the result structure only after position and both rates succeed.

## Horizons reference data and provenance

Collect pinned numerical output through the [Horizons file API](https://ssd-api.jpl.nasa.gov/doc/horizons_file.html).
Submit one `!$$SOF`/`!$$EOF` file per target using a multi-epoch `TLIST`
(at most 10,000 values), not one request per row. Submit these target
requests sequentially, reusing their saved results instead of repeatedly
querying the service. The [Horizons batch instructions](https://ssd.jpl.nasa.gov/ftp/ssd/horizons_batch.txt)
state that multiple `COMMAND` targets in one job work only through email,
not the API; the file API supports the multi-time `TLIST` for one target.
Use `EPHEM_TYPE='OBSERVER'`, `CENTER='500@399'` (Earth's geocenter),
`TIME_TYPE='TT'`, numeric `TLIST` dates with `TLIST_TYPE='JD'`,
`CAL_FORMAT='JD'`, `QUANTITIES='45,31,20'` (inertial ICRF apparent
RA/Dec, legacy ecliptic longitude/latitude, apparent range, and its
range-rate), `REF_SYSTEM='ICRF'`, `ANG_FORMAT='DEG'`, `RANGE_UNITS='AU'`,
`APPARENT='AIRLESS'`, `CSV_FORMAT='YES'`, `EXTRA_PREC='YES'`,
`OBJ_DATA='NO'`, and `MAKE_EPHEM='YES'`. Match each libjpl target to
the corresponding Horizons DE *barycenter* ID (1, 2, 4 through 9),
Sun (10), or Moon (301). Physical
Mars (499) currently reports source `mar099`, not DE441; do not use it
as the DE Mars-barycenter reference. For every response, verify that both
the selected target and the Earth center report `source: DE441`; reject
any row with other provenance or missing quantity/epoch data. Check the
response body for Horizons errors even on HTTP 200 (the service returns
HTTP 200 for some failed ephemeris requests). This checks the DE441
assumption for the stored data without claiming that
Horizons must use DE441 for all future queries.

Horizons quantity 45 is **airless apparent ICRF RA/Dec**, with light-time,
gravitational light deflection, and stellar aberration; quantity 31 is
**IAU76/80 ecliptic of date**. Keep both as published. Convert the
quantity-45 ICRF direction to the new public helper's IAU2006/2000A
true-of-date output in a test-only reference adapter. Retain quantity 31
for a separate legacy-frame consistency check; never compare it directly
to the modern-frame public output or mistake it for a modern standard.
Compare angular directions with wraparound handled correctly, and range
separately. Record exact request settings, target/center identities, source
labels, API version, acquisition date, and raw decimal epoch tokens. If
the reference model cannot support a justified numeric agreement bound at
a date, report that comparison as diagnostic, not as a precision pass.
Store reviewed values in `tests/data/`, pin their canonical-LF hash and
source provenance alongside the existing manifest
and provenance notes, and keep ordinary tests independent of the network.

Keep quantity 20's signed `deldot` column verbatim in km/s and compare
it to the helper's projected range-rate at sampled modern epochs. Horizons has no
direct true-of-date longitude-speed value in quantity 31. In each
one-target batch, request the original fourteen broadly separated TT
dates plus `-0.01` and `+0.01` day around each of the four modern dates
2451545.0, 2453005.0, 2453166.5, and 2455197.5. This yields 22 TT
dates per target, 220 rows in all, and allows an independent signed
longitude-speed estimate from corrected Horizons quantity-45 positions
rotated into the modern frame at each nearby TT date. Compare speed
with a wrap-safe difference; quantify the finite-difference and
published-decimal precision before setting a pass tolerance. Record
the superseding batch acquisition rather than inventing deldot values
for the earlier 140-row snapshot.

The pre-design J2000 Moon pilot (`TT JD 2451545.000000000`, Horizons API
1.2, target Moon 301 and center Earth 399 both `{source: DE441}`) returned
Q45 `222.447119268`, `-10.900128597` degrees, Q31 `223.3148557`,
`5.1708744` degrees, and Q20 range `0.00268997545281` AU. This pilot
also verified the selected output columns. Mars body center 499 reported
`{source: mar099}`, while Mars barycenter 4 reported `{source: DE441}`;
ancient and far-future Moon requests beyond 9999 BC/AD returned no
ephemeris. These are observed probes, not a substitute for the complete
per-batch provenance required above.

The [Swiss Ephemeris comparison guide](https://ephe.scryr.io/swisseph/doc/swisseph.htm#how-to-compare-the-swiss-ephemeris-with-ephemerides-of-the-jpl-horizons-system)
identifies traps directly relevant here: TT versus UTC/UT1 and Delta T,
astrometric versus corrected apparent coordinates, physical centers versus
planet-system barycenters, and Horizons quantity 2 sometimes using a
target-body equator instead of the inertial frame. Numeric TT Julian dates,
quantity 45 rather than quantity 2, exact DE441 target/center source
labels, and barycenter IDs avoid those mismatches. Its `-jplhora` example
also illustrates that differing nutation/correction models can remain after
matching times and bodies. Document any residual model difference instead
of hiding it in a loose tolerance. Since dates are supplied as Julian
dates, historical versus astronomical BCE year numbering does not enter
fixture identities. The Swiss comparison uses an older DE431 file and
is guidance on conventions, not a source for our DE441 reference numbers.

Sample every included target at at least twelve distinct TT epochs spread
through the Horizons/DE441 overlap, including safe margins near both
Horizons date limits, several intermediate ancient and future epochs,
and at least four center dates within the committed 2000-2010 DE441
slice, each with two neighboring TT dates. Include
additional configurations near the Sun to exercise deflection and cases
near a 0/360-degree longitude boundary. Horizons' Moon observer service
rejects dates earlier than about 9999 BC and later than about 9999 AD;
the local `D:/ephem/jpl/bin/DE441.eph` spans JD -3027215.5 through
7930192.5 (roughly 13000 BC to 17000 AD). Do not represent the narrower
Horizons sample as apparent-position verification at the file extremes.

## Verification policy

- Before the refactor, capture tests of existing convenience outputs and
  failure contracts, including identical-body and SSB shortcuts; prove
  they remain unchanged for both FILE and mmap handles and both endian
  layouts. Add focused solar-deflection, Sun/Moon, frame, range,
  wraparound, and invalid-input cases for the new query.
- Use checked-in DE441 slice rows and Horizons values from 2000-2010 for
  an always-on offline comparison. Establish and document reference
  uncertainties and acceptance thresholds before accepting golden rows;
  never widen a tolerance simply to turn a mismatch green. Test the
  reference frame conversion separately from the apparent calculation;
  use the Q45 inertial direction to isolate light-time/deflection/aberration
  discrepancies from ecliptic-frame discrepancies. Test both signed rates
  against the retained Q20 deldot and the Q45 neighboring positions.
  Q31 checks the older frame mapping but is not the primary modern-frame
  acceptance value.
- Register broader Horizons-overlap comparisons only when the full local
  DE441 file is available. Use tight, justified modern-epoch acceptance
  and separately identified deep-time diagnostics. The existing
  `jpl_testpo_441` and local DE441 sweep cover geometric interpolation
  across the full local binary span, including dates outside Horizons.
  They do not validate apparent corrections there.
- Keep network use in explicit reference acquisition, not normal CTest.
  Verify provenance/hash checks, Debug and Release builds and tests,
  docs (`WARN_AS_ERROR=YES`), installed-package use, and the existing
  offline/no-Python configuration. Add the public declaration to the
  Doxygen symbol inventory and grouped landing page.

## Work sequence

The previously validated position path and original batched Horizons
queries provide the baseline. Update the public result and Doxygen contract,
add failing rate tests, then extract the private corrected-position evaluator
without changing its positional output. Acquire one superseding batch per
target with retained range-rate and nearby TT epochs; verify all source
metadata and reference identities before replacing the original fixture.
Compare both rate estimates against independent Horizons values and rerun
the offline and local suites. Stop and revisit the correction model if
the independent comparisons reveal a physical-model mismatch; do not
silently substitute the easier Horizons IAU76/80 convention for the
approved modern public contract.
