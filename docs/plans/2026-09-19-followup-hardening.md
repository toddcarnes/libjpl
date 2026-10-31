# Follow-Up Hardening Batch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Clear all 13 deferred follow-up items from the four branch ledgers in one batch.

**Architecture:** Three tasks grouped by shape: src one-liners and comments (mechanical, one review pass), test strictness plus the SRS-amplifying case (each with green/red proof), and four independent judgment calls (TSan run, MANIFEST single-sourcing, tolerance spec alignment, single-runner build collapse) bundled with per-item evidence.

**Tech Stack:** C17 (`c_std_17`, `C_EXTENSIONS OFF`), CMake 3.20+, MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic` plus `-fsanitize=thread` on WSL, vendored ERFA 2.0.1 (read-only).

**Spec:** No design doc — authority is the four SDD ledgers under `.superpowers/sdd/` (review-findings-fix, jpl-body-numbering, tt-tdb-support, test-data-strategy) plus the cited file:line items. Item 7 resolves as documenting 1e-12 in the TT-TDB spec (code is green with zero MISMATCH), not tightening code to an unproven tolerance.

## Global Constraints

- C17 with `C_EXTENSIONS OFF`; MSVC `/W4 /permissive-` zero warnings; GCC/Clang `-Wall -Wextra -Wpedantic` zero warnings (lib AND tests).
- Physics constants live in `src/jpl_private.h`, never as literals in `src/*.c`.
- `src/erfa/` is read-only vendored code; never modify.
- Public headers accumulate Doxygen (`@brief/@param/@return/@note`); `WARN_AS_ERROR=YES` must stay clean; append, don't rewrite.
- Agent-run shell commands are PowerShell 5.1: chain with `;` (no `&&`/`||`), quote paths with spaces, use `workdir` instead of `cd`.
- Commit messages follow Conventional Commits with subject + body: `type(scope): imperative subject` (max 72 chars, lowercase after colon, no trailing period), blank line, body explaining what changed and why wrapped at 72 chars. Never commit with a bare one-line subject.
- Tests use explicit `JPL_TEST_CHECK` from `tests/jpl_test_util.h`, never `assert()`.
- Historical docs (`docs/specs/*`, `docs/plans/*`) are records: do NOT rewrite them, except the single tolerance-alignment edit to the TT-TDB spec explicitly required by Task 3 item 7 (a correction, not a rewrite).

---

## File map

- `src/jpl.c` — `checked_record_offset` guard reorder (item 1); light-time stacked-comment merge (item 9).
- `src/jpl_convenience.c` — `JPL_EARTH_MOON_BARYCENTER` symbols in range checks (item 8).
- `tests/test_all_ephemerides.c` — success-message reword (item 10).
- `tests/test_fixture.c` — fused-line DENUM prefix check (item 5), skip-vs-fail hardening or comment softening (item 6), fixture size assert (item 11), CHECK-path cleanup (item 13).
- `tests/test_aberration.c` — small-`s_au` SRS-amplifying differential case (item 4).
- `docs/specs/2026-09-18-tt-tdb-support-design.md` — tolerance alignment to 1e-12 (item 7, correction only).
- `CMakeLists.txt` — MANIFEST single-sourcing (item 3), single testpo runner collapse (item 12).
- `tests/data/MANIFEST.txt` — machine-readable values block if Task 3 chooses CMake-parses-MANIFEST (item 3).
- No TSan code changes (item 2 is a run + report).

---

### Task 1: Src one-liners, symbols, comments, message

**Files:**
- Modify: `src/jpl.c`, `src/jpl_convenience.c`, `tests/test_all_ephemerides.c`

**Interfaces:**
- Consumes: nothing (independent edits).
- Produces: nothing downstream (no API change).

**Steps:**

- [ ] **Step 1: Fix `checked_record_offset` guard order in `src/jpl.c:153-161`.** The current guard `if (index > (SIZE_MAX / b) - 2) return -1;` wraps when `SIZE_MAX / b < 2` (record_doubles > SIZE_MAX/16, exabyte-scale). Replace lines 156-157:
```c
    b = record_doubles * 8;
    if (index > (SIZE_MAX / b) - 2) return -1;
```
with:
```c
    b = record_doubles * 8;
    if (b == 0 || index > SIZE_MAX / b - 2) return -1;
```
Wait — that is the same expression. The actual fix must avoid the wrap: replace with:
```c
    b = record_doubles * 8;
    if (index + 2 > SIZE_MAX / b) return -1;
```
`index + 2` cannot overflow in practice (index comes from record counts bounded by file size), and `SIZE_MAX / b` is safe since `b >= 8` (record_doubles >= 1 established line 155). Verify: for `record_doubles = 357`, `b = 2856`, guard is `index + 2 > SIZE_MAX / 2856` — identical accept/reject as before for all realistic inputs; only the exabyte-scale wrap is closed.
- [ ] **Step 2: Use symbols in `jpl_planet_position` range checks in `src/jpl_convenience.c:22-24`.** Replace:
```c
    if(!((planet>=1&&planet<=11)||planet==13)) return JPL_ERROR_ARGUMENT;
    /* Centers: 0 (SSB) or bodies 1-13. Reject negatives and anything above 13. */
    if(center<0||center>13) return JPL_ERROR_ARGUMENT;
