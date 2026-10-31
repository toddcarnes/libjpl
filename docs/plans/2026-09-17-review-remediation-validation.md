# Validation Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the test system reject known defects and establish independent numerical, provenance, concurrency, and platform evidence.

**Architecture:** Share only the duplicated testpo parsing/comparison policy, keep fixture strictness separate, and exercise runners as processes with controlled data. Pin small independently generated truth vectors, preserve external provenance, and run the same CTest suites across supported platforms.

**Tech Stack:** C17, CMake 3.20+/CTest, native Windows/POSIX threads, Doxygen gates, GitHub Actions, opt-in vendored-ERFA reference generation.

**Spec:** [Approved design](../specs/2026-09-17-review-remediation-design.md); [master plan and interfaces](2026-09-17-review-remediation.md).

## Global Constraints

- All computational Julian epochs use actual arrays of two doubles. Both parts survive arithmetic and ERFA calls.
- All public AU-valued results and conversion helpers use the exact modern definition, `1 AU = 149597870.7 km`. Original header constants and raw records remain unchanged.
- Keep C17, `C_EXTENSIONS OFF`, three installed public headers, no new runtime or ordinary-build dependencies, and offline default tests.
- License first-party code under MIT, with Todd Carnes as copyright holder. Do not edit vendored ERFA sources or relicense external test data.
- Checks remain active in Release. Do not widen tolerances simply to make a suite green or count a skipped configuration as verified.
- Follow master dependencies, shared interfaces, common Debug/Release cycle, and commit-permission rules. Test fixtures use dedicated build-tree working directories, not arbitrary maintainer paths.

---

## Task T03: Checked testpo parsing, component mapping, and tolerances

**Dependencies:** T02. **Coverage:** S05/S06/S07, R13c/d; foundation for T15.

**Files:** Create `tests/jpl_testpo_util.h`, `tests/jpl_testpo_util.c`, `tests/test_testpo_parser.c`; modify `CMakeLists.txt`. Do not replace the live runners yet; T15 does that after public date migration.

**Interfaces:** Produces `jpl_testpo_row`, `jpl_testpo_parse_row`, `jpl_testpo_state_index`, and `jpl_testpo_tolerance` exactly as the master defines. Register target `jpl_testpo_parser_tests`, CTest `jpl_testpo_parser`; link private date helpers through `jpl`.

- [ ] Add these parser assertions, plus sentinel-preservation tests on invalid rows. A copied version of the fixture's current six-token rule must fail the second case:

```c
jpl_testpo_row r;
JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451544.5 4 12 1 1.25", &r) == 1);
JPL_TEST_CHECK(r.denum == 441 && r.jd[0] == 2451544.5 && r.expected == 1.25);
JPL_TEST_CHECK(jpl_testpo_parse_row("441 -9999.01.01-1931076.5 4 12 1 1.25", &r) == 1);
JPL_TEST_CHECK(r.jd[0] == -1931076.5);
JPL_TEST_CHECK(jpl_testpo_parse_row("441-13200.09.01-3099998.5 4 12 1 1.25", &r) == 1);
JPL_TEST_CHECK(jpl_testpo_parse_row("441 2000.01.01 2451545.5 17 0 1 nan", &r) == -1);
```

- [ ] Tokenize into bounded local storage; reject more/fewer than the three supported token counts, oversize tokens/lines, incomplete numeric conversions, invalid integer ranges, nonfinite expected values, and ERANGE where it changes the value. Validate the DENUM integer or DENUM prefix rather than hard-coding `441-` into a date token. Accept a documented `t` label suffix only when actually present in the source format; numeric DENUM remains the comparison key.
- [ ] Parse JD sign, integer digits and fractional digits separately, then normalize the resulting two-part pair. Handle negative fractional dates by applying the sign to both parts, including `-0.5`. Do not use `strtod` on the complete large JD. Add this precision regression:

```c
JPL_TEST_CHECK(jpl_testpo_parse_row(
    "441 2000.01.01 2451545.000000000001 4 12 1 1", &r) == 1);
JPL_TEST_CHECK(r.jd[0] == 2451545.0 && r.jd[1] > 0.0);
JPL_TEST_CHECK(fabs(r.jd[1] - 1e-12) < 1e-27);
```

