<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# Live documentation audit implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Audit and correct live libjpl documentation, every public API Doxygen declaration, and factual comments in handwritten code without changing behavior.

**Architecture:** Cross-check each claim against current implementations, tests, installed interfaces, and CMake registration. Make surgical comment/prose edits backed by evidence; keep historical records and vendored sources intact. Report implementation defects separately rather than hiding them behind documentation edits.

**Tech Stack:** C17, CMake 3.20+, CTest, Doxygen (`WARN_AS_ERROR=YES`); Graphviz `dot` for the validated Linux docs setup.

**Spec:** `docs/specs/2026-09-26-live-documentation-audit-design.md`.

## Global constraints

- Existing historical `docs/{specs,plans}/` and `docs/reviews/` files are records; do not rewrite them. Do not edit `src/erfa/`, reference fixtures, hashed data or generator source, API declarations, or implementation behavior.
- The currently uncommitted `README.md` and `AGENTS.md` changes clarify bundled ERFA v2.0.1; retain their intent. Inspect `git status` before moving files, branching, or staging; no stash/reset/overwrites of user work.
- Before execution, use an isolated branch/worktree as appropriate. A branch merged into `main` requires explicit maintainer approval, `git merge --no-ff <branch>`, and verification on merged `main` before cleanup. Commit only on explicit request.
- Public dates remain two-part `double[2]` in memory and scalar on disk. Do not confuse correction amplitude, numerical tolerance, and model accuracy, or claim that ephemeris coverage guarantees model validity.
- Fix only documented, verified mismatches. When the code contradicts an intended API contract, record the code finding and seek a separate behavior-change decision.

## File ownership map

| Area | Files | Responsibility |
| --- | --- | --- |
| User/build guidance | `README.md`, `AGENTS.md`, `docs/migration-two-part-dates.md`, `CHANGELOG.md` | Current feature and migration instructions; historical release entries remain history |
| Scientific and test evidence | `docs/validation.md`, `tests/data/PROVENANCE.md`, `tests/data/numerical_reference.md` | Accurate limits, tier names, provenance, verified-versus-historical results |
| Distribution | `LICENSES.md` | ERFA v2.0.1 attribution, installed notice paths, current license-check behavior |
| Public API | `include/jpl.h`, `include/jpl_frames.h`, `include/jpl_convenience.h` | 24 function contracts plus documented types/constants |
| Implementation comments | `src/jpl.c`, `src/jpl_date.c`, `src/jpl_frames.c`, `src/jpl_convenience.c`, `src/jpl_private.h` | Explain actual parsing, numeric, lifetime and frame semantics |
| Supporting comments | `CMakeLists.txt`, `cmake/*.cmake`, `tests/*.c`, `tests/*.h`, `tests/*.cmake`, `tests/cmake/*.cmake`, `tools/fixture_tool.c` | Explain live test, install and fetch behavior; leave test logic unchanged. Read the hash-pinned `tools/generate_numerical_reference.c` without editing it |
| Verification inventory | `tests/public_symbols.txt`, `Doxyfile.in`, `tests/cmake/check_docs.cmake` | Cross-check declared symbols and Doxygen generation; comment-only updates are allowed, but do not change symbol entries or checker logic |

---

### Task 1: Establish scope and documentation baseline

**Files:** Read all live files in the map, approved spec, `include/*.h`, relevant `src/jpl*.c`, `CMakeLists.txt`, and `tests/public_symbols.txt`. Do not modify code in this task.

**Produces:** A working evidence table with columns `file:line | exact claim | implementation/build/test evidence | correction or no change` (keep this in the session task ledger, not a new tracked historical document).

