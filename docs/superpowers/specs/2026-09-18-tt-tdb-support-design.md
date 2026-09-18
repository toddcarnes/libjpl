# TT-TDB Dataset Support Design

## Purpose

Expose the TT-TDB Chebyshev dataset carried by JPL time-segmented
(`*t`) ephemeris binaries (DE430t, DE432t, DE436t, DE438t, DE440t) so
callers get exact TT-minus-TDB in seconds instead of the ~2 ms ERFA
`eraDtdb` zero-observer approximation behind `jpl_tt_to_tdb()`. Also
answers "is this a t-variant?" from file content, not the filename:
DENUM carries no `t` (DE430t reports DE430), but the IPT entry-15
pointer is present if and only if the dataset exists.

## Background and authority

- JPL `testeph1.f`, PLEPH header: target 17 = TT-TDB at geocenter,
  units [seconds, seconds/day] (PV(1) value, PV(2) rate). JPL answers
  T#17 from `IPT(1,15)` via `INTCHB(..., NCM=1, ...)` and reports
  "TT-TDB requested but not found on file" when the pointer is absent.
- `dump_eph` on local binaries: DE430t carries `TT-TDB = (939, 11, 4)`;
  DE430 carries `(0, 0, 0)`. All five `*t` testpo files hold 876-908
  T#17 lines (X#1 value, X#2 rate); no testpo file references T#16
  (lunar mantle angular velocity), so T#16 is out of scope.
- Current libjpl header read is exactly 2856 bytes: IPT(1..12), DENUM,
  IPT(13) libration. IPT entries 14/15 live at bytes 2856-2880 and are
  never read today.

## API

```c
int jpl_has_tt_tdb(const jpl_ephemeris *e);
jpl_status jpl_get_tt_minus_tdb(const jpl_ephemeris *e, double jd_tdb,
                                double *seconds, double *rate_per_day);
```

- `jpl_has_tt_tdb`: nonzero iff the open file carries the dataset
  (all of start/coeff/sub positive, same presence rule as libration;
  mirrors JPL's `IPT(1,15) > 0` check). NULL handle returns 0.
- `jpl_get_tt_minus_tdb`: interpolates the dataset with 1 component at
  TDB Julian date. `seconds` is required (NULL gives
  `JPL_ERROR_ARGUMENT`); `rate_per_day` is optional (NULL allowed).
  Absent dataset or out-of-coverage jd gives `JPL_ERROR_RANGE`, matching
  JPL's "not on file" plus the library's out-of-range convention. Never
  falls back to ERFA: callers wanting the ~2 ms approximation keep using
  `jpl_tt_to_tdb()` (its header note cross-references the new getter).
- Both decls get full Doxygen (`@brief/@param/@return/@note`); the
  getter notes units [s, s/day], the PLEPH mapping, and the NULL-rate
  allowance. Implementation lives in `src/jpl.c` next to `interpolate`,
  reusing that engine (coefficients in seconds, span in days, so value
  and rate come out directly with no unit conversion). Pure read path:
  same per-handle cache rules as `jpl_get_state`, no mutation on mmap.

## Header changes

- `HEADER_SIZE` 2856 to 2880; parse IPT entries 14/15 after the
  libration pointer with the same u32-triple pattern and overflow
  guards. Entry 14 is parsed-and-stored or parsed-and-skipped per
  implementer judgment (nothing consumes it yet); entry 15 is stored.
- `jpl_header` gains `jpl_pointer tt_tdb_pointer` and the parsed flag
  backing `jpl_has_tt_tdb`. The flag is derived at parse time, so "is
  this a t-variant" never depends on the filename.
- Size guards in `jpl_open`/`jpl_open_mmap` move with `HEADER_SIZE`
  (files smaller than 2880 bytes are rejected as before; every real DE
  file is megabytes, so no valid file is affected).

## Validation

- `tests/test_testpo.c`: route T#17 lines to the new getter (X#1 vs
  `seconds`, X#2 vs `rate_per_day`) instead of `skipped_unsupported`;
  tolerance absolute ~1e-13 following `testeph.f` practice, pinned
  exactly during implementation against the five `*t` files.
  Pinned at implementation as 1e-12 * fmax(1, |expected|) (see tests/test_testpo.c T#17 routing); all five *t suites validate with zero MISMATCH at this envelope.
- New `tests/test_tt_tdb.c` (registered in CMakeLists with the same
  warning flags as other suites): absent-dataset error on a non-`t`
  mock (`JPL_ERROR_RANGE` + `jpl_has_tt_tdb() == 0`, no large binary
  needed), NULL-argument checks, and a real-data assertion via
  `%JPLEPH%` when set.
- Full suite must stay green: 6 unit + 2 integration + 29 testpo (the 5
  `*t` suites now validate their T#17 lines instead of skipping them),
  Windows/MSVC Debug plus Linux/GCC Release with zero warnings.

## Non-goals

- T#16 (lunar mantle angular velocity): no testpo coverage, not parsed,
  not exposed. Surveyed 2026-09: all 31 local binaries carry either
  `(0, 0, 0)` or `(start, 0, 0)` at the IPT-14 slot (e.g. `(939, 0, 0)`
  on DE430t/DE432t, `(1019, 0, 0)` on DE431+), so no file has a complete
  triple and there is nothing interpolatable. The nonzero start values
  reserve the slot where the dataset would begin; if JPL ever ships
  coefficients there, only the coeff/sub counts plus a new getter are
  needed to expose it.
- No ERFA fallback inside the getter; no change to `jpl_tt_to_tdb()`
  semantics; no filename-based detection anywhere.