- [ ] Extract fused JD fields according to the calendar-label grammar, not by indiscriminately treating an exponent minus sign as a date separator. The accepted JPL date grammar is decimal fixed-point; reject unsupported exponent notation for the JD field explicitly. Coordinates may use the exponent forms established by the authoritative files. Test all three row forms, blank rows, bad DENUM, huge body/component integers, signed zero, missing separators, and numeric over/underflow.
- [ ] Implement component mapping and tolerance as one shared policy:

```c
int jpl_testpo_state_index(const jpl_testpo_row *r) {
    if (r->target == 14) {
        static const int indices[4] = {0,1,3,4};
        return r->component >= 1 && r->component <= 4
            ? indices[r->component - 1] : -1;
    }
    return r->target >= 1 && r->target <= 15 &&
           r->component >= 1 && r->component <= 6
        ? r->component - 1 : -1;
}
```

For tolerance use absolute `5e-13`, except T#15 X#3 gets `5e-13*(1+100*abs(delta_days)/365.25)`, T#15 X#4-6 gets `5e-12`, and T#17 X#1-2 gets `1e-12`. Derive delta_days by subtracting `{2440400.5,0}` through date helpers. No expected-coordinate-magnitude factor. Invalid rows must never reach the tolerance function.

- [ ] Test a far-epoch psi value with expected magnitude `1e6`: an error `1e-3` rad must fail the new bound at JD `{8000000.5,0}`, whereas the old double-scaled rule accepted it. Test nutation rates map to slots 3/4 and invalid X#5/6 fails.
- [ ] Run `jpl_testpo_parser` and common cycle. Document that the helper is now independently tested but runner false-pass findings remain open until T15. Checkpoint: `test(reference): add checked two-part testpo parsing`.

## Task T14: Actual concurrent reads and ownership lifetimes

**Dependencies:** T12, T13. **Coverage:** G03/G04, B06.

**Files:** Create `tests/test_threads.c`; modify `tests/test_mmap.c`, `CMakeLists.txt`.

**Interfaces:** Public APIs only. Register target `jpl_threads_tests`, CTest `jpl_threads`; native thread details stay in this test file. Find/link `Threads::Threads` for tests only.

- [ ] Build a valid multi-record mock with independently calculable linear Mars/EMB coefficients and a distinct one-component TT-TDB table. Each query selects a different record/epoch deterministically. Do not calculate expected results by calling the library on another handle.
- [ ] Define each worker's own result/error storage and use the same query loop on Windows and POSIX:

```c
typedef struct worker_args {
    jpl_ephemeris *ephemeris;
    int worker_index, failed;
} worker_args;
static void run_queries(worker_args *a) {
    for (int k = 0; k < 1000; ++k) {
        double jd[2] = {2451545.0 + (double)((k + a->worker_index) % 8), 0.25};
        double state[6];
        if (jpl_get_state(a->ephemeris, jd, JPL_MARS, 0, state) != JPL_OK) {
            a->failed = 1;
            return;
        }
        /* For this mock x(t) = 1 + 0.01*(t-2451545), in modern AU. */
        double expected = 1.0 + 0.01*((jd[0]-2451545.0)+jd[1]);
        if (fabs(state[0]-expected) > 1e-13 || fabs(state[3]-0.01) > 1e-14) {
            a->failed = 1;
            return;
        }
    }
}
```

- [ ] Start at least four workers behind a real start gate: Windows event/WaitForSingleObject and POSIX mutex/condition variable. Worker wrappers call `run_queries` with their own arguments; the parent joins every thread and checks its result. Add TT-TDB and angular queries in separate deterministic loops with analytic constants/rates. Do not use sleeps or a sequential alternation as evidence of concurrency.
- [ ] First share one mmap handle across readers; next give each reader an independent FILE clone. In a third case close the unused original after clones are created and let clone readers continue; close siblings in a different order after their own joins. Keep a designated source handle alive for synchronized clone/close churn while other clones read. Never close a handle another worker is using.
- [ ] Demonstrate sensitivity using a disposable local mutation that re-enables shared mmap cache writes, then run TSan and confirm the race is detected; restore only that mutation before acceptance. The production behavior need not fail an ordinary unsanitized run to be an effective concurrency test.
- [ ] Set a generous CTest timeout such as 120 seconds to catch hangs, not a tight timing assertion. Run normal Debug/Release tests, then TSan under T22 and ASan lifetime checks. Checkpoint: `test(concurrency): exercise shared mappings and clone lifetimes`.