- [ ] **Step 1: Preserve the workspace.** Run `git status --short --branch`, `git diff -- README.md AGENTS.md`, and `git log --oneline -10`. Keep the two existing user edits. If execution uses a new worktree, transfer those exact edits and the approved spec/plan into it without removing or overwriting the originals. Verify the worktree and branch names before any edits.
- [ ] **Step 2: Inventory the live files.** Read the eight live Markdown files listed in the file map, the three installed headers, the five handwritten `src/jpl*` files, and test/build comments. Enumerate all 24 public function names from `tests/public_symbols.txt` (functions occupy its first 24 non-comment entries), then confirm the three headers contain the same declarations. Use targeted searches for present-tense words (`pending`, `current`, `must`, `requires`, `always`) and opaque task/fix IDs in comments; a dated observation is not a current defect unless live guidance treats it as current.
- [ ] **Step 3: Establish the docs baseline.** Configure `cmake -S . -B build/docs-audit -DLIBJPL_BUILD_DOCS=ON -DLIBJPL_FETCH_EPHEMERIDES=OFF`, then build `cmake --build build/docs-audit --config Debug --target docs`. Run `ctest --test-dir build/docs-audit -C Debug --output-on-failure -R '^(jpl_docs_symbols|jpl_docs_paths|jpl_docs_only)$'`. Record actual results; do not interpret a clean Doxygen run as a semantic review. If Doxygen or the Linux docs toolchain is missing, stop under `AGENTS.md` instead of silently omitting a check.
- [ ] **Step 4: Open the evidence table.** One confirmed example is `LICENSES.md:35-36` saying “T19 must pass” although `CMakeLists.txt` and `tests/cmake/test_install.cmake` already pass `DATADIR`. Record it with those source references before rewriting it. Every further correction needs the same support.

### Task 2: Correct active guides, notices and evidence pages

**Files:** Modify only evidence-backed passages in `README.md`, `AGENTS.md`, `LICENSES.md`, `CHANGELOG.md` (Unreleased/current guidance only), `docs/validation.md`, `docs/migration-two-part-dates.md`, `tests/data/PROVENANCE.md`, `tests/data/numerical_reference.md`.

**Consumes:** Task 1 evidence table. **Produces:** Live guidance whose version, date, model, test and distribution statements match current code and registrations.

- [ ] **Step 1: Verify each link, prerequisite and command.** Compare README examples to `tests/examples.c` and public declarations; compare fetch, docs/test options, installation and conditional test registrations to `CMakeLists.txt`; verify licensing against actual `install(FILES ...)` paths and `tests/cmake/test_install.cmake`. Check the reference/provenance hashes against `tests/data/MANIFEST.txt` using the existing integrity tests, not a new source hash or a rewritten fixture.
- [ ] **Step 2: Replace the concrete stale license instruction.** After confirming CMake still forwards `DATADIR`, replace `LICENSES.md`'s “T19 must pass that datadir” sentence with a present-tense statement such as: “The `jpl_install` test passes the configured data directory to `check_licenses.cmake`, so custom install layouts are checked without assuming `share`.” Leave ERFA's license and version attribution intact.
- [ ] **Step 3: Disambiguate validation history without erasing it.** `docs/validation.md` has a current 2026-09-25 acceptance section but its dated T22/T23 sections still contain “Latest observed result: pending” (e.g. lines 225-227, 248-253, 289-300). Mark those sections as **2026-09-19 historical observations** and direct readers to the newer acceptance at the top; when editing a specific “latest” sentence, change “Latest observed” to “Observed at T23” while preserving the old measured counts and blocked gates. Keep genuinely unrun macOS/clang-cl claims unverified. Refresh the title/prerequisite wording only to the extent the current section supports it.
- [ ] **Step 4: Review every remaining live page.** Check the README and migration signature tables against installed headers, the scientific claims against `src/jpl_frames.c` and `docs/validation.md`, current changelog entries against commits, and test-data notes against the actual checked-in fixtures/manifest. In `tests/data/numerical_reference.md`, compare the documented generation command with `tools/generate_numerical_reference.c` and CMake; clarify its prose only if the observed command or file format differs. Do not change previously released changelog claims or authoritative data to make a statement pass.
- [ ] **Step 5: Verify the guide changes.** Run `git diff --check`, `ctest --test-dir build/docs-audit -C Debug --output-on-failure -R '^(jpl_examples|jpl_licenses|jpl_numerical_reference_integrity|jpl_fixture_integrity)$'` after building the ordinary tests in that configuration (`cmake --build build/docs-audit --config Debug --parallel`). Expected: all four selected tests pass. Record any Doxygen prerequisites or selected test names that differ from the baseline rather than assuming the list.

### Task 3: Audit the core public API Doxygen

**Files:** Modify documentation only in `include/jpl.h`. Read `src/jpl.c`, `src/jpl_date.c`, `src/jpl_private.h`, `tests/test_api_errors.c`, `tests/test_parser.c`, `tests/test_lighttime.c` as evidence.