```
with:
```c
    if(!((planet>=1&&planet<=11)||planet==JPL_EARTH_MOON_BARYCENTER)) return JPL_ERROR_ARGUMENT;
    /* Centers: 0 (SSB) or bodies 1-13. Reject negatives and anything above 13. */
    if(center<0||center>JPL_EARTH_MOON_BARYCENTER) return JPL_ERROR_ARGUMENT;
```
Numerically identical (13 == 13); symbols prevent the next transposition-class bug. Keep the `1`/`11`/`0` literals (no symbols exist for range endpoints).
- [ ] **Step 3: Merge the stacked light-time comments in `src/jpl.c`.** Find the `jpl_get_state_lighttime` bounds comment pair (`/* Nutations... */` immediately followed by `/* Bodies 0-13... */`) and fold into one block:
```c
    /* Nutations/librations are angles with no light-time meaning; reject.
     * Bodies 0-13 only; JPL_EARTH_MOON_BARYCENTER (13) is the highest code. */
```
Keep both facts, one block.
- [ ] **Step 4: Reword the sweep success message in `tests/test_all_ephemerides.c:63`.** Replace:
```c
    puts("all supplied JPL DE binaries opened successfully");
```
with:
```c
    puts("all present JPL DE binaries opened successfully (skipped absent files)");
```
- [ ] **Step 5: Rebuild + run affected suites.**
Run: `cmake --build build --config Debug; ctest --test-dir build -C Debug -R "jpl_tests|jpl_convenience|jpl_all_ephemerides" --output-on-failure`
Expected: PASS (behavior identical; comment/symbol/message only, plus the guard reorder which no realistic input exercises).
- [ ] **Step 6: Commit.**
```bash
git add src/jpl.c src/jpl_convenience.c tests/test_all_ephemerides.c
git commit -m "fix(jpl): close record-offset guard wrap, symbolize body checks" -m "Reorder the checked_record_offset overflow guard so exabyte-scale record sizes cannot wrap it, use JPL_EARTH_MOON_BARYCENTER in the convenience range checks, merge a stacked comment, and reword the sweep success message for subset scans." -m "No behavior change on realistic inputs; all guards accept/reject identically below SIZE_MAX/16 scale."
```

### Task 2: Test strictness, SRS case, CHECK cleanup

**Files:**
- Modify: `tests/test_fixture.c`, `tests/test_aberration.c`

**Interfaces:**
- Consumes: `tests/test_testpo.c` T#15 exception constants (mirror exactly); `JPL_SRS_AU` semantics from `src/jpl_private.h`.
- Produces: stronger aberration regression + fixture strictness; no API change.

**Steps:**

- [ ] **Step 1: Fused-line DENUM check in `tests/test_fixture.c`.** The 7-token branch verifies `tokens[0]=="441"` but 6-token (`tokens[1]`) and 5-token (`tokens[0]`) fused labels skip prefix validation. Add after each `parse_fused_jd` success: verify the label starts with `"441"` followed by space-or-dash. Exact code for the 6-token branch (after line 118 `ok = ...`):
```c
        if (ok && !(tokens[1][0] == '4' && tokens[1][1] == '4' && tokens[1][2] == '1' &&
                    (tokens[1][3] == ' ' || tokens[1][3] == '-' || tokens[1][3] == '\0'))) ok = 0;
