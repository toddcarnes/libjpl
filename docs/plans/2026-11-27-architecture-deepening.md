# Architecture Deepening Implementation Plan

**Goal:** Implement the eight deepening opportunities from the 2026-11-27 architecture review, in dependency order: light-time solution, fixture builder, verified epoch, record store, TT-TDB table, body catalog, record schedule, header access. The sequence turns the remaining shallow seams into deep modules while keeping the scientific behavior fixed and the binding surface deliberate.

**Architecture:** Behavior-preserving refactors except two deliberate public changes: one additive light-time value and one opaque-header accessor set. New modules live behind private interfaces (`src/jpl_private.h` declarations or file-static code); new private translation units carry only modules with an independent test surface. Handles remain opaque; the thread model is unchanged. Goldens and the full suite are the tripwires: any residual numerical change fails the step.

**Tech Stack:** C17 (`C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic` (Clang also enforces `-Wnewline-eof`; keep trailing newlines), ERFA v2.0.1 (vendored, never modified).

**Spec:** Architecture review `architecture-review-20261127.html` (temp directory, not committed), candidates 1-8. The review's before/after diagrams are the design rationale; this plan is the durable record.

## Global Constraints

- C17 with `C_EXTENSIONS OFF` (enforced by CMake).
- MSVC: `/W4 /permissive-` zero warnings; GCC **and** Clang: `-Wall -Wextra -Wpedantic` zero warnings.
- Public headers: exactly two deliberate changes in this sequence:
  1. Additive `jpl_lighttime_solution` + `jpl_get_lighttime_solution()` (Step 1).
  2. Opaque `jpl_header` + eight grouped accessors; `jpl_get_header()` deleted (Step 8, gated). The fallback is a versioned snapshot struct with `version`/`struct_size` fields.
  No other public additions or deletions.
- Every step except Step 1's documented emission correction and Step 8 keeps all numerical goldens byte-identical. Step 1 may move apparent-place last bits only by the solver residual; any reference update must be measured, within the existing scientific comparison tolerances, and recorded in the merge message. Existing assertions must not be weakened.
- Two-part date arithmetic stays compensated; public/legacy on-disk scalar epochs untouched.
- Physics constants only from `src/jpl_private.h`.
- Transactional outputs: caller buffers unchanged on failure (existing tests enforce this; keep enforcing it).
- Thread-safety model unchanged: mmap handles shareable, FILE handles need `jpl_ephemeris_clone()` per thread, the record cache is per-handle, no locks. `RecordStore` may move where the cache lives, never its concurrency rules.
- One branch per step (`refactor/<slug>`; Step 1 may use `feat/lighttime-solution`), merged with `git merge --no-ff` only when the verification matrix is green and the maintainer has approved. Delete the branch after merge.
- `CHANGELOG.md` `Unreleased` gains one bullet per step as it lands.
- Newly coined module names go in `GLOSSARY.md` as they land.
- Every new source/header/test file carries the MIT license header and is added to `tests/license_inventory.cmake`.
- Version: lands at **0.12.0** at sequence end (the additive light-time value plus the header change justify a minor bump). Tag `v0.12.0` only after the full matrix passes. If Step 8 falls back to the versioned snapshot, the additive value alone still warrants 0.12.0.
- Do not modify `src/erfa/`.

## Verification Matrix (every step)

Default suite (46 tests currently) plus a bindings-enabled suite (50 tests: 46 + the four language bindings), Debug and Release, zero warnings. Horizons goldens and `tests/data/numerical_reference.h` byte-identical after every step except Step 1's measured emission correction and Step 8. Record the actual leg results in the merge commit message; a clean run is not a semantic review.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

```powershell
cmake -S . -B build_bind -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_CPP_BINDING=ON -DLIBJPL_BUILD_PYTHON_BINDING=ON -DLIBJPL_BUILD_CSHARP_BINDING=ON -DLIBJPL_BUILD_VBNET_BINDING=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build_bind --config Debug
ctest --test-dir build_bind -C Debug --output-on-failure
cmake --build build_bind --config Release
ctest --test-dir build_bind -C Release --output-on-failure
```

```bash
# WSL Ubuntu (GCC default cc; Clang installed 2026-11-01)
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_BUILD_TESTS=ON
cmake --build build_linux -j4
ctest --test-dir build_linux --output-on-failure
CC=clang cmake -S . -B build_clang -DCMAKE_BUILD_TYPE=Debug -DLIBJPL_BUILD_TESTS=ON
cmake --build build_clang -j4
ctest --test-dir build_clang --output-on-failure
```

Sanitizers (ASan/UBSan, float-cast-overflow) at the sequence tip; TSan for `jpl_threads` and `jpl_mmap` after any storage change. Docs checks with Doxygen `WARN_AS_ERROR=YES`.

## Public API Delta

Added in Step 1:

```c
typedef struct jpl_lighttime_solution {
    jpl_state rel_state;       /* target - center at emission, AU / AU per day */
    jpl_state observer_state;  /* center barycentric state at reception, AU / AU per day */
    double light_time_days;    /* solved retarded-minus-reception interval */
    jpl_date emission;         /* TDB epoch used for rel_state */
} jpl_lighttime_solution;

JPL_API jpl_status jpl_get_lighttime_solution(const jpl_ephemeris *ephemeris,
                                              const jpl_date *jd_tdb,
                                              int target, int center,
                                              jpl_lighttime_solution *out_solution);
```

`jpl_get_state_lighttime()` stays as a wrapper that maps the solution to `state` plus optional delay.

Changed in Step 8 (gated):

```c
typedef struct jpl_header jpl_header;   /* opaque; definition moves private */

JPL_API jpl_status jpl_header_titles(const jpl_ephemeris *, char titles[3][JPL_TITLE_LINE_LENGTH + 1]);
JPL_API jpl_status jpl_header_coverage(const jpl_ephemeris *, jpl_date *start, jpl_date *end, double *step_days);
JPL_API jpl_status jpl_header_shape(const jpl_ephemeris *, int *constant_count, size_t *record_double_count, jpl_byte_order *byte_order);
JPL_API jpl_status jpl_header_identity(const jpl_ephemeris *, int *ephemeris_number, jpl_reference_frame *reference_frame);
JPL_API jpl_status jpl_header_scalars(const jpl_ephemeris *, double *astronomical_unit_km, double *earth_moon_mass_ratio, int *has_tt_tdb);
JPL_API jpl_status jpl_header_pointer(const jpl_ephemeris *, int slot, jpl_pointer *out_pointer);
JPL_API jpl_status jpl_header_libration_pointer(const jpl_ephemeris *, jpl_pointer *out_pointer);
JPL_API jpl_status jpl_header_tt_tdb_pointer(const jpl_ephemeris *, jpl_pointer *out_pointer);
```

`jpl_get_constant_name()` remains the name accessor; `jpl_pointer` remains public. Every other function keeps its signature and semantics.

## Glossary

New module/role names, recorded in `GLOSSARY.md` as each step lands:

- **VerifiedEpoch**: the gate-issued data epoch (normalized pair plus coverage bounds) after any domain conversion. Implemented as `jpl_verified_epoch` and produced by the EntryGate coverage step.
- **LightTimeSolution**: the atomic solver value `{rel_state, observer_state, light_time_days, emission}`; public type `jpl_lighttime_solution`.
- **RecordStore**: the module owning source selection, decoded record views, and the per-handle record cache. HandleReader remains the raw byte transport beneath it.
- **RecordSchedule**: the module owning scheduled endpoints and the epoch -> record -> subinterval mapping.
- **BodyCatalog**: the module owning body validity and execution plans per query mode.
- **TtTdbTable**: the validated TT-TDB availability table built once at open.
- **FixtureBuilder**: the test-side DE fixture adapter (`tests/jpl_fixture.h` / `.c`).
- **HeaderAccess**: the opaque `jpl_header` accessor set.

---

## Step 1: Light-time solution (candidate 2) — FIRST

**Status:** implemented on `feat/lighttime-solution`. MSVC Debug/Release 46/46 default and 50/50 bindings, zero warnings; WSL GCC Debug 46/46 and WSL Clang 14 Debug 46/46, zero warnings. No numerical reference or Horizons golden update was required; all comparisons stayed within the existing tolerances.

**Rationale for position:** Smallest high-value step; the solver already computes the observer state and accepted emission, so returning them removes a re-read and a reconstructed epoch before the larger core surgery. It also proves the sequence.

**Files:**
- Modify: `src/jpl.c` (`lighttime_solve`, `jpl_get_state_lighttime`, new public function), `include/jpl.h` (public struct + declaration), `src/jpl_convenience.c` (`jpl_compute_geometry`), `src/jpl_private.h` (internal solution declaration if needed)
- Modify: `bindings/cpp/include/jpl/ephemeris.hpp`, `bindings/python/src/libjpl/ephemeris.py` (and `_libjpl.py` cdef if needed), `bindings/csharp/src/LibJpl/JplEphemeris.cs`, `bindings/vbnet/src/LibJpl/JplEphemeris.vb`
- Test: `tests/test_lighttime.c`, `tests/test_api_errors.c`, `bindings/*/tests/*`, `tests/public_symbols.txt`, `docs/api.dox`

**Seam sketch (finalize at implementation):**
- Consumes: target/center/epoch, the existing solver budgets.
- Produces (internal and public): one atomic `jpl_lighttime_solution` in public units (`rel_state` AU, `observer_state` AU/day, `light_time_days`, `emission` TDB).
- `lighttime_solve` fills the solution from the values it already holds; `jpl_get_lighttime_solution()` publishes it; `jpl_get_state_lighttime()` maps it to the existing outputs.
- `jpl_compute_geometry` copies `rel_state`, `observer_state`, and `emission` from the solution; deletes its second `jpl_get_state()` observer read and its `jpl_date_add_days` emission reconstruction.

- [x] **Step 1.1: Baseline** — full matrix green on `main`; record the leg results.
- [x] **Step 1.2: Add the public value and function; map the existing wrapper** (no behavior change; geometry still uses its old path until 1.3).
- [x] **Step 1.3: Fill observer state and emission inside the solver; convert units once; delete geometry's reconstruction.**
- [x] **Step 1.4: Add a call-count pin** (observer barycentric state evaluated once per solve) and an assertion that `solution.emission` is the epoch used for `rel_state`; measure the apparent-place delta caused by using the exact emission. If the delta exceeds the existing comparison tolerances, stop and revisit; otherwise record it and update references deliberately.
- [x] **Step 1.5: Expose the solution in all four bindings** with one drift assertion (emission equals the C result; observer state matches a direct barycentric read).
- [ ] **Step 1.6: Run the matrix; CHANGELOG bullet; merge.**

**Acceptance:** `jpl_compute_geometry` performs no observer re-read and no emission reconstruction; solution fields are in public units; `jpl_get_state_lighttime` outputs are byte-identical; any apparent-place delta is measured, within the existing comparison tolerances, and recorded; all four bindings expose the same value; 46+/50+ everywhere.

**Risks/rollback:** The exact-emission correction may move apparent-place last bits; the delta must be measured, not assumed. Unit conversion is the only other numerical risk. Any unexplained drift reverts Step 1.2 (the wrapper-only commit), not the whole plan.

---

## Step 2: FixtureBuilder (candidate 4) — SECOND

**Status:** foundation landed on `refactor/fixture-builder`. The builder and its LE/BE self-test are in place; `test_threads`, the `test_mmap` identity probes, the `test_lighttime` linear mocks, and the `test_numeric` polynomial/oracle mocks now declare bodies and series. The remaining behavior-test migrations (`test_convenience`, normal TT-TDB cases, `test_api_errors`, `make_validator_data`) are tracked as follow-up commits and do not block Step 3; parser/core/large-file tests stay raw by design.

**Rationale for position:** Test-only, behavior-neutral, and it makes the remaining six steps cheaper to test. Landing it second means the core surgery runs on the new harness instead of migrating tests later.

**Files:**
- Add: `tests/jpl_fixture.h`, `tests/jpl_fixture.c`, `tests/jpl_fixture_selftest.c` (or a self-test inside `test_parser.c`)
- Modify: `tests/jpl_test_util.h` (keep raw byte helpers and escape hatches), `tests/test_threads.c`, `tests/test_mmap.c`, `tests/test_lighttime.c`, `tests/test_numeric.c`, `tests/test_convenience.c`, `tests/test_tt_tdb.c` (normal cases), `tests/test_api_errors.c`, `tests/make_validator_data.c`
- Modify: `CMakeLists.txt` (compile the fixture unit into migrated tests, register the self-test), `tests/license_inventory.cmake`

**Seam sketch (finalize at implementation):**
- Consumes: a fixture specification in domain terms — coverage, step, DENUM, record width, constants `{name, value}`, bodies with coefficient series, angular targets, TT-TDB mode, endian, trailing record.
- Produces: a valid DE file on disk (or bytes) with the existing `JPL_HDR_*` constants and `jpl_tail_offset()` as the only layout source.
- Capability set: constant / linear / polynomial series; per-record coefficients; EMB + geocentric Moon and derived Earth/Moon; nutations/librations including the libration subinterval word; TT-TDB absent / present / invalid span / overflow-name tail; zero or explicit constants; >400 names; big-endian; trailing record; raw patch hook for deliberate mutations.
- Parser, core open/format, and large-file tests keep raw byte construction; the builder does not replace `test_parser.c`'s negative cases or `test_largefile.c`'s sparse writer.

- [x] **Step 2.1: Add the builder and a self-test** (builder -> bytes -> parser round trip; no test migration yet).
- [x] **Step 2.2: Migrate `test_threads.c` and `test_mmap.c`** (the mmap identity probes; the raw-record main mock stays raw).
- [x] **Step 2.3: Migrate `test_lighttime.c` and `test_numeric.c`** (linear/polynomial/oracle mocks).
- [ ] **Step 2.4: Migrate `test_convenience.c`.** Follow-up.
- [ ] **Step 2.5: Migrate normal TT-TDB cases and `test_api_errors.c`;** keep raw layout construction for boundary-span, overflow-name, and T15 placement. Follow-up.
- [ ] **Step 2.6: Migrate `make_validator_data.c`;** keep raw negative corpora (NaN, truncation, ambiguous widths). Follow-up.
- [ ] **Step 2.7: Run the matrix; CHANGELOG bullet; merge.**

**Acceptance:** Migrated behavior tests declare bodies and series, not slot indices or constants-record layout; parser/core/large-file tests remain byte-level; every assertion and expected value is unchanged; the builder self-test pins the emitted header fields and record bytes; `grep` shows no new `test_put_ptr` uses in migrated tests.

**Risks/rollback:** A builder bug changes many fixtures at once. The self-test and per-file migration order keep the blast radius to one test file per commit; any expectation drift reverts that migration commit, not the builder.

---

## Step 3: VerifiedEpoch (candidate 1) — THIRD

**Rationale for position:** The gate landed in the previous sequence but its output never crosses an internal seam. This step makes validation structural before RecordStore and RecordSchedule change the evaluation path.

**Files:**
- Modify: `src/jpl_private.h` (add `jpl_verified_epoch`, update gate declarations), `src/jpl.c` (`jpl_entry_prepare`, `jpl_entry_coverage`, `jpl_evaluate`, `barycentric_state`, `lighttime_solve`, `jpl_get_tt_minus_tdb`), `src/jpl_convenience.c` (`convenience_epoch`)
- Test: `tests/test_api_errors.c` (gate matrix), new call-count assertions; existing light-time/convenience goldens

**Seam sketch (finalize at implementation):**

```c
typedef struct jpl_verified_epoch {
    double epoch[2];      /* normalized data-read epoch (TDB where applicable) */
    double cov_start[2];
    double cov_end[2];
} jpl_verified_epoch;

jpl_status jpl_entry_prepare(const jpl_ephemeris *e, const double jd[2],
                             jpl_entry_ctx *out);
jpl_status jpl_entry_coverage(const jpl_entry_ctx *ctx, const double data_epoch[2],
                              jpl_verified_epoch *out_verified);
```

- `jpl_entry_prepare` validates and normalizes the caller's epoch; the domain conversion (TT -> TDB) happens outside; `jpl_entry_coverage` checks the data epoch and issues the token.
- `jpl_evaluate`, `barycentric_state`, `lighttime_solve`, and `jpl_get_tt_minus_tdb` take `const jpl_verified_epoch *` instead of a raw pair.
- Test hooks (`jpl_test_select_record`, `jpl_test_lighttime_with_budget`) prepare and verify at their own boundary so they still exercise the funnel.

- [ ] **Step 3.1: Add the token and convert `jpl_evaluate`;** keep a thin raw-pair wrapper for the test hook.
- [ ] **Step 3.2: Convert `barycentric_state` and `lighttime_solve`;** remove repeated prepare/coverage calls.
- [ ] **Step 3.3: Convert `jpl_get_tt_minus_tdb` and `convenience_epoch`;** make the data epoch explicit.
- [ ] **Step 3.4: Add a gate-run call-count pin** (one gate per public query) and keep the precedence matrix green.
- [ ] **Step 3.5: Run the matrix; CHANGELOG bullet; merge.**

**Acceptance:** Every public query runs the gate once, not once per leg; internal legs never normalize or re-check coverage; `convenience_epoch` checks coverage on the TDB epoch and passes that exact token onward; 46+/50+ everywhere.

**Risks/rollback:** The TT -> TDB coverage point is the subtle one; any RANGE/UNSUPPORTED drift reverts Step 3.1, where the old per-leg gate remains.

---

## Step 4: RecordStore (candidate 3) — FOURTH

**Rationale for position:** The storage seam has three remaining bypasses and a cache policy outside it. It goes after VerifiedEpoch so the evaluation signature is settled, and before RecordSchedule/TtTdbTable so those modules consume one record source.

**Files:**
- Add: `src/jpl_record_store.c`, `src/jpl_record_store.h` (private, or declarations in `src/jpl_private.h`), `tests/test_record_store.c`
- Modify: `src/jpl.c` (`read_doubles`, `jpl_get_constant_name`, `interp_read_record_cached`, `interp_publish_state`, `jpl_open`, `jpl_open_mmap`, `jpl_ephemeris_clone`, `jpl_close`), `src/jpl_private.h`
- Modify: `CMakeLists.txt`, `tests/license_inventory.cmake`
- Test: existing `tests/test_threads.c`, `tests/test_mmap.c`, `tests/test_api_errors.c`; new direct store tests

**Seam sketch (finalize at implementation):**

```c
typedef struct jpl_record_view {
    const double *values;
    void *owned;          /* NULL for a direct/cache view; per-call decode buffer otherwise */
} jpl_record_view;

jpl_status jpl_store_init_file(jpl_record_store *store, FILE *file, size_t size);
jpl_status jpl_store_init_mmap(jpl_record_store *store, const unsigned char *base, size_t size);
jpl_status jpl_store_read_raw(const jpl_record_store *store, size_t offset,
                              void *out, size_t count);
jpl_status jpl_store_record(jpl_record_store *store, size_t index,
                            jpl_record_view *out_view);
void jpl_record_view_release(jpl_record_view *view);
```

- The store is embedded in the handle; it owns the source adapter, size, byte order, and cache.
- Native-order mmap: view points into the mapping. Native-order FILE: view points at the per-handle cache. Swapped: per-call decoded buffer owned by the view.
- Public raw reads still copy into caller buffers.
- `jpl_evaluate` no longer branches on `is_mmap` or casts away `const` to publish cache state.

- [ ] **Step 4.1: Extract the store with current copy semantics** (pure relocation; suite green).
- [ ] **Step 4.2: Move source selection and cache policy inside;** delete `is_mmap` branches from `read_doubles`, `jpl_get_constant_name`, and the interp helpers.
- [ ] **Step 4.3: Introduce borrowed record views** (mmap direct, FILE cache, swapped per-call); update the evaluator.
- [ ] **Step 4.4: Direct store tests** for bounds, endian, cache hit/miss, failed-read non-poisoning, and view lifetime.
- [ ] **Step 4.5: Run the matrix, including thread/clone tests; CHANGELOG bullet; merge.**

**Acceptance:** No `is_mmap` branch outside the store, open/clone/close, and the mapping helpers; no per-evaluation `malloc` for native-order records; public raw-read copy semantics unchanged; cache tests are direct and do not need interpolation; 46+/50+ everywhere.

**Risks/rollback:** Borrowed-view lifetime is the main hazard. Step 4.1 is the bisect point: it keeps the old copy semantics behind the new interface. Any thread or golden drift reverts to it.

---

## Step 5: TtTdbTable (candidate 5) - FIFTH

**Rationale for position:** It is small, independent, and fixes a public predicate that currently over-promises. It goes after RecordStore so the query path has a single record source when the table replaces the recheck.

**Files:**
- Add: `src/jpl_tt_tdb_table.c`, `tests/test_tt_tdb_table.c`; declarations in `src/jpl_private.h`
- Modify: `src/jpl.c` (`tt_tdb_span_end`, open publication, `jpl_has_tt_tdb`, `jpl_get_tt_minus_tdb`), `src/jpl_convenience.c` (`jpl_tt_to_tdb_policy`)
- Modify: `CMakeLists.txt`, `tests/license_inventory.cmake`
- Test: `tests/test_tt_tdb.c`, `tests/test_parser.c` (absent/partial/overflow triples)

**Seam sketch (finalize at implementation):**

```c
typedef struct jpl_tt_tdb_table {
    int present;          /* validated: span_end fits the record */
    jpl_pointer pointer;
    size_t span_end;
} jpl_tt_tdb_table;

jpl_status jpl_tt_tdb_table_build(const jpl_header *header, jpl_tt_tdb_table *out);
```

- Built once at open from the parsed header; stored on the handle.
- `jpl_has_tt_tdb()` returns `table.present`.
- `jpl_get_tt_minus_tdb()` consumes the table and never recomputes the span.
- `jpl_tt_to_tdb_policy()` refines only when `table.present`.

- [ ] **Step 5.1: Build the table at open;** keep the public flag and query behavior identical.
- [ ] **Step 5.2: Convert the public flag, query, and policy;** delete the per-call span check.
- [ ] **Step 5.3: Add direct table tests** for absent, valid, span-equal-to-width, partial, overflowing, and overflow-name-tail cases.
- [ ] **Step 5.4: Run the matrix; CHANGELOG bullet; merge.**

**Acceptance:** Exactly one availability predicate; `jpl_has_tt_tdb()` equals "a valid in-coverage `jpl_get_tt_minus_tdb()` can succeed" for every fixture; no per-call span recomputation; 46+/50+ everywhere.

**Risks/rollback:** The table build must accept every fixture the current query accepts. Any divergence reverts Step 5.1, where the old query recheck remains.

---

## Step 6: BodyCatalog (candidate 7) - SIXTH

**Rationale for position:** It touches the same query paths as VerifiedEpoch but is independent of storage. It goes after the storage/table steps so the catalog's plan can name the final record/pointer accessors.

**Files:**
- Add: `src/jpl_body_catalog.c`, `tests/test_body_catalog.c`; declarations in `src/jpl_private.h`
- Modify: `src/jpl.c` (`valid_state_target`, `valid_state_center`, `valid_lighttime_body`, `barycentric_state`, `jpl_get_state`), `src/jpl_convenience.c` (planet/center/apparent target checks)
- Modify: `CMakeLists.txt`, `tests/license_inventory.cmake`
- Test: `tests/test_api_errors.c` (precedence matrix), existing convenience tests

**Seam sketch (finalize at implementation):**

```c
typedef enum jpl_body_kind {
    JPL_BODY_DIRECT,      /* one file slot */
    JPL_BODY_EMB_MOON,    /* EMB plus geocentric Moon derivation */
    JPL_BODY_ANGULAR,     /* nutations or librations */
    JPL_BODY_ZERO         /* SSB / identical-body zero vector */
} jpl_body_kind;

typedef struct jpl_body_plan {
    jpl_body_kind kind;
    const jpl_pointer *primary;
    const jpl_pointer *secondary;
    size_t components;    /* 2 for nutations, 3 for librations, 3 for bodies */
} jpl_body_plan;

jpl_status jpl_body_plan_for(jpl_query_mode mode, const jpl_header *header,
                             int target, int center, jpl_body_plan *out);
```

- Modes: state, light-time, planet, apparent.
- Invalid target/center returns `JPL_ERROR_ARGUMENT`; the gate still runs after code validation so ARGUMENT precedes RANGE.
- `barycentric_state` executes the plan instead of hardcoding `target - 1`, `JPL_FILE_SLOT_EMB`, and `JPL_MOON - 1`.
- Convenience checks become `jpl_body_plan_for(planet/apparent, ...)` instead of raw ranges.

- [ ] **Step 6.1: Add the table and plans;** keep the old predicates as wrappers.
- [ ] **Step 6.2: Convert `barycentric_state` and the core state/light-time paths.**
- [ ] **Step 6.3: Convert the convenience target/center checks;** delete the raw range blocks and the unreachable `JPL_TT_TDB` branch.
- [ ] **Step 6.4: Pin the validity and precedence matrix in `test_body_catalog.c`;** run the existing API-error matrix unchanged.
- [ ] **Step 6.5: Run the matrix; CHANGELOG bullet; merge.**

**Acceptance:** One body validity/execution table; convenience performs no raw range checks; error precedence and every existing result unchanged; 46+/50+ everywhere.

**Risks/rollback:** Center rules are the subtle branch (`valid_state_center` order). Step 6.1 keeps the old predicates callable; any precedence drift reverts to it.

---

## Step 7: RecordSchedule (candidate 6) - SEVENTH

**Rationale for position:** It changes the evaluator internals last, after the gate, storage, and body plan are settled. The layout validator also consumes the endpoint helper, so it runs after the storage/table interface is stable.

**Files:**
- Add: `src/jpl_schedule.c`, `tests/test_schedule.c`; declarations in `src/jpl_private.h`
- Modify: `src/jpl.c` (`scheduled_endpoint`, `select_record`, `interp_verify_record_bounds`, `interp_find_subinterval`, `jpl_evaluate`)
- Modify: `CMakeLists.txt`, `tests/license_inventory.cmake`
- Test: `tests/test_api_errors.c` (selector hook), existing numeric/lighttime tests

**Seam sketch (finalize at implementation):**

```c
typedef struct jpl_record_span {
    size_t index;
    double start[2];
    double end[2];
} jpl_record_span;

typedef struct jpl_subinterval_span {
    size_t index;
    double origin[2];
    double substep;
    double argument;      /* normalized Chebyshev argument */
} jpl_subinterval_span;

jpl_status jpl_schedule_endpoint(const jpl_header *header, size_t index, double out[2]);
jpl_status jpl_schedule_record(const jpl_header *header, size_t covered_count,
                               const jpl_verified_epoch *epoch, jpl_record_span *out);
jpl_status jpl_schedule_verify_record(const double record_dates[2],
                                      const jpl_verified_epoch *epoch,
                                      jpl_record_span *out);
jpl_status jpl_schedule_subinterval(const jpl_record_span *span, size_t nsub,
                                    const jpl_verified_epoch *epoch,
                                    jpl_subinterval_span *out);
```

- The module owns scheduled endpoints, the quotient/one-neighbor/binary-search record lookup, record-bounds verification, and the subinterval partition walk.
- `jpl_evaluate` composes the calls and no longer re-derives boundaries.
- `validate_layout` uses `jpl_schedule_endpoint` for first/last/trailing endpoints.

- [ ] **Step 7.1: Extract `jpl_schedule_endpoint` and `jpl_schedule_record` with direct tests;** keep the evaluator's old path callable.
- [ ] **Step 7.2: Extract bounds verification and subinterval location;** convert the evaluator.
- [ ] **Step 7.3: Convert layout validation to the shared endpoint helper.**
- [ ] **Step 7.4: Add fallback tests** for the one-neighbor correction and binary-search branch (currently only one selector test).
- [ ] **Step 7.5: Run the matrix; CHANGELOG bullet; merge.**

**Acceptance:** Scheduled boundary arithmetic exists only in `jpl_schedule.c`; the evaluator is a composition; the selector fallback has direct tests; 46+/50+ everywhere and all goldens byte-identical.

**Risks/rollback:** Two-part date edge cases are the risk. Step 7.1 is a pure extraction; any drift reverts to it.

---

## Step 8: HeaderAccess (candidate 8) - EIGHTH, GATED

**Rationale for position:** It is the only public-surface step and should not churn the core sequence. It runs after the private modules settle, with the binding drift net and docs stable.

**Files:**
- Modify: `include/jpl.h` (opaque `jpl_header`, remove `jpl_get_header`, add eight accessors), `src/jpl.c` (move struct definition private, add accessors, keep `headers_equal` internal), `src/jpl_private.h`, `src/jpl_convenience.c`
- Modify: all four bindings — C++ `ephemeris.hpp`, Python `_libjpl.py` + `ephemeris.py`, C# `Native.cs` + `LibJpl.cs` + `JplEphemeris.cs`, VB `Native.vb` + `LibJpl.vb` + `JplEphemeris.vb`
- Modify: all C tests that dereference `jpl_get_header(e)->field` (13 files), `tests/public_symbols.txt`, `docs/api.dox`, `README.md`, `docs/migration-two-part-dates.md`, `docs/validation.md`, `bindings/CONTRACT.md`, `bindings/*/examples/README.md`
- Add: `docs/migration-header-access.md`, `docs/adr/0001-opaque-header-accessors.md`
- Test: `bindings/*/tests/*` (replace layout assertions with accessor parity), `tests/test_parser.c` and other C header readers, `tests/cmake/test_install.cmake` if the installed header set changes (it should not)

**Seam sketch (finalize at implementation):**
- The public `jpl_header` becomes opaque; the definition moves private.
- The grouped accessors in the "Public API Delta" section replace `jpl_get_header`.
- `jpl_get_constant_name` remains the name accessor; `jpl_pointer` remains public.
- Binding public `Header` classes/properties keep their existing field surface; their internals call accessors instead of marshalling a 3320-byte struct.
- C#/VB native `JplHeaderNative` layout mirrors and their size/offset tests are deleted; the accessor calls replace `PtrToStructure`.

**Go/no-go gate (before 8.3):**
- Proceed with opaque accessors only if (1) all four bindings build their existing public `Header` classes from accessor calls without new native shims or public binding-surface changes, and (2) the C test migration is mechanical field-read replacement with no test-expectation rewrites.
- Otherwise fall back to a versioned snapshot struct: keep `jpl_header` public, add `version`/`struct_size`, update the four mirrors, and check the version in each binding.

- [ ] **Step 8.1: Add the accessors while `jpl_header` is still public;** migrate the four bindings to accessor calls; keep `jpl_get_header` temporarily.
- [ ] **Step 8.2: Migrate the C tests to accessors;** delete the managed layout asserts once the accessor path is proven.
- [ ] **Step 8.3: Go/no-go review;** choose opaque accessors or the versioned snapshot fallback.
- [ ] **Step 8.4: Apply the chosen interface;** remove `jpl_get_header` and the public struct definition (or add the versioned fields); update `public_symbols.txt`, `api.dox`, docs, examples, and `bindings/CONTRACT.md`.
- [ ] **Step 8.5: Write `docs/migration-header-access.md` and `docs/adr/0001-opaque-header-accessors.md`;** record the gate outcome and the fallback rationale.
- [ ] **Step 8.6: Run the full matrix including bindings;** CHANGELOG bullet; version 0.12.0; merge and tag `v0.12.0`.**

**Acceptance:** No binding mirrors the `jpl_header` field layout (accessor path) or the versioned snapshot is checked at runtime (fallback path); every public header field remains reachable through the documented accessors; `jpl_get_header` is gone or replaced per the gate; the C and binding test suites pin the same metadata values; 50+/50+ with bindings, docs clean, migration doc complete.

**Risks/rollback:** This is the broadest migration. The accessor-first order (8.1-8.2) keeps the old struct path alive until the gate; a failed gate falls back to the versioned snapshot without reverting the core sequence. The ADR records the outcome either way.

---

## Execution Notes

- Implement in strict order: Step 1 -> Step 8. Step 2 (fixture builder) is behavior-neutral and may overlap Step 1's test work, but land it second so the core steps use it.
- One branch per step (`refactor/<slug>`; Step 1 may be `feat/lighttime-solution`), one commit per completed step plus focused intermediate commits where a step spans test migrations.
- `git merge --no-ff` into `main` only after the full matrix is green and the maintainer approves. Record the actual leg results in the merge commit message.
- If any test fails, stop and debug; the ratchet is the point. Do not weaken assertions to make a step pass.
- Do not touch `src/erfa/`, do not add or delete public API outside the two declared changes, and do not change the thread model.
- New module names go into `GLOSSARY.md` as they land. The ADR is created at Step 8, not on the plan branch.
- Version and tag: 0.12.0 at Step 8, `v0.12.0` after the full matrix.

## Definition of Done

- All eight steps merged with `--no-ff` and the full matrix recorded.
- All steps except Step 1's documented emission correction and the header step are byte-identical against the numerical reference and Horizons goldens; Step 1's delta is measured and recorded.
- Call-count pins demonstrate one gate per query, one observer read per light-time solve, and no per-evaluation record allocation on native-order reads.
- The light-time solution is exposed in C and all four bindings with the same values.
- The header interface is either opaque accessors or the versioned snapshot, with the ADR and migration doc recording why.
- `GLOSSARY.md`, `CHANGELOG.md`, `docs/validation.md`, and `bindings/CONTRACT.md` reflect the final interfaces.
- `v0.12.0` tagged after the maintainer approves the merged result.