**Consumes:** Current `jpl_status` and source control flow. **Produces:** Verified comments for `jpl_status_string`, `jpl_open`, `jpl_close`, `jpl_get_header`, `jpl_read_constants`, `jpl_read_record`, `jpl_record_count`, `jpl_get_tt_minus_tdb`, `jpl_has_tt_tdb`, `jpl_get_state`, `jpl_get_state_lighttime`, `jpl_apply_aberration`, `jpl_apply_aberration_full`, `jpl_open_mmap`, `jpl_ephemeris_clone`; review the associated enum/struct/macro descriptions too.

- [ ] **Step 1: Build a 15-row contract matrix.** For each function above, record every argument, result/units, valid target/center combinations, the returned status per early exit (including allocation and I/O), ownership/lifetime, date-pair and frame policy, output-on-failure behavior and thread/alias constraints. Derive expected values from implementation and independent test assertions, not from an adjacent header comment. List any missing `@param`, incomplete `@return`, or misleading `@note` in the Task 1 evidence table.
- [ ] **Step 2: Check known candidates before editing.** `src/jpl.c:930-961` can return `JPL_ERROR_MEMORY` from `jpl_open`, but `include/jpl.h:154-155` currently lists ARGUMENT/IO/FORMAT only. `src/jpl.c:1337-1352` restricts ordinary centers to 0 or 1-13 and angular targets to center 0; `include/jpl.h:262-268` currently sounds as if centers 14/15 are accepted. Trace the actual validation paths, then add the missing status and explicit center-domain distinction if confirmed. Check the `jpl_open_mmap`/clone notes and raw-read exceptions separately rather than copying open's wording.
- [ ] **Step 3: Correct only the comment blocks proved wrong or incomplete.** Maintain each function's existing Doxygen structure (`@brief`, each `@param`, `@return`, applicable `@note`) and retain the scientific caveats that are accurate. For any source/test difference not explained by documented preconditions, record a code finding instead of adjusting API text to conceal it. Do not alter a declaration or implementation.
- [ ] **Step 4: Verify the core headers.** Rebuild `docs`; run `jpl_docs_symbols`, `jpl_docs_paths`, `jpl_api_errors`, `jpl_parser`, and `jpl_lighttime` (with `-C Debug` under MSVC). Read the generated `jpl.h` reference page for the changed entries; Doxygen presence alone is not enough to check `@return` completeness.

### Task 4: Audit frame and convenience Doxygen

**Files:** Modify documentation only in `include/jpl_frames.h` and `include/jpl_convenience.h`. Read `src/jpl_frames.c`, `src/jpl_convenience.c`, `src/jpl_private.h`, `docs/validation.md`, `tests/test_frames.c`, `tests/test_convenience.c`, `tests/test_numeric.c`.

**Produces:** Verified comments for five frame/time functions and four convenience/unit functions.

- [ ] **Step 1: Build a nine-row contract matrix.** For the five frame/time functions and four convenience/unit functions, check every `@param`, units, date scale/split, finite/NULL handling, aliasing, returned error codes, transactional outputs, frame direction, observer/target epoch, legacy-frame gating, Sun potential term and fixed-epoch velocity rotation. Compare `jpl_moon_position` to its delegation to `jpl_planet_position`, including all possible downstream errors.
- [ ] **Step 2: Investigate two known documentation candidates.** `include/jpl_frames.h:92-95` calls a millisecond-scale value the “ERFA model envelope,” whereas `docs/validation.md` distinguishes the ~1.7 ms **correction amplitude** from ERFA's cited nanosecond **model accuracy**; verify the ERFA source reference, then correct that note. `include/jpl_convenience.h:71-74` lists fewer errors than `jpl_planet_position` can return; confirm delegation and update Moon's `@return` to include downstream FORMAT/NUMERIC/IO/MEMORY/CONVERGENCE where applicable. Avoid describing the Sun potential as gravitational bending.
- [ ] **Step 3: Correct the remaining evidence-backed comments.** Preserve the project's documented fixed-epoch `R*v` velocity convention (no `dR/dt`), modern AU, center-0/12 alias, and transactional output semantics. Verify two-part values are passed separately to `eraEcm06` and `eraDtdb` before asserting precision. Do not broaden the ICRF allowlist in a documentation task.
- [ ] **Step 4: Verify the frame/convenience pages.** Rebuild `docs` and run `jpl_docs_symbols`, `jpl_docs_paths`, `jpl_frames`, `jpl_convenience`, `jpl_numeric`, and `jpl_examples`; inspect the generated pages for the corrected functions, not just the XML symbol count.