## Task T15: Integrate strict validators and test their failures

**Dependencies:** T03, T07. Execute before T08. **Coverage:** all R13 subdivisions, S05-S07.

**Files:** Modify `tests/test_testpo.c`, `tests/test_fixture.c`, `tests/test_all_ephemerides.c`, `CMakeLists.txt`; create `tests/make_validator_data.c`, `tests/cmake/test_validators.cmake`.

**Interfaces:** Consumes T03's shared row parser/tolerance and T07's pair API. Produces `jpl_validator_data` generator and `jpl_validator_negative` CTest script. Build `jpl_testpo_tests` whenever tests are enabled, independent of downloads; only its large-data registrations remain opt-in. Body comparison stays in existing file-AU until T08 updates it in one coordinated change.

- [ ] Implement `jpl_validator_data <output-dir>` to generate small valid ephemerides and data files using the T04 mock writer. Cases: good body rows; 100 T#17 NaN rows; 100 finite valid rows plus one missing ordinary-body query; a present angular dataset producing a deliberate read error; wrong DENUM; malformed six-token row; nonexistent component; positive/negative numeric overflow; overlong row; and a sweep directory containing one good named DE file plus one five-byte named DE file. Add proper full-file headers and EOT for runner inputs, so a NaN test does not fail merely due to missing framing.
- [ ] Create a CMake process driver that requires the right failure, not just any nonzero exit. The runner must exist; the generator must succeed; expected data errors must exit 1 and match a diagnostic category:

```cmake
execute_process(COMMAND "${RUNNER}" "${WORK}/tt.eph" "${WORK}/nan.testpo"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "1" OR NOT "${err}" MATCHES "nonfinite|invalid.*row")
  message(FATAL_ERROR "NaN reference did not fail correctly: ${rc}\n${out}\n${err}")
endif()
```

- [ ] Replace both runners' duplicated data-row parsing with T03 helpers. Full files require a recognized header/EOT transition; arbitrary data-shaped malformed lines after EOT fail. Data-only committed fixture mode tolerates only blank lines. Validate row DENUM against the open binary before any coverage skip. Validate target/center/component, including T#17, before calling the API; retain a counted, explicit T#16 exclusion.
- [ ] On in-coverage body errors, fail unconditionally. Skip angular datasets only after checking their absent header pointer, then require all present datasets to return OK. Report counts by reason and body/component, not only a total >=100. Keep the full runner's minimum meaningful comparison threshold and the fixture's exact pinned coverage; neither threshold excuses a malformed row.
- [ ] Use state indices 0,1,3,4 for nutation X#1-4; use T#17 seconds/rate directly. Apply the shared absolute tolerance and corrected psi epoch factor. Add a negative far-epoch perturbation and a finite wrong-sign TT-TDB value to ensure the test checks values, not only finiteness.
- [ ] Check read and close errors explicitly. To exercise the post-read failure branch portably, compile a test-only runner variant with `JPL_TEST_READER_FAULT=1` that ORs a simulated read failure into `ferror(file)` after 100 otherwise valid rows. Register it only for negative tests; the normal runner has no fault behavior.

```c
int read_failed = ferror(file);
#if defined(JPL_TEST_READER_FAULT)
read_failed = 1;
#endif
if (read_failed) {
    fprintf(stderr, "reference input read error\n");
    fclose(file);
    jpl_close(ephemeris);
    return 1;
}
```

- [ ] In the sweep, classify `stat`/platform-equivalent ENOENT or ENOTDIR as absent; permission/other errors fail. If a path exists, any open/read failure fails. Keep the no-files-opened failure. Tests must not treat a privileged CI account's ability to read a permission-restricted file as a pass for the permission branch; report that platform case separately.
- [ ] Build and run `jpl_validator_negative`, `jpl_testpo_parser`, `jpl_fixture`, and the common cycle. Test 5/6/7-token rows in both fixture and full-file modes. Checkpoint: `fix(tests): reject false-pass reference and sweep cases`.

## Task T16: Independent scientific truth and complete input/output matrix

**Dependencies:** T10, T11, T12, T15. **Coverage:** G01/G02/G05/G06/G11/G12/G13, scientific B01-B04.

