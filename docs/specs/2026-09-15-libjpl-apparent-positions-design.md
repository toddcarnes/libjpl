# libjpl Apparent Positions Design

**Date**: 2026-09-15  
**Status**: Approved for implementation

---

## Summary

Add light-time correction, stellar aberration, coordinate frame conversions, convenience API, and thread-safe I/O to libjpl. All features are optional layers on top of the existing geometric-position core (`jpl_get_state`).

---

## 1. Light-Time Correction

### Function
```c
jpl_status jpl_get_state_lighttime(const jpl_ephemeris *ephemeris, double jd_tdb,
                                   int target, int center, double state[6], double *out_light_time);
```

### Algorithm
1. Compute geometric state at `jd_tdb` → `state_0`
2. `light_time = distance(state_0) / c` (c = 173.144632684 AU/day)
3. Re-evaluate TARGET leg at jd_tdb - light_time; center stays at jd_tdb
4. Iterate until `|light_time_new - light_time_old| < 1e-12` days or max 10 iterations
5. Return converged state and light-time

### Notes
- Reuses `jpl_get_state` for barycentric positions
- Target/center semantics identical to `jpl_get_state`
- Returns `JPL_ERROR_RANGE` if iteration fails to converge

---

## 2. Aberration Correction

### Function
```c
void jpl_apply_aberration(const double state[6], const double observer_vel[3], double out_state[6]);
```

### Algorithm
Special-relativistic direction transform (cf. ERFA `eraAb`, Explanatory
Supplement Expr. 7.40), with distance preservation:
```
beta  = v_obs / c
bm1   = sqrt(1 - |beta|^2)
pdv   = pnat . beta               (pnat = pos / |pos|)
w1    = 1 + pdv / (1 + bm1)
p     = pnat*bm1 + w1*beta
out_pos = p * (|pos| / |p|)
out_vel = vel  (passed through unchanged)
```
with c = 173.144632684 AU/day.