### Task 5: Audit handwritten implementation and support comments

**Files:** Comment-only changes, when supported, in `src/jpl.c`, `src/jpl_date.c`, `src/jpl_frames.c`, `src/jpl_convenience.c`, `src/jpl_private.h`, `CMakeLists.txt`, `cmake/*.cmake`, `tests/*.c`, `tests/*.h`, `tests/*.cmake`, `tests/cmake/*.cmake`, `tools/fixture_tool.c`, and comment lines in `tests/public_symbols.txt`. Read but do not edit the hash-pinned `tools/generate_numerical_reference.c`.

- [ ] **Step 1: Check handwritten source comments.** Follow the `src/jpl.c` parser/layout (`validate_layout`, `parse_header`, `read_doubles`), record/subinterval selection, clone/mmap lifetime and light-time blocks; verify comments against the code directly below or called helpers. Check `jpl_date.c` compensated arithmetic, `jpl_frames.c` ERFA conventions and `jpl_convenience.c` observer/frame assumptions. Search for old R/F/T task references and future-tense comments. For a comment such as “R06b trailing-record case,” state the actual ambiguity or boundary rule instead; preserve the reason, not the task label.
- [ ] **Step 2: Check build/test/tool comments.** Compare statements about test-only targets, options, fetch and notice paths with `CMakeLists.txt`. Compare validator, fixture, provenance and numerical-reference comments with their actual CTest registrations and assertions. In particular verify Python has been replaced by `jpl_fixture_tool`, that `jpl_testing` is not installed, and that docs-only checks register with ordinary tests OFF. Check actual tested behavior, not merely matching strings in neighboring comments.
- [ ] **Step 3: Edit verified discrepancies in place.** Delete stale process narration and opaque remediation IDs, or replace them with the invariant/current behavior they were trying to explain. Do not erase informative explanations of byte order, record width, two-part precision, overflow guards or solver boundaries. Keep references to standards or directly relevant tests where they explain a non-obvious rule. Keep test sources, public declarations, private algorithms and CMake logic byte-for-byte unchanged apart from comments.
- [ ] **Step 4: Verify affected comment contexts.** Use focused tests for any numerical/API claim corrected in comments (e.g. `jpl_parser`, `jpl_numeric`, `jpl_threads`, `jpl_mmap`), then run `jpl_licenses` for inventoried sources. A test failure after a comment-only edit is a blocker, not grounds to modify production behavior inside this scope.

### Task 6: Final reconciliation and report

**Files:** Review the changed files above and the unchanged historical/vendored paths; no new API or implementation files.

- [ ] **Step 1: Reconcile coverage.** Verify all 24 public function declarations have been compared with their implementations, every live Markdown page was read, and handwritten comment areas in the file map were searched and assessed. Confirm no unexplained R/F/T remediation label remains in live code comments; historical records are exempt. Mark each candidate in the evidence table `corrected`, `already accurate`, `historical`, or `code finding (not fixed)`. Link remaining defects to file:line evidence in the final report.
- [ ] **Step 2: Run final documentation checks.** On a fresh docs-enabled build or a rebuilt `build/docs-audit`: `cmake --build build/docs-audit --config Debug --target docs` and `ctest --test-dir build/docs-audit -C Debug --output-on-failure -R '^(jpl_docs_symbols|jpl_docs_paths|jpl_docs_only|jpl_examples|jpl_licenses|jpl_numerical_reference_integrity|jpl_fixture_integrity)$'`. Report each actual registration, pass count and any prerequisite limits. Run additional focused behavioral tests only for claims changed after Task 3/4/5 verifications.
- [ ] **Step 3: Inspect the final diff and workspace.** Run `git diff --check`, `git status --short --branch`, and review `git diff` for `README.md`/`AGENTS.md` preservation, unchanged declarations/algorithms/reference data, and no `src/erfa/` or dated historical-document changes. Leave edits uncommitted unless the maintainer explicitly requests a commit.
- [ ] **Step 4: Report results.** Summarize each material correction with path/line, verification evidence and any unresolved implementation mismatch. Distinguish newly observed test results from past acceptance in historical records.

## Execution handoff

Start with the baseline and evidence table, then work through live guidance, core API, frame/convenience API, and support comments. Existing Doxygen symbol checks cannot prove semantic accuracy; the cross-check in Tasks 3 and 4 is mandatory. Preserve current user edits and use the repository's no-fast-forward merge rule if a merge is later requested.
