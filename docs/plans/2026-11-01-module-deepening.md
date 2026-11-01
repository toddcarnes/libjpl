# Module Deepening Implementation Plan

**Goal:** Implement the six deepening opportunities from the 2026-11-01 architecture review, in dependency order, turning shallow modules into deep ones (more behaviour behind smaller interfaces). Sequence: test-bed foundation (LayoutDescriptor), date seam (`Jd` discipline), engine (RecordEvaluator), geometry (EphemerisGeometry), frames (EpochFrame), pipeline (ApparentPlace).

**Architecture:** Behavior-preserving refactors only — no public API changes, no numerical changes. New modules live behind the existing interfaces: file-static functions or declarations in `src/jpl_private.h` (internal; tests already include it). Horizons goldens and the 46-test suite are the tripwires: any residual change fails the step.

**Tech Stack:** C17 (`C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`, ERFA v2.0.1 (vendored, never modified).

**Spec:** Architecture review `architecture-review-20261101.html` (temp dir, not committed), candidates 1–6; full sequence section of that report.

## Global Constraints

- C17 with `C_EXTENSIONS OFF` (enforced by CMake).
- MSVC: `/W4 /permissive-` zero warnings; GCC **and Clang**: `-Wall -Wextra -Wpedantic` zero warnings (Clang also enforces `-Wnewline-eof`; keep trailing newlines).
- Public headers (`include/jpl.h`, `include/jpl_frames.h`, `include/jpl_convenience.h`) must not change — no semver impact, 0.7.0 stands.
- ERFA vendored sources must not be modified; raw ERFA entry points are called from exactly one place per concern after each step.
- Two-part date arithmetic stays compensated; public/legacy on-disk scalar epochs untouched.
- Physics constants only from `src/jpl_private.h`.
- Transactional outputs: caller buffers unchanged on failure (existing tests enforce this; keep enforcing it).
- Thread-safety model unchanged: mmap shareable, FILE handles need `jpl_ephemeris_clone()`.
- All 46 existing tests must pass after every step (the ratchet). New tests may be added; existing assertions must not be weakened.
- One branch per step (`refactor/<slug>`), merged with `git merge --no-ff` only when its verification matrix is green; delete the branch after merge.
- CHANGELOG `Unreleased` section gains one bullet per step as it lands.

## Verification matrix (every step)

```powershell
cmake -S . -B build
cmake --build build --config Debug
$env:JPLEPH='D:\ephem\jpl\bin'; ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
$env:JPLEPH='D:\ephem\jpl\bin'; ctest --test-dir build -C Release --output-on-failure
```

```bash
# WSL Ubuntu (GCC default cc; Clang installed 2026-11-01)
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_BUILD_TESTS=ON
cmake --build build_linux -j4   # zero warnings required
JPLEPH=/mnt/d/ephem/jpl/bin ctest --test-dir build_linux --output-on-failure
CC=clang cmake -S . -B build_clang -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_BUILD_TESTS=ON
cmake --build build_clang -j4   # zero warnings required (incl. -Wnewline-eof)
JPLEPH=/mnt/d/ephem/jpl/bin ctest --test-dir build_clang --output-on-failure
# Release: reconfigure a second tree with -DCMAKE_BUILD_TYPE=Release and repeat
```

## Glossary

New module/role names are recorded in `GLOSSARY.md` (created alongside this plan). Use those terms in code comments and commit messages.

---

### Step 0: Sketch the consumer-facing seams (no code)

**Files:**
- Modify: none (design notes live in this plan until implementation)

**Interfaces (draft — finalize at implementation):**
- Consumes: candidate-3 needs (review §"Full sequence")
- Produces (sketches only):
  - `jpl_status jpl_geometry_compute(const jpl_ephemeris *e, const jpl_date *tdb, int target, int center, jpl_geometry *out);` where `jpl_geometry` is `{ jpl_state rel_state; double light_time_days; double observer_vel[3]; double sun_vec[3]; }` with emission/reception kept in distinct fields so mixing them is visible at the type level
  - Normalized-date discipline: entry points normalize; internal code assumes normalized pairs (debug-asserted, as `jpl_date_compare` already does)
  - `EpochFrame` rotation entry shape as candidate 3's final stage wants it: `rotate(state, which_epoch /*mean|true*/)`

- [ ] **Step 0.1: Record the three sketches above in the step branches that need them (no implementation)**
- [ ] **Step 0.2: Confirm no public-header change is required for any sketch (0.7.0 stands)**

Acceptance: each of Steps 2/4/5/6 cites its sketch; no sketch may demand a public signature change (defer to a later release if it does).

---

### Step 1: LayoutDescriptor — one layout for parser and test mocks

**Files:**
- Modify: `src/jpl_private.h` (add `jpl_layout` descriptor + `jpl_layout_parse`), `src/jpl.c` (`validate_layout`, `parse_header` consume it), `tests/jpl_test_util.h` (`test_put_ptr`, `jpl_test_mock` built from it), per-TU slot lore (`tests/test_numeric.c`, `tests/test_lighttime.c`, `tests/test_convenience.c`)
- Test: existing `tests/test_parser.c`, `tests/test_fixture*.c`, full suite

**Interfaces:**
- Consumes: raw header bytes + byte order
- Produces (internal, `src/jpl_private.h`):
  - `typedef struct jpl_layout { size_t width_bytes; size_t record_doubles; uint32_t constant_count; jpl_pointer pointers[JPL_TARGET_COUNT]; jpl_pointer libration; int has_tt_tdb; jpl_pointer tt_tdb; ... } jpl_layout;`
  - `jpl_status jpl_layout_parse(const unsigned char *hdr, size_t len, jpl_byte_order order, jpl_layout *out);`
  - `int test_mock_write_layout(unsigned char *rec, const jpl_layout *layout, int big_endian);` (test-side adapter: descriptor → bytes)

- [ ] **Step 1.1: Baseline — full matrix green on `main` (record the 6 leg results)**
- [ ] **Step 1.2: Add `jpl_layout` + `jpl_layout_parse` to `src/jpl_private.h` / `src/jpl.c`; keep `parse_header` behaviour identical (new code path, old code still called; assert both agree in debug builds)**
- [ ] **Step 1.3: Migrate `test_put_ptr` / `jpl_test_mock` to build from a `jpl_layout`; remove hardcoded `2696+idx*12` and the `TT_TDB_TAIL_BASE+6*extra` re-derivations**
- [ ] **Step 1.4: Remove the old parse path once the agreement assertion has passed a full matrix; per-TU slot lore now references `JPL_FILE_SLOT_EMB`-style names only**
- [ ] **Step 1.5: Add a round-trip test: descriptor → mock bytes → parse → identical descriptor (new test in `tests/test_parser.c`)**
- [ ] **Step 1.6: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: no test mock changes behaviour (goldens byte-identical); offset constants exist exactly once (`src/jpl_private.h`); 46/46 on all legs.
Risks/rollback: parser is load-bearing — the Step 1.2 agreement assertion is the guard; revert to the old path (kept until 1.4) on any golden mismatch.

---

### Step 2: Normalized-date discipline (`Jd` without a new public type)

**Files:**
- Modify: `src/jpl_date.c`, `src/jpl_private.h` (normalize-on-entry helper + debug assertions), call sites in `src/jpl_frames.c`, `src/jpl_convenience.c`, `src/jpl.c` (replace ad-hoc normalize/compare/alias patterns)
- Test: existing `tests/test_dates.c` plus new wrong-site-normalization tests

**Interfaces:**
- Consumes: public `jpl_date` (dumb struct, unchanged — C aggregates cannot enforce construction)
- Produces (internal):
  - `jpl_status jpl_date_normalize_entry(const jpl_date *in, double out[2]);` — the single trust-boundary normalizer; every public entry point funnels through it
  - `JPL_ASSERT_NORMALIZED(pair)` debug assertion at the top of internal consumers (`compare`, `subtract`, `partition`, `add_scaled` already assume it; `jpl_date_compare` has one — extend the pattern)
  - Keep `jpl_date_to_pair` for the ERFA FFI edge only; delete the redundant per-site conversions

- [ ] **Step 2.1: Audit every public entry point: date input must pass the single normalizer exactly once (list sites in the commit message)**
- [ ] **Step 2.2: Add debug assertions to internal consumers; run Debug matrix — no assertion may fire (a firing assertion is a wrong-site bug, fix the site, not the assert)**
- [ ] **Step 2.3: Remove redundant normalizations the audit exposes; keep FFI-edge conversions**
- [ ] **Step 2.4: New tests: unnormalized-pair rejection at each entry category (ARGUMENT), in-place aliasing (`normalize(x, x)`), equivalent-split agreement (already covered — keep)**
- [ ] **Step 2.5: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: `grep` shows no `jpl_date_normalize` call outside entry funnels + the helper itself; Debug matrix fires zero assertions; 46/46 everywhere.
Risks/rollback: assertion fires reveal latent wrong-site bugs — that is the point; fix forward, do not relax the assert.

---

### Step 3: RecordEvaluator — deepen the interpolation cluster

**Files:**
- Modify: `src/jpl.c` (`select_record`, `interp_*` helpers, `interpolate`, `barycentric_state` → one evaluator behind a single seam; helpers become file-static implementation detail)
- Test: existing `tests/test_jpl.c`, `tests/test_lighttime.c`, `tests/test_convenience.c`, `tests/test_fixture*.c`; new evaluator-level white-box tests via `jpl_private.h` inclusion (existing pattern)

**Interfaces:**
- Consumes: `jpl_ephemeris`, normalized date pair (Step 2), `jpl_pointer`
- Produces (internal, file-static type + one entry):
  - `jpl_status jpl_evaluate(const jpl_ephemeris *e, const double jd[2], const jpl_pointer *p, int components, jpl_state *out);` — owns buffer/cache/bounds/subinterval/Chebyshev/publish atomically (publish-or-nothing preserves the transactional-output contract, including cache-poisoning rule)

- [ ] **Step 3.1: Move the 7 helpers behind `jpl_evaluate` without changing their bodies (pure relocation commit; suite must stay green)**
- [ ] **Step 3.2: Fold the threading state (record_index, sub_origin, normalized, cache flag) into the evaluator; delete parameters that only threaded it**
- [ ] **Step 3.3: Re-express `barycentric_state` (EMB/Earth/Moon derivation) as evaluator calls; keep the `divisor` re-check**
- [ ] **Step 3.4: New white-box tests: last-boundary clamp, `start-1` slot edges, RANGE-vs-UNSUPPORTED ordering, cache-not-poisoned-on-failure (observable via second-query success, not timing)**
- [ ] **Step 3.5: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: `interpolate` is a thin caller of `jpl_evaluate`; no caller outside the evaluator names subintervals, slots, or cache state; 46/46 everywhere.
Risks/rollback: largest surgery in the sequence — Step 3.1's pure-relocation commit is the bisect point; any golden drift reverts to it, not to `main`.

---

### Step 4: EphemerisGeometry — atomic light-time geometry

**Files:**
- Modify: `src/jpl.c` (solver internals stay; wrap the composition), `src/jpl_convenience.c` (`observer_geometry` + the inline duplicate in `apparent_position_at_tt` both become calls), `src/jpl_private.h` (geometry struct decl)
- Test: existing `tests/test_lighttime.c`, `tests/test_convenience.c`, `tests/test_api_errors.c` (budget-hook block); new atomicity tests

**Interfaces:**
- Consumes: Step-3 evaluator, Step-2 normalized dates
- Produces (internal, `src/jpl_private.h`):
  - `typedef struct jpl_geometry { jpl_state rel_state; double light_time_days; double observer_vel[3]; double sun_vec[3]; jpl_date emission; } jpl_geometry;` — emission stored explicitly so a reception/emission mixup is visible, not ambient
  - `jpl_status jpl_compute_geometry(const jpl_ephemeris *e, const jpl_date *tdb, int target, int center, jpl_geometry *out);` — owns coverage-then-read ordering and the subluminal `FORMAT` contract once
  - Fast-vs-bracket policy and the `LIBJPL_TESTING` budget hook stay behind this seam (hook signature unchanged)

- [ ] **Step 4.1: Add `jpl_geometry` + `jpl_compute_geometry` as a wrapper over the current solver path (no behaviour change; both convenience call sites converted to it, duplicate deleted)**
- [ ] **Step 4.2: Push the coverage/read ordering and subluminal gate inside; convenience layer performs no checks of its own**
- [ ] **Step 4.3: New tests: geometry atomicity (identical inputs → identical struct, field-by-field), stale-`lt` regression (fallthrough path publishes the solved value, never the trial), mixed-epoch velocity pinned to `v_target(emission) - v_center(reception)`**
- [ ] **Step 4.4: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: exactly one `observer_geometry`-shaped composition in the tree; convenience performs zero light-time logic; 46/46 everywhere.
Risks/rollback: wrapper-first (4.1) keeps the old path one revert away until 4.2 lands.

---

### Step 5: EpochFrame — collapse the frame-wrapper prologues

**Files:**
- Modify: `src/jpl_frames.c` (6 entries → one parameterized transform + thin public adapters), `src/jpl_private.h` (frame struct decl)
- Test: existing `tests/test_frames.c`, `tests/test_numeric.c` (pinned references); no new goldens needed

**Interfaces:**
- Consumes: Step-2 normalized dates; candidate-3's final stage is the designing consumer (real seam, not hypothetical)
- Produces (internal):
  - `typedef struct jpl_epoch_frame { jpl_date tt; double mean[3][3]; double true_[3][3]; } jpl_epoch_frame;`
  - `jpl_status jpl_epoch_frame_create(const jpl_date *tt, jpl_epoch_frame *out);` (caches both matrices once)
  - `jpl_status jpl_epoch_frame_rotate(const jpl_epoch_frame *f, int true_equinox, const jpl_state *in, jpl_state *out);`
  - Public `jpl_ecliptic_to_icrs` / `jpl_icrs_to_ecliptic` / `jpl_frame_rotation_matrix` / `jpl_frame_context_*` stay as **compatibility adapters** (explicitly marked) — no public API change in 0.7.x

- [ ] **Step 5.1: Build `jpl_epoch_frame` behind the existing entries (adapters delegate; no behaviour change)**
- [ ] **Step 5.2: Move the shared prologue (validate/pair/normalize/finiteness) inside once; adapters pass through**
- [ ] **Step 5.3: Document the `dR/dt` velocity omission at the new seam (it is currently silent); no behaviour change**
- [ ] **Step 5.4: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: one prologue in the tree; public signatures and goldens byte-identical; 46/46 everywhere.
Risks/rollback: lowest-risk step; adapters make revert trivial. Ranked here (not earlier) so its interface is shaped by candidate 3's real needs.

---

### Step 6: ApparentPlace pipeline — newtypes per stage

**Files:**
- Modify: `src/jpl_convenience.c` (`apparent_position_at_tt`, rate estimator), `src/jpl_private.h` (stage types)
- Test: existing `tests/test_convenience.c`, `tests/test_horizons.c` (goldens must not move); new per-stage tests

**Interfaces:**
- Consumes: Step-4 geometry, Step-2 dates, Step-5 frame rotation
- Produces (internal):
  - `typedef struct jpl_direction { double v[3]; } jpl_direction;` — constructor `jpl_direction_from_raw` enforces finite + unit (the fixed 138° bug becomes a construction-time failure)
  - Stage functions: `geometric → jpl_direction → jpl_apply_deflection → renormalize → jpl_apply_aberration → jpl_to_true_ecliptic → jpl_apparent_ecliptic_result`
  - Rate estimator extracted as a separate strategy object (one-sided/adaptive-`h` policy testable without ephemeris data)

- [ ] **Step 6.1: Introduce `jpl_direction` at the deflection input only (constructor + the one call site; suite must stay green)**
- [ ] **Step 6.2: Thread stage types through the pipeline; delete manual `/distance` renormalizations in favour of constructor guarantees**
- [ ] **Step 6.3: Extract the rate estimator; add edge-of-coverage unit tests (one-sided/adaptive-`h` branches currently golden-only)**
- [ ] **Step 6.4: Add a regression test shaped exactly like the fixed bug (non-unit or single-component direction input must fail construction, never reach `eraLd`)**
- [ ] **Step 6.5: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: no bare `double[3]` crosses a stage seam; Horizons goldens unchanged (refactor, not model change); 46/46 everywhere.
Risks/rollback: behaviour must be bit-identical — the Horizons tests are the tripwire; any residual drift fails the step.

---

## Execution Notes

- Implement in order 0→6 (Step 0 is design notes, no branch). Steps 1 and 2 are independent of each other and may overlap; both must land before Step 3.
- One branch per step (`refactor/<slug>`), `git merge --no-ff` into `main` only when the verification matrix is fully green; delete the branch after merge (repo convention).
- If any test fails, STOP and debug before continuing — the ratchet is the point.
- Do not touch `src/erfa/`, do not change public headers, do not weaken assertions to make a step pass.
- Newly coined module names go in `GLOSSARY.md` as they land (created with this plan).

### Step B: Bindings migration to the struct API (do first)

**Rationale:** The 0.7.0 struct migration left all four language bindings calling the old array API — and calling `jpl_apply_aberration_full`, which no longer exists. The C++ binding fails to compile under CI's warnings-as-errors; the Python cffi `cdef` references a missing export (import-time failure); C#/VB.NET P/Invoke a missing entry point (call-time failure). `ci.yml` requires all four binding tests registered and passing, so the bindings legs fail on the next push. This step is independent of C1–C6 (public C API is stable) and unblocks CI, so it goes first.

**Strategy (applies to all four bindings): keep each binding's public surface byte-for-byte identical; convert to `jpl_date`/`jpl_state` only at the FFI boundary.** Binding tests and examples then run unmodified and become regression oracles proving behavior preservation. The removed `..._full` variants forward to unified `jpl_apply_aberration` (with the Sun distance, or `0.0` to omit the term).

**Files:**
- Modify: `bindings/cpp/include/jpl/ephemeris.hpp` (internal `jpl_date`/`jpl_state` locals; `_full` forwards with `sun_distance_au`; 3-arg `apply_aberration` passes `0.0`)
- Modify: `bindings/python/src/libjpl/_libjpl.py` (cdef gains `jpl_date`/`jpl_state` structs; all signatures updated; `_full` decl removed), `bindings/python/src/libjpl/ephemeris.py` (list↔struct conversion at boundaries; `_full` method forwards)
- Modify: `bindings/csharp/src/LibJpl/Native.cs` (blittable `JplDate`/`JplState` structs; all P/Invokes updated; `_full` import removed), `bindings/csharp/src/LibJpl/JplEphemeris.cs` (internal conversions; `ApplyAberration` passes `0.0`)
- Modify: `bindings/vbnet/src/LibJpl/Native.vb` (`Structure JplDate/JplState`; imports updated; `_full` import removed), `bindings/vbnet/src/LibJpl/JplEphemeris.vb` (internal conversions)
- Modify (docs consistency, same root cause): `include/jpl.h` `@note` (~line 398), `include/jpl_convenience.h` `@note` (~line 52) — both still name the deleted `jpl_apply_aberration_full`; `bindings/c/examples/README.md` C snippets (raw C API); `docs/migration-two-part-dates.md` (add a 0.7.0 "arrays → structs" section; README links it as the usage contract)
- Unchanged (verify by running, not editing): all binding tests, all binding examples, public binding surfaces

- [ ] **Step B.1: C++ — convert `ephemeris.hpp` internals; keep every public signature; build the binding test with `-Werror` (MSVC `/WX`, GCC/Clang `-Werror`) and run `jpl_cpp_binding`**
- [ ] **Step B.2: Python — update cdef + `ephemeris.py` conversions; run `jpl_python_binding` (needs shared lib + cffi)**
- [ ] **Step B.3: C# — update `Native.cs` + `JplEphemeris.cs`; run `jpl_csharp_binding` (needs dotnet SDK + NuGet restore)**
- [ ] **Step B.4: VB.NET — update `Native.vb` + `JplEphemeris.vb`; run `jpl_vbnet_binding`**
- [ ] **Step B.5: Docs consistency — the two header `@note` fixes, the `bindings/c` README snippets, the migration-doc 0.7.0 section**
- [ ] **Step B.6: Configure with all four bindings + warnings-as-errors (CI-like) and run the full matrix; if a language toolchain is unavailable locally, document the gap and let CI cover it**
- [ ] **Step B.7: CHANGELOG `Unreleased` bullet (bindings work on 0.7.0 API); merge**

Acceptance: `ci.yml` bindings jobs would go green (registration proof finds all four tests; all pass); binding tests/examples byte-identical to before; 46/46 on the default matrix.
Risks/rollback: per-language commits if possible; any binding that cannot be verified locally (missing SDK) gets extra review scrutiny, not a weakened test.