**Files:** Create `tests/test_numeric.c`, `tests/data/numerical_reference.h`, `tests/data/numerical_reference.md`, `tools/generate_numerical_reference.c`; modify existing TT-TDB/frame/light-time/convenience/aberration/API tests and `CMakeLists.txt`.

**Interfaces:** Register `jpl_numeric`. Add `jpl_generate_reference` as an EXCLUDE_FROM_ALL test-development target, compiled with upstream ERFA sources directly and not linked to libjpl. It accepts `--output <new-header-path>` and writes that selected file only when explicitly invoked; reject an existing output path to prevent accidental truth replacement. Ordinary tests consume checked-in constants and never regenerate them. The generated header defines these test-only types and arrays:

```c
typedef struct jpl_time_reference {
    double tt[2], tdb_minus_tt_seconds, icrs_to_ecliptic[3][3];
} jpl_time_reference;
typedef struct jpl_apparent_reference {
    double tt[2], state[6], delay_days;
} jpl_apparent_reference;
/* Generator emits initialized static const arrays named
 * jpl_time_references and jpl_apparent_references, plus their size_t counts
 * jpl_time_reference_count and jpl_apparent_reference_count. */
```

- [ ] Replace TT-TDB finiteness-only assertions with exact independent values and rates. For the existing first linear mock:

```c
const double mid[2] = {2451545.0, 0.5};
double sec, rate;
JPL_TEST_CHECK(jpl_get_tt_minus_tdb(e, mid, &sec, &rate) == JPL_OK);
JPL_TEST_CHECK(fabs(sec - 0.001) < 1e-18);
JPL_TEST_CHECK(fabs(rate - 0.00004) < 1e-19);
```

Give all four appended-table subintervals different coefficients and sample each midpoint, both ends, and low-part offsets around boundaries. Duplicate representative cases across FILE/mmap and byte orders.

- [ ] Add a degree-four scalar polynomial reference independent of the production Chebyshev recurrence. With coefficients `c0=1,c1=2,c2=3,c3=4,c4=5`, use expanded polynomial `40*x^4+16*x^3-34*x^2-10*x+3` and derivative `160*x^3+48*x^2-68*x-10`, then multiply by `2/substep` for daily rate. Assert at -1, 0, 1 and several interior values; use scaled coefficients for low-part observability.
- [ ] Generate absolute geocentric time and frame reference values by calling upstream primitives directly, independently of libjpl's wrappers. First verify the published ERFA anchor so the generator's upstream configuration is checked:

```c
double anchor = eraDtdb(2448939.5, 0.123, 0.76543, 5.0123, 5525.242, 3190.0);
if (fabs(anchor - (-0.1280368005936998991e-2)) > 1e-15) return 1;
const double epochs[][2] = {
    {2451545.0,0.0}, {2451620.5,0.0}, {2451800.5,0.0},
    {-3100000.5,0x1p-40}, {8000000.5,0x1p-40}
};
```

For each epoch write `eraDtdb(date1,date2,0,0,0,0)` in seconds and a direct `eraEcm06` matrix. Compare ICRS-to-ecliptic outputs with that matrix and `jpl_frame_rotation_matrix` with its transpose, not the same orientation. Serialize constants as C hexadecimal doubles with `%a`, retaining both input parts. For these input pairs with zero/tiny low parts, compare recovered correction seconds through the small differences `(output[0]-input[0]) + (output[1]-input[1])`, not through subtraction of two pre-summed full JDs. Record ERFA version/source revision, the published anchor citation, command and generator checksum. Distant-epoch vectors verify arithmetic/convention, not claimed physical model accuracy there.

- [ ] Add an independent analytic end-to-end apparent-position oracle for a linear mock, not a composition of libjpl functions. Define barycentric target `P={1,0.4,-0.2}`, `V={0.005,-0.003,0.001}` and observer `O={0.1,-0.2,0.03}`, `W={0.002,0.004,-0.001}` in modern AU/day at a stated reference TDB epoch. The Sun is stationary at the origin. For a selected TT input, use direct upstream ERFA to obtain the correction and evaluate these linear functions at the resulting local TDB offset. Solve delay by the stable analytic positive root:

```c
/* r = target(receive) - observer(receive), v = target velocity. */
double rv = r[0]*v[0] + r[1]*v[1] + r[2]*v[2];
double vv = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
double rr = r[0]*r[0] + r[1]*r[1] + r[2]*r[2];
double tau = rr / (sqrt(rv*rv + (c*c-vv)*rr) + rv);
```

