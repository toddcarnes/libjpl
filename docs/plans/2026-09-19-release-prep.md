# Release Prep (No Version Bump) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare the repo for a future public release without changing the version number.

**Architecture:** Three independent docs/build-hygiene tasks with no library behavior change: a CHANGELOG built from merged-branch history, a README accuracy pass against the current tree, and a Doxygen plus ASCII plus fresh-clone verification pass. No `project(VERSION)` change, no tags.

**Tech Stack:** C17 (`c_std_17`, `C_EXTENSIONS OFF`), CMake 3.20+, Doxygen 1.9+ (`WARN_AS_ERROR=YES`), MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`.

**Spec:** No design doc (docs/hygiene batch; authority is the current tree + merged-branch history below).

## Global Constraints

- C17 with `C_EXTENSIONS OFF`; MSVC `/W4 /permissive-` zero warnings; GCC/Clang `-Wall -Wextra -Wpedantic` zero warnings (lib AND tests).
- `src/erfa/` is read-only vendored code; never modify.
- Public headers accumulate Doxygen (`@brief/@param/@return/@note`); `WARN_AS_ERROR=YES` must stay clean; append, don't rewrite.
- Agent-run shell commands are PowerShell 5.1: chain with `;` (no `&&`/`||`), quote paths with spaces, use `workdir` instead of `cd`.
- Commit messages follow Conventional Commits with subject + body: `type(scope): imperative subject` (max 72 chars, lowercase after colon, no trailing period), blank line, body explaining what changed and why wrapped at 72 chars. Never commit with a bare one-line subject.
- Historical docs (`docs/specs/*`, `docs/plans/*`) are records: do NOT rewrite them.
- NO version bump: `project(libjpl VERSION 0.1.0 ...)` stays; no git tags.

---

## File map

- `CHANGELOG.md` (new) — Keep-a-Changelog `Unreleased` section built from the merged branches below.
- `README.md` — accuracy fixes only (suite counts, test-data section vs CMake, ASCII check).
- `Doxyfile.in`, `include/*.h` — only if the Doxygen build reports warnings (fix warnings, never weaken `WARN_AS_ERROR`).
- Untouched: `src/*`, `tests/*`, `CMakeLists.txt` (unless Doxygen config needs a path fix).

---

### Task 1: CHANGELOG from merged history

**Files:**
- Create: `CHANGELOG.md`
- Test: markdown renders (headings nest correctly, no tabs)

**Interfaces:**
- Consumes: merged-branch history (`git log --oneline main`, branch names deleted but commits remain).
- Produces: `CHANGELOG.md` with `Unreleased` section; future release commits add dated sections above it.

**Steps:**

- [ ] **Step 1: Collect the history.** Run:
```powershell
git log --oneline --reverse d266986..HEAD
git log --oneline --reverse 97ad52d..HEAD
git log --oneline --merges -15
```
The branches (all merged, all deleted) in order: `bugfix/review-findings-fix` (8 correctness tasks: light-time center epoch, SRS constant, overflow guards, mmap thread-safety, convenience observer epoch, explicit test checks, docs, frames golden, POSIX fstat, testpo harness), `bugfix/jpl-body-numbering` (Earth=3/EMB=13 renumber, BREAKING), `bugfix/testpo-t-variants` (t-variant registration), `feature/tt-tdb-support` (TT-TDB getter + presence), `test/public-test-data` (fixture + fetch tier), `chore/followup-hardening-batch` (guard/symbol/SRS/MANIFEST/runner hardening), `chore/cosmetic-minors` (committed).
- [ ] **Step 2: Write `CHANGELOG.md`.** Keep-a-Changelog format, exact skeleton:
```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- TT-TDB dataset support ...
- Committed DE441 2000-2010 fixture ...
- Opt-in JPL download tier ...

### Changed
- Body codes Earth=3 / EMB=13 (BREAKING: raw-integer callers swap) ...
- ...

### Fixed
- Light-time center epoch ...
- SRS constant ...
- ...
```
Fill each entry from the actual commit subjects (not from memory): every bullet names the user-visible behavior, never internal task numbers. The renumber entry carries a `**BREAKING CHANGE**` marker inline. Keep it to one screen (skim, not exhaustive): group related commits, omit plan/spec/doc commits except the user-facing test-data strategy.
- [ ] **Step 3: Verify + commit.**
Run: `Get-Content CHANGELOG.md | Select-Object -First 20` (headings nest, no tabs).
```bash
git add CHANGELOG.md
git commit -m "docs(changelog): add Unreleased section from merged history" -m "Start a Keep-a-Changelog file grouping the merged branches (correctness fixes, body renumber with BREAKING marker, TT-TDB support, test-data tiers, hardening) under Unreleased. No version bump; future releases add dated sections above it."
```

### Task 2: README accuracy pass

**Files:**
- Modify: `README.md` (surgical edits only)
- Test: `ctest -N` counts + CMake option defaults cross-checked by hand

**Interfaces:**
- Consumes: current `CMakeLists.txt` registration logic; current `ctest -N` output.
- Produces: README claims that match the tree.

**Steps:**

- [ ] **Step 1: Check suite counts.** Run `ctest --test-dir build -N` twice: once default configure (expect 8: 6 unit + `jpl_tt_tdb` + `jpl_fixture`), once with `-DLIBJPL_FETCH_EPHEMERIDES=ON` + local `JPLEPH`/`JPL_TESTPO` (expect 8 + 2 integration + 2 fixed testpo + 27 sweep = 39). If README names counts, correct them; if it names suite names (`jpl_real_ephemeris`, `jpl_all_ephemerides_local`, `jpl_testpo_440[t]`), verify each exists in the listing.
- [ ] **Step 2: Check the Test data section against CMake.** Verify: option name `LIBJPL_FETCH_EPHEMERIDES`, default OFF, ~210 MB figure, the four URLs/filenames, manifest path `tests/data/MANIFEST.txt`, fixture paths. Fix any drift (wrong name, stale size, moved path). Verify the `## Use` snippets still compile-conceptually (symbols `JPL_EARTH`/`JPL_SUN` resolve post-renumber; `jd_tt`/`jd_tdb` distinct).
- [ ] **Step 3: ASCII + links.** Run `Select-String -Path README.md -Pattern "[^\x00-\x7F]"` — expect no hits (prior mojibake fix). Confirm the two JPL URLs return 200 (short HEAD request is enough; full download NOT needed).
- [ ] **Step 4: Commit.**
```bash
git add README.md
git commit -m "docs(readme): correct suite counts and test-data section" -m "Align the README with the current tree: verified ctest suite names and counts in both default and fetch configurations, manifest and fixture paths, and download URLs. Pure documentation; no code change."
```
If Step 1-3 find zero drift, commit an empty-shape change is forbidden — instead report `NO-DRIFT` with the evidence and skip the commit (record in the report; reviewer verifies the claim).

### Task 3: Doxygen, ASCII tree, fresh-clone proof

**Files:**
- Modify: `Doxyfile.in`, `include/*.h` ONLY if Doxygen reports warnings (fix warnings, never set `WARN_AS_ERROR=NO`)
- Test: docs build + ASCII scan + clean-configure proof

**Interfaces:**
- Consumes: Doxygen 1.9+ if installed (check `doxygen --version` first; if absent, record `DOXYGEN-ABSENT` and verify the rest).
- Produces: WARN_AS_ERROR-clean docs build evidence; ASCII-clean tree proof; default-configure green proof.

**Steps:**

- [ ] **Step 1: Doxygen build.** Run:
```powershell
cmake -S . -B build-docs -DLIBJPL_BUILD_DOCS=ON; cmake --build build-docs
```
Expected: success with zero warnings (`WARN_AS_ERROR=YES`). If warnings name a header decl, fix the decl's Doxygen (append tags, never remove content) and rebuild. If Doxygen is not installed, record versions/proof-skipped and move on (do NOT install software). Delete `build-docs/` after (not gitignored — MUST delete).
- [ ] **Step 2: ASCII scan of user-facing files.** Run:
```powershell
Select-String -Path README.md,CHANGELOG.md,CMakeLists.txt,Doxyfile.in -Pattern "[^\x00-\x7F]"
```
Expected: no hits. (Prior mojibake rounds fixed README + CMake; CHANGELOG is new.) Fix any hit with ASCII equivalents.
- [ ] **Step 3: Fresh-clone proof (simulated).** Fresh scratch configure with NO `JPLEPH`/`JPL_TESTPO`/`LIBJPL_FETCH_EPHEMERIDES` (defaults): `ctest -N` shows exactly 8 suites; `ctest -C Debug -R "jpl_fixture|jpl_tt_tdb"` green. This simulates the public-clone experience on the maintainer box. Delete the scratch dir after (not gitignored — MUST delete).
- [ ] **Step 4: Commit (only if Step 1-2 changed files).**
```bash
git add Doxyfile.in include/jpl.h include/jpl_frames.h include/jpl_convenience.h
git commit -m "docs(doxygen): keep WARN_AS_ERROR build clean" -m "Fix <decl> Doxygen warnings reported by the docs build (<warning text>); no API or semantic change."
```
If nothing changed, no commit — record `NO-DRIFT` with evidence. proximate: never commit an empty change.
