# Public-Release Test Data Strategy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make fresh public clones green with no network while keeping full JPL reference validation one CMake option away.

**Architecture:** Commit a 931 KB DE441 slice plus its 121-line testpo slice as always-on fixtures; gate every local-data suite (integration + testpo) behind `LIBJPL_FETCH_EPHEMERIDES=OFF` by default with JPL downloads + manifest verification when ON; keep `JPLEPH`/`JPL_TESTPO` as source redirects behind the same switch.

**Tech Stack:** C17 (`c_std_17`, `C_EXTENSIONS OFF`), CMake 3.20+ (`file(DOWNLOAD EXPECTED_HASH)`), MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`.

**Spec:** docs/superpowers/specs/2026-09-19-test-data-strategy-design.md

## Global Constraints

- C17 with `C_EXTENSIONS OFF`; MSVC `/W4 /permissive-` zero warnings; GCC/Clang `-Wall -Wextra -Wpedantic` zero warnings (lib AND tests).
- Physics constants live in `src/jpl_private.h`, never as literals in `src/*.c`.
- `src/erfa/` is read-only vendored code; never modify.
- Public headers accumulate Doxygen (`@brief/@param/@return/@note`); `WARN_AS_ERROR=YES` must stay clean; append, don't rewrite.
- Agent-run shell commands are PowerShell 5.1: chain with `;` (no `&&`/`||`), quote paths with spaces, use `workdir` instead of `cd`.
- Commit messages follow Conventional Commits with subject + body: `type(scope): imperative subject` (max 72 chars, lowercase after colon, no trailing period), blank line, body explaining what changed and why wrapped at 72 chars. Never commit with a bare one-line subject.
- Tests use explicit `JPL_TEST_CHECK` from `tests/jpl_test_util.h`, never `assert()`.
- Historical docs are records: do NOT rewrite them.

---

## File map

- `tests/data/de441_2000_2010.eph` (new, ~931 KB) — sub-ephemeris via `sub_eph DE441.eph <out> 2000 2010` (JD 2451536.5-2455216.5), run by the maintainer, committed once.
- `tests/data/testpo_441_slice.txt` (new, 121 lines) — the `testpo.441` lines in the slice span, extracted verbatim (no reformatting).
- `tests/data/MANIFEST.txt` (new) — provenance + verification values for the fixture and all 4 download files.
- `tests/test_fixture.c` (new) — provenance asserts + slice cross-check via `jpl_get_state`.
- `CMakeLists.txt` — `LIBJPL_FETCH_EPHEMERIDES` option (default OFF); fixture suite always registered; integration + testpo suites gated behind the option; download + verify block; `JPLEPH`/`JPL_TESTPO` as source redirects.
- `README.md` — short "test data" section documenting the tiers (append, don't rewrite).
- Untouched: `src/*`, all existing `tests/test_*.c` (except CMake registration context).

---

### Task 1: Fixture files plus manifest

**Files:**
- Create: `tests/data/de441_2000_2010.eph`, `tests/data/testpo_441_slice.txt`, `tests/data/MANIFEST.txt`
- Test: byte sizes + provenance probe (no code yet)

**Interfaces:**
- Consumes: local `D:\ephem\jpl\bin\DE441.eph`, `D:\ephem\jpl\testpo\testpo.441`, `D:\ephem\jpl\tools\sub_eph.exe`.
- Produces: committed fixture + slice + manifest consumed by Tasks 2-3.

**Steps:**

- [ ] **Step 1: Generate the slice.** Run (exact provenance for the manifest):
```powershell
& "D:\ephem\jpl\tools\sub_eph.exe" "D:\ephem\jpl\bin\DE441.eph" "tests/data/de441_2000_2010.eph" 2000 2010
```
Expected: exit True, output names Start JD 2451536.5000 / End JD 2455216.5000, file ~952,848 bytes. Verify with `dump_eph`: version DE441, kernel size 2036, record size 8144.
- [ ] **Step 2: Extract the in-slice testpo lines.** With python3, read `D:\ephem\jpl\testpo\testpo.441`, keep data lines whose JED (last-dash extraction, same rule as `tests/test_testpo.c`) falls in [2451536.5, 2455216.5], write them byte-identical to `tests/data/testpo_441_slice.txt`. Expected: 121 lines. Verify count and that the first/last JEDs sit inside the bounds.
```powershell
python3 -c "lines = open(r'D:\ephem\jpl\testpo\testpo.441', errors='replace').read().splitlines(); ..."
```
(Implementer writes the full extraction inline; rule: split on whitespace, JED is the float after the last `-` of the fused leading token for 5/6-token lines else token index 2 for 7-token lines — mirror `test_testpo.c`, keep lines verbatim including newline style of the source file.)
- [ ] **Step 3: Write `tests/data/MANIFEST.txt`.** Exact content:
```
# libjpl test-data manifest. Verified 2026-09-19.
# Fixture (committed, always-on):
fixture.eph=tests/data/de441_2000_2010.eph size=952848 source=DE441.eph years=2000-2010 tool="sub_eph DE441.eph <out> 2000 2010" coverage=2451536.5-2455216.5 denum=441
fixture.testpo=tests/data/testpo_441_slice.txt lines=121 source=testpo.441 span=2451536.5-2455216.5
# Downloads (LIBJPL_FETCH_EPHEMERIDES=ON only):
de440.bin.url=https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/linux_p1550p2650.440
de440t.bin.url=https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440t/linux_p1550p2650.440t
testpo.440.url=https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/testpo.440 sha256=45084a7f406b8e41032e7d99dd8042d4df97a9f752a68c06d1a27af16693429f size=858238
testpo.440t.url=https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440t/testpo.440t sha256=9801f019be5c335d1e22697ea9fb5888becfd0c623a50c3372fe4491c7a3f2ad size=858238
# Binary sizes are captured from the FTP listing at implementation time
# (see Task 2) and filled in here: de440.bin.size=<bytes> de440t.bin.size=<bytes>.
# Local checksums.txt hashes describe different spans and MUST NOT be used.
```
(Byte sizes for the two binaries are display-rounded 97.5M/107.5M in the listing; the implementer captures exact bytes from the FTP table in Task 2 and fills the `<bytes>` slots before committing. If the listing is unreachable, record sizes by downloading once and measuring, noting the method.)
- [ ] **Step 4: Commit (fixtures + manifest).**
```bash
git add tests/data/de441_2000_2010.eph tests/data/testpo_441_slice.txt tests/data/MANIFEST.txt
git commit -m "test(data): commit DE441 2000-2010 slice plus manifest" -m "Add a 931 KB sub-ephemeris (sub_eph from DE441.eph, years 2000-2010, JD 2451536.5-2455216.5) and the 121 in-span testpo.441 lines as always-on fixtures, with a manifest recording provenance, download URLs, testpo hashes, and verification dates." -m "No code change; Task 2 wires the fixture suite, Task 3 the download tier."
```

### Task 2: Fixture suite (always-on, no network)

**Files:**
- Create: `tests/test_fixture.c`
- Modify: `CMakeLists.txt` (register `jpl_fixture`, always-on)
- Test: `ctest -R jpl_fixture` on a clean configure with NO `JPLEPH`/`JPL_TESTPO` set

**Interfaces:**
- Consumes: `tests/data/*` from Task 1; `jpl_open`, `jpl_get_header`, `jpl_get_state` (unchanged APIs).
- Produces: `jpl_fixture` suite green offline; pattern for Task 3's gated suites.

**Steps:**

- [ ] **Step 1: Write the failing test.** Create `tests/test_fixture.c`: open `tests/data/de441_2000_2010.eph` via a path relative to the test's working directory problem — CTest runs with `WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}` by default, so pass the fixture path as an argv like the real-ephemeris test does (`add_test(NAME jpl_fixture COMMAND jpl_fixture_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph" "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt")`). Checks, all `JPL_TEST_CHECK`:
```c
#include "jpl.h"
#include "jpl_test_util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    jpl_ephemeris *e = NULL;
    const jpl_header *h;
    FILE *f;
    char line[1024];
    size_t tested = 0;
    if (argc != 3) { fprintf(stderr, "usage: %s <eph> <testpo-slice>\n", argv[0]); return 2; }
    JPL_TEST_CHECK(jpl_open(argv[1], &e) == JPL_OK && e != NULL);
    h = jpl_get_header(e);
    JPL_TEST_CHECK(h != NULL);
    JPL_TEST_CHECK(h->ephemeris_number == 441);
    JPL_TEST_CHECK(h->start_jd == 2451536.5 && h->end_jd == 2455216.5);
    JPL_TEST_CHECK(jpl_has_tt_tdb(e) == 0);
    f = fopen(argv[2], "r"); JPL_TEST_CHECK(f != NULL);
    while (fgets(line, sizeof line, f) != NULL) {
        /* same 5/6/7-token parse as test_testpo.c, jd/target/center/comp/expected */
        /* jpl_get_state(e, jd, target, center, state); tol 5e-13*fmax(1,|exp|); ++tested */
    }
    fclose(f);
    JPL_TEST_CHECK(tested == 121);
    jpl_close(e);
    printf("fixture validated %zu lines\n", tested);
    return 0;
}
```
(Full parse body mirrors `tests/test_testpo.c` T#17-free path; the implementer copies that logic rather than inventing a parser. `tested == 121` pins the manifest line count; any manifest regeneration updates both.)
- [ ] **Step 2: Register always-on in `CMakeLists.txt`.** After the `jpl_tt_tdb_tests` block:
```cmake
    add_executable(jpl_fixture_tests tests/test_fixture.c)
    target_link_libraries(jpl_fixture_tests PRIVATE jpl)
    target_compile_features(jpl_fixture_tests PRIVATE c_std_17)
    add_test(NAME jpl_fixture
        COMMAND jpl_fixture_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt")
```
Append `jpl_fixture_tests` to the warnings `foreach` list.
- [ ] **Step 3: Run offline (prove no network/env needed).**
Run: `cmake -S . -B build; cmake --build build --config Debug --target jpl_fixture_tests; ctest --test-dir build -C Debug -R "^jpl_fixture$" --output-on-failure`
with `JPLEPH` and `JPL_TESTPO` explicitly UNSET in the environment.
Expected: PASS, `fixture validated 121 lines`.
- [ ] **Step 4: Commit.**
```bash
git add tests/test_fixture.c CMakeLists.txt
git commit -m "test(fixture): validate committed DE441 slice with zero network" -m "Add jpl_fixture, always registered: asserts DE441 provenance and coverage bounds on the committed 2000-2010 slice, then cross-checks all 121 committed testpo lines. Takes fixture paths as argv so CTest needs no working-directory coupling." -m "Proven with JPLEPH/JPL_TESTPO unset: a fresh public clone gets this green with no downloads."
```

### Task 3: Fetch tier (opt-in downloads, gated suites)

**Files:**
- Modify: `CMakeLists.txt` (option + download/verify + gating)
- Modify: `README.md` (test-data section, append)
- Test: default configure (no network, suites absent) + ON configure (downloads, 4+ suites present)

**Interfaces:**
- Consumes: `tests/data/MANIFEST.txt` values; existing `jpl_real_ephemeris_tests`, `jpl_all_ephemerides_tests`, `test_testpo.c` runner (unchanged).
- Produces: `ctest -N` shows 8 suites by default, 8+2+2+2 with the option ON.

**Steps:**

- [ ] **Step 1: Add the option + download block.** In `CMakeLists.txt` after the fixture block:
```cmake
option(LIBJPL_FETCH_EPHEMERIDES "Download JPL DE440/DE440t validation data (approx 210 MB)" OFF)
set(LIBJPL_EPHEMERIS_DIR "${CMAKE_CURRENT_BINARY_DIR}/ephemeris" CACHE PATH "Downloaded/located JPL test data")
```
When `LIBJPL_FETCH_EPHEMERIDES` is ON: for each of the 4 files, resolve source as `JPLEPH`-dir file if present else URL download via `file(DOWNLOAD <url> <dest> EXPECTED_HASH SHA256=<hash for testpo> STATUS st)`; for binaries assert exact byte size with `file(SIZE ...)` against the manifest and `message(FATAL_ERROR ...)` naming file/expected/actual/URL on any failure (binaries carry no pinned hash per the span-mismatch finding). Testpo downloads use `EXPECTED_HASH` directly (CMake fails loudly itself). All failures are `FATAL_ERROR` with file + expected vs actual + URL + manifest-update pointer. When OFF: none of this runs, no network touched.
- [ ] **Step 2: Gate the existing suites behind the option.** Replace the `if(NOT "$ENV{JPLEPH}" ... DE441.eph)` integration guard and the `if(NOT "$ENV{JPLEPH}" ...)` testpo guard with `if(LIBJPL_FETCH_EPHEMERIDES)`, pointing their paths at `${LIBJPL_EPHEMERIS_DIR}` (which `JPLEPH`/`JPL_TESTPO` env dirs override file-by-file when set: if `$ENV{JPLEPH}/DE440.eph` exists use it, else download; same for testpo via `$ENV{JPL_TESTPO}`). Keep the per-file `EXISTS` checks so a partial local dir + downloads compose. Net effect: default `ctest -N` lists 8 suites (6 unit + `jpl_tt_tdb` + `jpl_fixture`); ON lists those plus `jpl_real_de441`, `jpl_all_ephemerides`, `jpl_testpo_440`, `jpl_testpo_440t`.
- [ ] **Step 3: Prove the default is offline.** Fresh configure with the option OFF (default) and no env vars: `ctest -N` must show exactly the 8 always-on suites and `cmake` must not attempt any download (verify by absence of `Downloading` lines in configure output). Then configure with `-DLIBJPL_FETCH_EPHEMERIDES=ON` (needs network + ~210 MB): all 12+ suites register; `ctest -R "jpl_testpo_440|jpl_real_de441"` green. On this maintainer machine, prefer `JPLEPH`/`JPL_TESTPO` pointed at local dirs to avoid re-downloading; ALSO run one true-download configure into a scratch dir to prove the URLs + hashes (at least the two testpo files; binaries by size) — delete the scratch dir after.
- [ ] **Step 4: Document in `README.md`.** Append a `## Test data` section (do not rewrite existing content): always-on fixture (what it is, where it lives, provenance one-liner), the fetch option with the ~210 MB note and JPL source URLs, the manifest-update procedure on JPL re-cuts, and the maintainer note that the 31-file sweep stays local via `JPLEPH`.
- [ ] **Step 5: Commit.**
```bash
git add CMakeLists.txt README.md tests/data/MANIFEST.txt
git commit -m "build(tests): gate network test data behind fetch option" -m "Add LIBJPL_FETCH_EPHEMERIDES=OFF by default: plain configures never touch the network and register 8 suites. When ON, DE440/DE440t plus testpo files download with manifest verification (sizes for binaries, SHA-256 for testpo) and register integration plus reference suites." -m "JPLEPH/JPL_TESTPO become per-file source redirects behind the same switch instead of ambient auto-registration triggers."
```

---

## Execution order

Task 1 then Task 2 then Task 3 (each builds on the last). Final gate: default configure green offline with 8 suites; ON configure green with downloads; Windows Debug + Linux Release zero warnings; every commit subject + body.