### Notes
- Pure math, no I/O, no ephemeris dependency
- `observer_vel` in AU/day (from center body's barycentric velocity)
- `state[0..2]` = position (AU), `state[3..5]` = velocity (AU/day)
- In-place safe: `out_state` may equal `state`
- The <=0.4uas solar gravitational term of `eraAb` is omitted: it needs
  the Sun-observer distance, unavailable in this signature
- Known limitation, deferred by design (not an oversight): <=0.4uas sits
  ~10x below the ~5uas velocity-knowledge floor (cf. `ab.c` Notes 1-2);
  threading `s` through would churn the API, cost an extra ephemeris
  evaluation per call, and go singular for Sun-centered observers
- Validated against ERFA `t_erfa_c` vectors plus a direct `eraAb` grid
  comparison (see `tests/test_aberration.c`)

---

## 3. Coordinate Frame Conversions (via embedded ERFA)

### Embedded ERFA
- Copy minimal ERFA source (`erfa.h` + ~15 `.c` files) to `src/erfa/`
- BSD-3 license, MIT-compatible
- Credit in `src/erfa/LICENSE` and `README.md`

### Functions (in `jpl_frames.h`)
```c
// Ecliptic (of date) ↔ ICRS
void jpl_ecliptic_to_icrs(const double state[6], double jd_tt, double out_state[6]);
void jpl_icrs_to_ecliptic(const double state[6], double jd_tt, double out_state[6]);

// Rotation matrix
void jpl_frame_rotation_matrix(double jd_tt, double R[3][3]);  // ecliptic → ICRS

// TT ↔ TDB (approx)
double jpl_tt_to_tdb(double jd_tt);
double jpl_tdb_to_tt(double jd_tdb);
```

### ERFA routines needed
- `eraPmat06` — precession matrix (IAU 2006)
- `eraNut06a` — nutation matrix (IAU 2000A)
- `eraBpn2xy` — bias-precession-nutation
- `eraPvtob` — position/velocity vector rotation
- `eraDtdb` / `eraDtcb` — TT↔TDB (approximate)

---

## 4. Convenience API (`jpl_convenience.h`)

### Functions
```c
/**
 * @brief Compute apparent position of a planet (light-time + aberration, ICRS).
 * @param ephem Open ephemeris handle.
 * @param jd_tt TT Julian date.
 * @param planet Planet target (JPL_MERCURY..JPL_PLUTO, or JPL_SUN).
 * @param center Center target (any valid jpl_target, 0 = SSB).
 * @param state[6] Output: position (AU) + velocity (AU/day) in ICRS.
 * @return JPL_OK on success, JPL_ERROR_ARGUMENT for invalid target/center.
 */
jpl_status jpl_planet_position(const jpl_ephemeris *ephem, double jd_tt,
                               int planet, int center, double state[6]);

/**
 * @brief Compute apparent position of the Moon (Earth-centered, ICRS).
 * @param ephem Open ephemeris handle.
 * @param jd_tt TT Julian date.
 * @param state[6] Output: position (AU) + velocity (AU/day) in ICRS, Earth-centered.
 * @return JPL_OK on success.
 * @note To get Moon wrt another body (e.g., Sun), compute:
 *       moon_wrt_sun = moon_wrt_earth + earth_wrt_sun
 *       using jpl_moon_position + jpl_planet_position(ephem, jd_tt, JPL_EARTH, JPL_SUN, ...).
 */
jpl_status jpl_moon_position(const jpl_ephemeris *ephem, double jd_tt,
                             double state[6]);

/**
 * @brief Convert velocity from AU/day to km/s.
 */
void jpl_au_per_day_to_km_per_s(const double vel_au_day[3], double vel_km_s[3]);

/**
 * @brief Convert distance from km to AU.
 */
void jpl_km_to_au(double km, double *au);
```

### Behavior
- Input `jd_tt` → convert to TDB → light-time → aberration → ICRS output
- Uses `jpl_get_state_lighttime` + `jpl_apply_aberration` + `jpl_ecliptic_to_icrs`
- Uses existing `jpl_target` enum values (JPL_MERCURY=1 .. JPL_PLUTO=9, JPL_SUN=11)
- Returns `JPL_ERROR_ARGUMENT` for invalid planet/center

---

## 5. Thread Safety / Memory-Mapped I/O

### New Functions
```c
// Memory-mapped open (read-only, shareable across threads)
jpl_status jpl_open_mmap(const char *path, jpl_ephemeris **out_ephemeris);

// Clone handle for concurrent read access
jpl_status jpl_ephemeris_clone(const jpl_ephemeris *ephem, jpl_ephemeris **out_clone);

// Close (decrements refcount; unmaps at zero)
void jpl_close(jpl_ephemeris *ephem);
```

### Implementation
- `jpl_open_mmap`: `CreateFileMapping`/`MapViewOfFile` (Windows) or `mmap` (POSIX)
- `jpl_ephemeris` gains `refcount` and `mmap_base`/`mmap_size`
- `jpl_read_record`/`jpl_read_constants` read from mapped memory
- Thread-safe: multiple clones can read concurrently
- `jpl_close` decrements refcount; actual unmap at zero

---

## API Surface Summary

| Header | Functions | Dependencies |
|--------|-----------|--------------|
| `jpl.h` | `jpl_get_state_lighttime`, `jpl_apply_aberration`, `jpl_open_mmap`, `jpl_ephemeris_clone` | Core only / Platform mmap |
| `jpl_frames.h` | `jpl_ecliptic_to_icrs`, `jpl_icrs_to_ecliptic`, `jpl_frame_rotation_matrix`, `jpl_tt_to_tdb`, `jpl_tdb_to_tt` | Embedded ERFA |
| `jpl_convenience.h` | `jpl_planet_position`, `jpl_moon_position`, `jpl_au_per_day_to_km_per_s`, `jpl_km_to_au` | All above |

---

## Documentation

All new public functions will have **Doxygen-style comments** in headers (`/** ... */` with `@param`, `@return`, `@retval`, `@note`). **Existing `jpl.h` functions will also be updated with Doxygen comments.** CMake option `LIBJPL_BUILD_DOCS` (OFF by default) to generate HTML via `doxygen`.

---

## CMake Changes

- Add `src/erfa/` to `jpl` target sources
- No new options (always build)
- Install `jpl_frames.h`, `jpl_convenience.h` alongside `jpl.h`
- Optional: `option(LIBJPL_BUILD_DOCS "Build Doxygen docs" OFF)` → `add_custom_target(docs ...)`

---

## Testing

| Feature | Test Approach |
|---------|---------------|
| Light-time | Compare against JPL Horizons for known dates |
| Aberration | Analytic test: known velocity → known aberration |
| Frames | Round-trip ecliptic→ICRS→ecliptic ≈ identity |
| Convenience | End-to-end vs. manual chain |
| Thread safety | Concurrent reads from multiple threads |
| Doxygen | `doxygen --version` check; parse warnings as errors in CI |

---

## License Compliance

- ERFA BSD-3 license file copied to `src/erfa/LICENSE`
- `README.md` updated with ERFA attribution
- All new code MIT (project license)