```
Hmm — tokens are NUL-terminated at whitespace, so `tokens[1][3]` can never be `' '`; the fused label is `"441-..."` (dash) or the bare `"441"` (NUL, only if JED missing — which `parse_fused_jd` already rejects). Simplify to: label must start with `"441-"`. Apply the same 4-line check to the 5-token branch on `tokens[0]`. If either rejects a committed slice line (it should not — slice is DE441-only), the slice itself is corrupt: stop and report BLOCKED with the line.
- [ ] **Step 2: Harden or document the skip paths in `tests/test_fixture.c:105,126`.** `if (ntok != 5 && ...) continue;` and `if (!ok) continue;` silently skip, contradicting the file comment's "nothing is ever skipped". Minimal fix preserving the `parsed==tested==121` pin: count skips and fail at the end. Add `size_t skipped = 0;` next to `tested/parsed`, increment in both `continue` paths (blank lines `ntok==0` may stay silent — count them separately or exclude: only count `ntok != 0` shapes), and before the `parsed == 121` check add:
```c
    JPL_TEST_CHECK(skipped == 0);
```
Update the file comment's "nothing is ever skipped" sentence to: "header/blank lines are skipped; every data-shaped line must parse and pass (skipped != 0 fails)."
- [ ] **Step 3: Assert fixture byte size in `tests/test_fixture.c`.** After the coverage-bounds check (line 82), add a portable size check on `argv[1]`:
```c
    { FILE *sz = fopen(argv[1], "rb");
      long cur, endpos;
      JPL_TEST_CHECK(sz != NULL);
      JPL_TEST_CHECK(fseek(sz, 0, SEEK_END) == 0);
      endpos = ftell(sz);
      fclose(sz);
      JPL_TEST_CHECK(endpos == 952848L); }
```
`952848` is the manifest size; `long` holds it on all supported platforms (MSVC long is 32-bit, 952848 fits). If CRLF ever touched the binary it would fail here — binaries are never converted, so this also pins that invariant.
- [ ] **Step 4: Small-`s_au` SRS-amplifying case in `tests/test_aberration.c`.** The existing differential block (1e-12 at `s_au=0.9998`) passes even against half-SRS code. Append a block with the same `t_ab` vectors but `s_au = 0.05` (20x amplification of `SRS/s`): first VERIFY it fails against half-SRS by temporarily halving `JPL_SRS_AU` in a scratch build (do not commit the halved constant — stash, run, unstash), then commit the passing test. First review the physics: `eraAb` requires Sun distance in AU, positive; 0.05 AU is inside Mercury's orbit but representable and finite — note the physical-validity judgment in the report. If the halved-constant run still passes at 0.05, try 0.01; if it still passes, report the finding (term may be below 1e-12 for these vectors at any valid distance) and keep the test as drift-pin documentation with a comment saying so. Exact block:
```c
    /* SRS-amplifying differential: at small Sun distance the solar term
     * exceeds 1e-12, so half-SRS code fails this block (verified by
     * stash experiment); pins JPL_SRS_AU against eraAb. */
    {
        double vc[3] = {2.1044018893653786e-5, -8.9108923304429319e-5, -3.8633714797716569e-5};
        double pn[3] = {-0.76321968546737951, -0.60869453983060384, -0.21676408580639883};
        double s2[6] = {pn[0], pn[1], pn[2], 0.0, 0.0, 0.0};
        double vo2[3] = {vc[0]*JPL_TEST_C_AU_PER_DAY, vc[1]*JPL_TEST_C_AU_PER_DAY, vc[2]*JPL_TEST_C_AU_PER_DAY};
        double got[6], be[3], exp[3];
        double s_au = 0.05;
        int k;
        for (k = 0; k < 3; ++k) be[k] = vc[k];
        jpl_apply_aberration_full(s2, vo2, s_au, got);
        eraAb(pn, be, s_au, sqrt(1.0 - (be[0]*be[0]+be[1]*be[1]+be[2]*be[2])), exp);
        for (k = 0; k < 3; ++k) JPL_TEST_CHECK(fabs(got[k] - exp[k]) < 1e-12);
    }