Form the emission-to-reception vector directly from those polynomials, apply upstream `eraAb` with `W/c`, `sqrt(1-|W/c|^2)` and observer-to-Sun distance, restore range, and retain `V-W` for velocity. Pin the generated state and delay. This validates the wrapper chain and independent light-time solution without treating production calls as truth. Add distinct observer-acceleration cases with an independently evaluated reception polynomial to catch stale observer epochs.

- [ ] Compile the generator explicitly with `cmake --build build/remediation --config Debug --target jpl_generate_reference`, run it against a temporary output under the build tree, inspect every generated value/provenance, and use the approved file-edit workflow to add reviewed first-party generated reference artifacts. Generation must not silently overwrite tracked truth. Cross-check the analytic equation residual and published frame/time anchor before accepting values.
- [ ] Extend `test_numeric` and existing tests to compare API results to the pinned arrays. Compute date differences as pairs and compare the small offset in seconds, not a rounded full JD. Include equivalent input splits, both correction signs, inverse residuals, legacy-frame rejection, modern-AU mock, whole-state finiteness, zero/transverse/radial aberration, exact aliasing, and untouched outputs after each error category.
- [ ] Prove sensitivity by disposable mutations: replace time conversion with identity, flip the correction sign, remove low parts, use emission-time observer velocity, omit SRS, swap Earth/EMB, and return zero TT-TDB. Each targeted test must fail. Restore only the introduced mutation before proceeding; do not change tolerances to accommodate it.
- [ ] Run common cycle and document independent versus shared-upstream evidence honestly. The published upstream primitive is independent of the wrapper, not a second physical model. Real geometric DE/testpo remains the separate data check; no unsupported Horizons/SPICE convention is compared without explanation. Checkpoint: `test(numerics): pin independent time and apparent-state references`.

## Task T17: Fixture integrity, interior identities, and provenance

**Dependencies:** T15. **Coverage:** G07/G08.

**Files:** Modify `tests/data/MANIFEST.txt`, `tests/test_fixture.c`; create `tests/data/fixture_rows.txt`, `tests/cmake/check_fixture.cmake`, `tests/cmake/test_fixture_integrity.cmake`, and `tests/data/PROVENANCE.md`; modify `CMakeLists.txt`.

**Interfaces:** Manifest gains explicit binary SHA-256, canonical-text SHA-256, and identity-list SHA-256 keys. CTest names `jpl_fixture_integrity` and `jpl_fixture_integrity_negative` run offline. Hash validation uses CMake, not a new library crypto dependency.

- [ ] Independently recover the exact source ephemeris/testpo artifact identities and the slicing tool revision. Consult the existing manifest, maintainer-local source metadata, and JPL source URLs without changing original files. Record byte sizes, spans, hashes, the full `sub_eph DE441.eph <out> 2000 2010` invocation with actual reviewed paths, and extraction rules for 121 rows. If evidence is unavailable, stop that provenance subtask and record the blocker; a checksum of the current artifact is not fabricated historical provenance.
- [ ] Create a small CMake checker that reads expected hashes from the manifest and hashes the binary exactly. Hash reference text after only CRLF-to-LF normalization:

```cmake
file(SHA256 "${BINARY_FIXTURE}" actual_binary)
file(READ "${TEXT_FIXTURE}" text)
string(REPLACE "\r\n" "\n" canonical "${text}")
string(SHA256 actual_text "${canonical}")
if(NOT actual_binary STREQUAL expected_binary OR NOT actual_text STREQUAL expected_text)
  message(FATAL_ERROR "Committed fixture integrity mismatch")
endif()
```

- [ ] Populate the manifest with the actual verified hashes, not example digests. Produce an ordered identity list from the authoritative filtered reference source, each row recording decimal JD text, target, center, and component. Compare the committed slice's parsed identities to it. Check all rows, not just endpoints, uniqueness count, or total count.
- [ ] Add negative tests on build-tree copies: flip a binary byte; replace 119 interior rows with duplicates while preserving endpoints and total count; alter one target/component; alter a coordinate; and change LF to CRLF. The first four fail and the last succeeds. Never mutate the source fixture in place.
- [ ] Correct the existing comment claiming count/endpoints prove byte-verbatim provenance. Document exactly which checks establish identity, data integrity, and upstream provenance, including any outstanding evidence gap. If authoritative reconstruction differs, obtain review before replacing data rather than changing the expected hash automatically.
- [ ] Run integrity, negative-integrity, and numerical fixture tests in Debug/Release and on LF/CRLF checkout configurations. Record provenance closure separately from mere checksum completion. Checkpoint: `test(data): pin fixture integrity and source provenance`.

