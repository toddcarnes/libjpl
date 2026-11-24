# Architecture Followup Implementation Plan

**Goal:** Implement the seven deepening opportunities from the 2026-11-23 architecture review, in dependency order, turning shallow modules into deep ones. Sequence: precedence gate (proven bug class), TT-TDB policy (adjacent pipeline), date boundary (entry discipline), handle readers (I/O core), rotation collapse (thin wrappers), binding contract (cross-language), suite provenance (infra last).

**Architecture:** Behavior-preserving refactors only — no public API changes, no numerical changes (one surface shrinkage allowed at S5, with migration + version bump — pre-release breakage is free, silent removal is not). New modules live behind existing interfaces: file-static functions or declarations in `src/jpl_private.h` (internal; tests already include it). Horizons goldens and the full suite are the tripwires: any residual change fails the step.

**Tech Stack:** C17 (`C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic` (Clang also enforces `-Wnewline-eof`; keep trailing newlines), ERFA v2.0.1 (vendored, never modified).

**Spec:** Architecture review `architecture-review-20261123-1745.html` (temp dir, not committed), candidates 1–7; "Full sequence" section of that report.

## Global Constraints

- C17 with `C_EXTENSIONS OFF` (enforced by CMake).
- MSVC: `/W4 /permissive-` zero warnings; GCC **and** Clang: `-Wall -Wextra -Wpedantic` zero warnings.
- Public headers: no additions in this sequence; one deletion allowed where removal concentrates the interface (S5 rotation wrappers), with in-repo caller migration per the C6 precedent and the version bump below. Zero consumers and nothing published, so pre-release breakage is free — but deletions still need migration, not silent removal.
- Two-part date arithmetic stays compensated; public/legacy on-disk scalar epochs untouched.
- Physics constants only from `src/jpl_private.h`.
- Transactional outputs: caller buffers unchanged on failure (existing tests enforce this; keep enforcing it).
- Thread-safety model unchanged: mmap shareable, FILE handles need `jpl_ephemeris_clone()`.
- All existing tests must pass after every step (the ratchet). New tests may be added; existing assertions must not be weakened.
- One branch per step (`refactor/<slug>`), merged with `git merge --no-ff` only when the verification matrix is green; delete the branch after merge.
- CHANGELOG `Unreleased` section gains one bullet per step as it lands.
- Newly coined module names go in `GLOSSARY.md` as they land.
- Version: stays 0.8.1 through the sequence unless S5 deletes entries, in which case bump to 0.9.0 at that step (0.x surface shrinkage) and move the tag at sequence end.

## Verification matrix (every step)

Default suite plus a bindings-enabled suite (all four bindings + warnings-as-errors, CI-like), Debug and Release, zero warnings. Horizons goldens byte-identical after every step. Record the actual leg results in the merge commit message; do not interpret a clean run as a semantic review.

## Glossary

New module/role names are recorded in `GLOSSARY.md`. Working names below are sketches; finalize at implementation and coin them there.

---

### Step 1: Error-precedence gate (review candidate 3) — **FIRST**

**Rationale for position:** The proven bug class in this tree is composition ordering (stale light-time, mixed-epoch velocity). Hot files, in-process, cheapest to deepen. Establishes the ordered-gate pattern Step 2 plugs into.

**Files:**
- Modify: `src/jpl.c` (`jpl_evaluate` coverage ordering, `lighttime_solve` leg ordering, `jpl_get_tt_minus_tdb` status ordering), `src/jpl_convenience.c` (`convenience_epoch` + planet/center validation ordering), `src/jpl_private.h` (gate declaration)
- Test: existing `tests/test_api_errors.c` (precedence cases become gate unit tests); no golden changes expected

**Seam sketch (finalize at implementation):**
- Consumes: validated inputs from each entry point (codes, dates, pointers)
- Produces (internal): one ordered gate — validate → normalize → coverage → dataset → frame — returning the first failure; each entry point funnels through it instead of composing its own order

- [ ] **Step 1.1: Baseline** — full matrix green on `main` (record the leg results)
- [ ] **Step 1.2: Extract the gate as a wrapper over current orderings** (no behaviour change; one call site converted at a time, suite green throughout)
- [ ] **Step 1.3: Push each remaining call-site ordering inside** (evaluate, solver, tt_tdb, convenience_epoch); delete the per-site compositions
- [ ] **Step 1.4: Convert scattered precedence cases to gate unit tests** (one matrix: every bad-input class × expected first-failure status)
- [ ] **Step 1.5: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: no entry point composes its own check order; the precedence table exists exactly once; 46+/49+ everywhere (default + bindings-enabled).
Risks/rollback: wrapper-first (1.2) keeps the old orderings one revert away; any golden drift reverts to it, not to `main`.

---

### Step 2: TT-to-TDB policy (review candidate 6) — **SECOND**

**Rationale for position:** Same pipeline as Step 1, same goldens as tripwire, plugs into the gate vocabulary (file-vs-model availability is a dataset-ordering decision). Pair them while the pipeline is hot.

**Files:**
- Modify: `src/jpl.c` (file-interpolated path), `src/jpl_frames.c` (model path + envelope), `src/jpl_convenience.c` (`convenience_epoch` consumes the policy instead of hard-calling the model), `src/jpl_private.h` (policy declaration)
- Test: existing `tests/test_horizons.c` (goldens must not move — model path stays the default where file data is absent); new file-TT-TDB × apparent-place golden (currently uncovered composition)

**Seam sketch (finalize at implementation):**
- Consumes: Step-1 gate (availability/coverage ordering), `has_tt_tdb` + span check, epoch
- Produces (internal): one policy entry — file data when present and in coverage, else model with envelope check; epoch-out (not seconds) so both adapters satisfy one shape

- [ ] **Step 2.1: Add the policy as a wrapper over the current model-only call** (no behaviour change; convenience calls it, suite green)
- [ ] **Step 2.2: Move the file-vs-model choice inside** (span validation + rate stay with the file adapter; iteration + envelope stay with the model adapter)
- [ ] **Step 2.3: Add the file-TT-TDB × apparent-place golden** (new test; proves the previously uncovered composition)
- [ ] **Step 2.4: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: exactly one TT-to-TDB choice in the tree; convenience performs no source selection; goldens byte-identical plus one new golden.
Risks/rollback: 2.1 is the bisect point (mechanical wrapper); any apparent-place drift reverts to it.

---

### Step 3: Date trust boundary (review candidate 4) — **THIRD**

**Rationale for position:** Gives the gate's normalize step its real meaning before the broad mechanical passes. Touches every entry point, so it goes after the pipeline pair while entry-point behavior is freshly pinned by their tests.

**Files:**
- Modify: `src/jpl_private.h` (funnel declaration + normalized-pair convention), `src/jpl_date.c` (primitives assume normalized; funnel normalizes once), call sites in `src/jpl.c`, `src/jpl_frames.c`, `src/jpl_convenience.c` (replace ad-hoc normalize/compare patterns)
- Test: existing `tests/test_dates.c` plus wrong-site-normalization tests; Debug assertions fire on violations (a firing assertion is a wrong-site bug — fix the site, not the assert)

**Seam sketch (finalize at implementation):**
- Consumes: public `jpl_date` (dumb struct, unchanged — C aggregates cannot enforce construction)
- Produces (internal): the single trust-boundary normalizer every public entry point funnels through exactly once; internal consumers take an already-normalized pair (debug-asserted); `compare` chronological by construction

- [ ] **Step 3.1: Audit every public entry point** (list the funnel sites in the commit message)
- [ ] **Step 3.2: Add debug assertions to internal consumers; run Debug matrix** (zero assertions may fire)
- [ ] **Step 3.3: Remove redundant normalizations the audit exposes** (keep FFI-edge conversions; unify the two assert spellings into `JPL_ASSERT_NORMALIZED`)
- [ ] **Step 3.4: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: `grep` shows normalization only at entry funnels + the helper; Debug matrix fires zero assertions; 46+/49+ everywhere.
Risks/rollback: assertion fires reveal latent wrong-site bugs — that is the point; fix forward, do not relax the assert.

---

### Step 4: Handle readers (review candidate 1) — **FOURTH**

**Rationale for position:** Largest surgery in the sequence (I/O core + thread-safety contract). Goes mid-sequence with tripwires hot and entry discipline settled, mirroring how the evaluator surgery was sequenced in the prior deepening.

**Files:**
- Modify: `src/jpl.c` (struct `jpl_ephemeris`, `read_doubles`, `jpl_open`, `jpl_open_mmap`, `jpl_ephemeris_clone`, stream-identity helpers), `src/jpl_private.h` (byte-source seam declaration)
- Test: existing `tests/test_threads.c`, `tests/test_mmap.c`, `tests/test_fixture*.c`; new composition tests (open → read → clone → close orderings, cache-not-poisoned-on-failure via second-query success)

**Seam sketch (finalize at implementation):**
- Consumes: path or mapping + byte order
- Produces (internal): one open path parameterised by byte-source adapter (FILE, mmap — the real seam, two adapters); one reader behind it owning cache-refresh and thread rules; Windows/POSIX mmap halves move to the adapter, out of the constructor

- [ ] **Step 4.1: Move the two byte readers behind the seam without changing bodies** (pure relocation commit; suite must stay green)
- [ ] **Step 4.2: Fold the `is_mmap` branches out of `read_doubles` into the adapters** (no caller names a source kind afterwards)
- [ ] **Step 4.3: Unify the three constructors onto one open path** (cache-refresh-on-success and identity re-check live in exactly one place)
- [ ] **Step 4.4: New composition tests** (thread-shareability matrix, clone-identity, cache discipline)
- [ ] **Step 4.5: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: deleting the reader branch concentrates complexity (passes the deletion test the current code fails); no caller branches on source kind; no platform `#if` in constructors.
Risks/rollback: 4.1 pure-relocation commit is the bisect point; any golden or thread-flake drift reverts to it.

---

### Step 5: Rotation collapse + velocity contract (review candidate 5) — **FIFTH**

**Rationale for position:** Small and speculative; goes after the core settles so its contract test pins final behavior. Pre-release breakage is free, so the redundant split goes here rather than living on as forwards.

**Files:**
- Modify: `src/jpl_frames.c` (one rotation computer; delete the redundant position/velocity split, keep one vector rotation + state rotation — names finalized at implementation), `include/jpl_frames.h` (remove the deleted declarations + doc blocks), `src/jpl_private.h` if the computer needs sharing (otherwise file-static)
- Migrate in-repo callers of the deleted entries (tests, all four bindings, c-examples README) to the surviving entries, per the C6 precedent; keep every test expectation byte-identical
- Test: existing `tests/test_frames.c`; new velocity-contract test pinning the documented dR/dt omission (currently note-only); `tests/public_symbols.txt` and `docs/api.dox` lose the deleted lines

**Seam sketch (finalize at implementation):**
- Consumes: cached context matrix (unchanged EpochFrame seam)
- Produces (internal): one rotation computer; public surface keeps one vector rotation + `rotate_state` (the position/velocity split — a single-adapter seam with identical bodies — is deleted, C6-style); the omission becomes a test assertion, not just a `@note`

- [ ] **Step 5.1: Collapse the three bodies onto one computer** (mechanical; suite green)
- [ ] **Step 5.2: Delete the redundant split; migrate all in-repo callers** (mechanical, assertion-preserving; prune `public_symbols.txt` + `api.dox`; `grep` clean outside historical docs)
- [ ] **Step 5.3: Add the velocity-contract test** (pins the omission against a fixed expectation)
- [ ] **Step 5.4: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: one `eraRxp` + finite-check body in the tree; no references to the deleted entries in code, tests, bindings, or live docs; omission pinned by test; version bumped to 0.9.0 at this step (0.x surface shrinkage).
Risks/rollback: single-commit surgery; revert whole step on any drift. **Tripwire (standing):** if a sixth `EclipticToIcrs`-shaped consumer appears (four bindings + test shim exist today), treat the C6 deletion as a hypothesis disproven and propose re-adding a thin public wrapper as its own decision — do not expand this step to cover it.

---

### Step 6: Binding contract (review candidate 2) — **SIXTH**

**Rationale for position:** Cross-language spec + tests, no C changes; captures the final shapes from Steps 1–5 (especially the context-wrapping form). Independent of C code, so it overlaps anything — scheduled late so the contract is written once, against settled seams.

**Files:**
- Modify (docs/tests only, no binding behavior change): new `bindings/CONTRACT.md` (conversion contract: header snapshot incl. >400-name fallback, date/state bridging incl. aliasing, context create/rotate/destroy wrapping, `0.0`-omits-solar-term rule), per-language drift tests asserting each binding satisfies it
- Unchanged (verify by running, not editing): all four bindings' surfaces and behavior; existing binding tests become oracles
- Test: new cross-binding drift tests (same fixture inputs → same outputs in all four languages, incl. the >400-name path with a synthetic header)

**Seam sketch (finalize at implementation):**
- Consumes: the C FFI (stable public API)
- Produces: one written contract + executable drift tests; each language adapter keeps its own implementation but satisfies the same assertions

- [ ] **Step 6.1: Write the contract from the current C++ binding** (descriptive, not prescriptive — record what the four adapters actually do today; flag divergences instead of silently picking a winner)
- [ ] **Step 6.2: Add drift tests per language** (same fixtures, same expectations; the >400-name path gets a synthetic header in each language)
- [ ] **Step 6.3: Reconcile divergences the tests expose** (binding-surface changes allowed where the contract demands them — pre-release breakage is free; record load-bearing ones, just fix ephemeral ones)
- [ ] **Step 6.4: Run the verification matrix (bindings-enabled legs carry this); CHANGELOG `Unreleased` bullet; merge**

Acceptance: a divergence in any adapter fails a test instead of requiring a four-way diff; no binding surface changes.
Risks/rollback: contract-first (6.1) means 6.2 can only reveal, never break; reconcile conservatively (match the majority + C semantics).

---

### Step 7: Suite provenance (review candidate 7) — **LAST**

**Rationale for position:** Infra-only, zero behavior risk, most valuable once behavior is stable (the provenance format won't churn under it). Explicitly not a driver-deepening step.

**Files:**
- Modify: `CMakeLists.txt` (emit a per-run data manifest: which ephemeris inputs each data-dependent test actually consumed — committed slice vs `JPLEPH`/`JPL_TESTPO` redirect vs absent), `docs/validation.md` (one paragraph: how to read the manifest)
- Unchanged: all 16 `tests/cmake/*` drivers (correctly thin; do not touch)

**Seam sketch (finalize at implementation):**
- Consumes: the existing fetch-tier variables + test registrations
- Produces: one manifest file per configure (checked-in slice hashes + env-redirect resolution + absent-data list), printed as a ctest preamble line so "what did this run validate?" has one answer

- [ ] **Step 7.1: Emit the manifest at configure time** (read-only addition; no test logic changes)
- [ ] **Step 7.2: Surface it in the `jpl_docs_only`-style preamble** (visible in CI logs, not buried in a file)
- [ ] **Step 7.3: Document it in `docs/validation.md`** (short paragraph + example)
- [ ] **Step 7.4: Run the verification matrix; CHANGELOG `Unreleased` bullet; merge**

Acceptance: a default configure answers "which data did the suite validate?" in one place; no driver modified; fetch-tier behavior unchanged.
Risks/rollback: additive-only; revert is deletion.

---

## Execution Notes

- Implement in strict order: S1 → S2 → S3 → S4 → S5 → S6 → S7. S2 prefers S1 landed (shares gate vocabulary) but can proceed against a stub. S6 is independent of C code and may overlap any step after S5's shapes settle. S7 may be done any time but is scheduled last to avoid format churn.
- One branch per step (`refactor/<slug>`), one commit per completed step; `git merge --no-ff` into `main` only when the full verification matrix is green at the sequence tip (per-step commits preserve bisectability; repo convention: branch + explicit approval before merge).
- If any test fails, STOP and debug before continuing — the ratchet is the point.
- Do not touch `src/erfa/`, do not add or delete public API in this sequence, do not weaken assertions to make a step pass.
- Newly coined module names go in `GLOSSARY.md` as they land.
- Standing tripwire (from candidate 5): a sixth `EclipticToIcrs`-shaped consumer reopens the C6 deletion as its own decision — record it as an ADR at that time rather than expanding any step here.