```
- [ ] **Step 5: CHECK-path cleanup in `tests/test_fixture.c`.** Audit every `JPL_TEST_CHECK` after `jpl_open` succeeds: on failure they return without `fclose`/`jpl_close`. Convert post-open CHECKs to explicit fail-with-cleanup:
```c
        if (!(cond)) { fprintf(stderr, "CHECK FAILED ..."); fclose(file); jpl_close(ephemeris); return 1; }
```
Apply to the provenance checks (lines 80-86), the `parsed/tested/span` pins (lines 202-204), and any other post-open CHECK. Pre-open CHECKs (argc, jpl_open itself) stay as-is — nothing to clean yet. If the conversion touches more than ~10 lines, a tiny local macro is acceptable:
```c
#define FIXTURE_CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); fclose(file); jpl_close(ephemeris); return 1; } } while (0)
```
scoped to this file (note: `file` may be NULL on early paths — `fclose(NULL)` is UB, so guard `if (file) fclose(file);`; `jpl_close(NULL)` is a documented no-op, safe).
- [ ] **Step 6: Run affected suites.**
Run: `cmake --build build --config Debug; ctest --test-dir build -C Debug -R "jpl_fixture|jpl_aberration" --output-on-failure`
Expected: PASS, including the new SRS block and the strict fixture pins.
- [ ] **Step 7: Commit.**
```bash
git add tests/test_fixture.c tests/test_aberration.c
git commit -m "test(harness): tighten fixture provenance and SRS regression" -m "Check the 441 label prefix on fused testpo lines, fail on any skipped data-shaped line, pin the fixture byte size, convert post-open checks to fail-with-cleanup, and add a small-Sun-distance eraAb differential that fails against half-SRS code." -m "Proven by stash experiment (halved SRS constant fails the new block) and CRLF-tolerant re-run."
```

### Task 3: TSan run, MANIFEST single-source, tolerance doc, single runner

**Files:**
- Modify: `docs/specs/2026-09-18-tt-tdb-support-design.md` (tolerance alignment only), `CMakeLists.txt` (MANIFEST parsing or drift check + runner collapse), `tests/data/MANIFEST.txt` (machine-readable block if chosen), possibly `tests/test_testpo.c` (if runner collapses to argv form — it already takes `<eph> <testpo>` argv, so likely no change needed)
- Test: WSL TSan build, configure-log assertions, full suites

**Interfaces:**
- Consumes: Task 1-2 landed code (independent files; can run in parallel with them if dispatched separately — sequential is safer for review).
- Produces: no API change; CMake behavior change (fewer targets) is user-visible in `ctest -N`.

**Steps:**

- [ ] **Step 1: ThreadSanitizer run on WSL (item 2).** Configure a scratch dir (MUST delete after — not gitignored):
```
JPLEPH=/mnt/d/ephem/jpl/bin JPL_TESTPO=/mnt/d/ephem/jpl/testpo cmake -S /mnt/d/src/libjpl -B /mnt/d/src/libjpl/build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=thread -g" -DLIBJPL_BUILD_TESTS=ON
cmake --build /mnt/d/src/libjpl/build-tsan --target jpl_mmap_tests jpl_frames_tests
./build-tsan/jpl_mmap_tests && ./build-tsan/jpl_frames_tests (with TSAN_OPTIONS=halt_on_error=1)
```
Expected: clean, no data-race reports (mmap reads touch only stack locals + read-only mapping). If TSan reports a race: stop, report BLOCKED with the full report — do NOT fix in this task (concurrency fix needs its own review). Delete `build-tsan/` after regardless of outcome and record the verdict (clean or report text) in the commit message body of the final commit OR the task report if clean (no commit needed for a clean run — record in report only).
- [ ] **Step 2: MANIFEST single-sourcing (item 3).** CMake parses `tests/data/MANIFEST.txt` at configure time (`file(STRINGS ...)` + regex) for sizes/hashes/URLs, failing configure if a value is missing. The values exist in exactly one authoritative place (the manifest); CMakeLists carries only a comment pointing at it. Proof: temporarily corrupt one value, show configure failing, restore.
- [ ] **Step 3: Tolerance doc alignment (item 7).** In `docs/specs/2026-09-18-tt-tdb-support-design.md`, the validation section says tolerance "~1e-13 following testeph.f practice, pinned exactly during implementation". Append one sentence: "Pinned at implementation as 1e-12 * fmax(1, |expected|) (see tests/test_testpo.c T#17 routing); all five *t suites validate with zero MISMATCH at this envelope." This is a correction, not a rewrite — one sentence, no other spec text changes.
- [ ] **Step 4: Single testpo runner collapse (item 12).** Replace the 29 per-DE executables with one `jpl_testpo_tests` target plus 29 `add_test(NAME jpl_testpo_NNN COMMAND jpl_testpo_tests <eph> <testpo>)` registrations (the runner already takes argv, so no C changes). Keep: `NOT TARGET` unnecessary for a single target (drop it), per-target warning flags (apply once), the `440/440t` fixed-vs-sweep skip logic (recheck: with one executable the sweep loop registers names, the fixed block registers names — the duplicate-name `continue()` skip stays, now guarding `add_test` duplication instead of target duplication; verify no `add_test` name collision causes FATAL_ERROR). Proof: `ctest -N` lists the same 29 `jpl_testpo_*` names; full testpo chunk green (run in 2-3 `-R` chunks; single full invocation exceeds tool timeout).
- [ ] **Step 5: Run affected suites + commit.**
Run: `cmake -S . -B build; cmake --build build --config Debug; ctest --test-dir build -C Debug -R "jpl_fixture|jpl_tt_tdb|jpl_testpo_440" --output-on-failure`
Expected: PASS with the collapsed runner (same names, one binary).
```bash
git add CMakeLists.txt docs/specs/2026-09-18-tt-tdb-support-design.md tests/data/MANIFEST.txt
git commit -m "build(tests): single testpo runner plus manifest single-source" -m "Collapse 29 per-DE executables into one jpl_testpo_tests binary with per-DE registrations, source download values from MANIFEST.txt so CMake and manifest cannot drift, and record the implemented 1e-12 T#17 tolerance in the spec." -m "TSan run on the mmap share path reported clean (or: see report). ctest -N lists identical suite names with one build target."
```
Stage only files actually touched; TSan evidence goes in the report (no commit needed for a clean run).

---

## Execution order

Task 1 then Task 2 then Task 3, strictly sequential (Task 2's fixture edits and Task 3's runner collapse both touch test registration context; Task 3's MANIFEST work benefits from Tasks 1-2 landed). Final gate: Windows full-suite chunks green + Linux Release green, zero warnings both, `ctest -N` suite names unchanged (except the single-runner collapse, which keeps names), every commit subject + body.