## Task T22: Cross-platform CI and reproducible validation

**Dependencies:** T14, T16, T17, T19, T20, T21. **Coverage:** G09, R03/R12 platform evidence and all final gates.

**Files:** Create `.github/workflows/ci.yml`, `.github/workflows/ephemerides.yml`; modify `docs/validation.md`, `CMakeLists.txt` only for required test labels/timeouts/feature diagnostics.

**Interfaces:** Runs the existing CMake/CTest interface. Default CI remains offline with respect to ephemeris data. Workflow execution/push is not authorized merely by writing YAML; local tests and later actual CI runs are recorded separately.

- [ ] Add Debug/Release jobs for Windows MSVC, Ubuntu GCC, Ubuntu Clang, and macOS Clang. Use `actions/checkout@v4` initially or the repository's vetted pinned revision when available; do not invent action SHAs. Linux/macOS configure single-config builds with explicit build type. Windows uses `--config` and CTest `-C`. A minimal single-config job step is:

```yaml
- name: Configure
  run: cmake -S . -B build/ci -DCMAKE_BUILD_TYPE=${{ matrix.config }} -DLIBJPL_FETCH_EPHEMERIDES=OFF
- name: Build
  run: cmake --build build/ci --config ${{ matrix.config }} --parallel
- name: Test
  run: ctest --test-dir build/ci -C ${{ matrix.config }} --output-on-failure
```

- [ ] Add a Windows clang-cl job using an initialized MSVC environment, Ninja, and `-DCMAKE_C_COMPILER=clang-cl`; run the installed consumer to prove there is no `m.lib` dependency. Missing LLVM/SDK components fail configuration clearly. Add a Linux 32-bit job with multilib toolchain installed explicitly in CI and `-DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32`, including large-file tests. Record address-space-dependent mmap rejections as expected explicit cases, not silent omissions.
- [ ] Add separate ASan/UBSan and TSan Linux jobs. Commands for a supported local POSIX shell or the CI runner:

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined,float-cast-overflow" -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure -E 'jpl_install|jpl_docs_paths'
cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" -DLIBJPL_FETCH_EPHEMERIDES=OFF
cmake --build build/tsan --parallel
ctest --test-dir build/tsan -R 'jpl_threads|jpl_mmap' --output-on-failure
```

Installed-package and copied-source docs tests run in regular jobs; they are excluded only from sanitizer sub-builds to avoid conflating a newly configured unsanitized consumer with sanitizer instrumentation. Do not combine ASan and TSan. An unsupported sanitizer runtime is a blocked gate requiring a supported runner, not a pass or an excuse to suppress diagnostics.

- [ ] Run Doxygen with `LIBJPL_BUILD_DOCS=ON` in a job with Doxygen installed, then execute symbol and spaces-path checks. Run license/header checks and fixture integrity in all ordinary builds. Keep warnings zero; use `-Werror=implicit-function-declaration` for strict POSIX project code and preserve MSVC `/W4 /permissive-`.
- [ ] Add a separate `workflow_dispatch` ephemeris job using `LIBJPL_FETCH_EPHEMERIDES=ON` and a cache keyed by manifest hash and platform where needed. Verify the cache after restoration with production resolver checks. Run DE440/DE440t testpo in both configurations, retain summary counts/hashes as artifacts, and run the local legacy/long-span sweep only on a configured maintainer runner. Do not assume hosted CI owns the maintainer's local ephemerides or transmit them as artifacts.
- [ ] Write exact local Windows, WSL/Linux, macOS, 32-bit, sanitizer, installed-package, docs, offline-cache, and real-tier commands into `docs/validation.md`. Separate PowerShell commands from POSIX commands; preserve quoted paths and report prerequisites. State the latest observed result for each configuration and leave unrun ones explicitly pending.
- [ ] Validate YAML structure and CMake/CTest commands in available environments, then require actual CI run results before T23 marks platform findings closed. Do not infer a macOS pass from Windows compilation. Checkpoint: `ci: gate remediation across platforms and independent validation`.
