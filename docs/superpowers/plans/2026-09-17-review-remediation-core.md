# Core Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement safe binary reading, two-part epochs, consistent units, finite scientific results, and identity-safe ownership.

**Architecture:** Preserve the existing public modules and static library. Add one private date implementation and one shared binary-validation path, retaining storage-specific bounded reads. Change public date signatures and their callers together, then add numerical and ownership fixes with independent regressions.

**Tech Stack:** Dependency-free C17, CMake 3.20+, CTest, native FILE/mmap APIs, vendored ERFA 2.0.1.

**Spec:** [Approved design](../specs/2026-09-17-review-remediation-design.md); [master plan and interfaces](2026-09-17-review-remediation.md).

## Global Constraints

- All computational Julian epochs use actual arrays of two doubles. Both parts survive arithmetic and ERFA calls.
- All public AU-valued results and conversion helpers use the exact modern definition, `1 AU = 149597870.7 km`. Original header constants and raw records remain unchanged.
- Necessary API and ABI breaking changes are permitted. Replace scalar-date interfaces directly; do not add scalar compatibility wrappers.
- Keep C17, `C_EXTENSIONS OFF`, static `jpl`, three installed public headers, and offline default tests.
- License first-party code under MIT, with Todd Carnes as copyright holder. Do not edit vendored ERFA sources.
- Computed outputs are unchanged on failure; exact aliasing is supported where specified. Normal IEEE arithmetic, no fast-math.
- Follow the master's dependency graph, shared signatures, common Debug/Release cycle, and review checkpoints. No commits or implementation outside the authorized task; stop for missing workflow prerequisites.

---

## Test registration convention

New focused tests use the existing one-executable-per-test-file pattern. Register CTest names `jpl_dates`, `jpl_parser`, `jpl_largefile`, `jpl_api_errors`, and `jpl_numeric` with targets named by appending `_tests`. Add each target to the existing warnings/CRT-definition loop and apply `C_STANDARD 17`, `C_STANDARD_REQUIRED YES`, `C_EXTENSIONS NO`. Tests that include `jpl_private.h` get a PRIVATE `src` include directory only. Link `jpl`; never install private headers. Use `JPL_TEST_CHECK`, not removable `assert`.

## Task T01: Strict C17 and bounded large-file I/O

**Dependencies:** T00. **Coverage:** R03a/b, integer-offset portion of R01c.

**Files:** Modify `src/jpl.c:1-27,148-161,307-355`, `CMakeLists.txt`; create `tests/test_largefile.c`.

**Interfaces:** Consumes current public open/raw-read API. Produces correctly declared 64-bit file operations and checked physical offsets without public signature changes.

- [ ] Add a Linux compile regression using strict `-std=c17 -Werror=implicit-function-declaration`. Run `wsl -d Ubuntu --exec gcc -std=c17 -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration -I/mnt/d/src/libjpl/include -fsyntax-only /mnt/d/src/libjpl/src/jpl.c` on the baseline and record the `fseeko`/`ftello` failure. Other checkouts substitute their verified mounted absolute paths.
- [ ] Add a sparse-file test containing valid 4096-byte header/constants/first/last records, `786432` advertised one-day data intervals, and a last-record marker beyond signed 32-bit offset range. FILE opening and reading that last record must work when the size fits `size_t`. On 32-bit, a second sparse fixture larger than `SIZE_MAX` must fail cleanly with NULL handle; mmap need not map an unaddressable file. Check sparse creation support explicitly; do not allocate gigabytes of RAM.
- [ ] Put feature-test definitions before every include in `src/jpl.c`, so private headers cannot include libc first:

```c
#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif
```

- [ ] Enforce the supported offset representation with `_Static_assert(sizeof(jpl_file_offset) == sizeof(int64_t), "64-bit file offsets required");`. Check `(uintmax_t)offset > (uintmax_t)INT64_MAX` before seek; do not cast INT64_MAX down to size_t. Keep actual file-size conversion checked against SIZE_MAX. Reject unsupported offset representations at configuration/compilation, not by silent truncation.
- [ ] Repair checked offset ordering so neither addition nor multiplication wraps:

```c
if (record_doubles == 0 || record_doubles > SIZE_MAX / 8) return -1;
if (index > SIZE_MAX - 2) return -1;
size_t width = record_doubles * 8;
if (index + 2 > SIZE_MAX / width) return -1;
*out = (index + 2) * width;
```

- [ ] Set explicit strict C17 target properties and retain existing warnings. Build/run `jpl_largefile`; repeat the GCC syntax command and the master's common cycle. Run the sparse test under 32-bit POSIX during T22. Review the diff and record evidence. Checkpoint: `fix(io): declare and bound large-file operations`.

## Task T02: Private compensated date arithmetic

**Dependencies:** T00. **Coverage:** R00, G06/G12, B05; introduces status vocabulary for later tasks.

**Files:** Create `src/jpl_date.c`, `tests/test_dates.c`; modify `src/jpl_private.h`, `include/jpl.h` status enum, `src/jpl.c` status strings, `CMakeLists.txt`.

**Interfaces:** Produces all seven date helper signatures specified in the master. Include `jpl.h` in the private header so status and size types are declared. Add UNSUPPORTED, CONVERGENCE, NUMERIC statuses now; existing status values may be retained by appending entries.

- [ ] Write the exact low-part regression and register `jpl_dates`. Initially it fails to link; after adding signatures, confirm a naive scalar implementation fails the low-part assertion:

```c
double a[2] = {8000016.5, 0.0}, b[2], d[2];
const double tick = 0x1p-40;
JPL_TEST_CHECK(jpl_date_add_days(a, tick, b) == JPL_OK);
JPL_TEST_CHECK(b[0] == a[0] && b[1] == tick);
JPL_TEST_CHECK(jpl_date_subtract(b, a, d) == JPL_OK);
JPL_TEST_CHECK(d[0] == tick && d[1] == 0.0);
JPL_TEST_CHECK(jpl_date_compare(b, a) > 0);
```

- [ ] Implement normalization using a local error-free sum; validate both inputs and the sum before publishing outputs:

```c
static void two_sum(double a, double b, double *s, double *e) {
    double x = a + b;
    double bv = x - a;
    *s = x;
    *e = (a - (x - bv)) + (b - bv);
}
```

- [ ] Implement addition by normalizing both input pairs, two-summing their high parts and low parts separately, merging the error terms, and renormalizing. Use full TwoSum, not QuickTwoSum without its magnitude precondition. Check intermediate finiteness and leave output untouched on failure. Subtraction negates a normalized operand into locals and reuses addition; adding days uses `{days,0}`. Comparison lexicographically compares normalized high/low parts.

```c
/* All input pairs here are already normalized finite locals. */
two_sum(a[0], b[0], &s, &e);
two_sum(a[1], b[1], &t, &f);
two_sum(e, t, &u, &v);
two_sum(s, u, &h, &l);
two_sum(h, l + (v + f), &result[0], &result[1]);
```

- [ ] Implement scaled-duration addition using `p = (double)index * step` and `r = fma((double)index, step, -p)`, then add `{p,r}` as a pair. Check `(uintmax_t)index <= UINT64_C(9007199254740992)` and successful exact integer round-trip before using it; do not cast that bound down to 32-bit size_t. Apply the same bound to partition count. Implement partition by subtracting endpoints into a pair, dividing by count with compensated remainder `qlo = (fma(-qhi,n,span[0]) + span[1])/n`, multiplying the quotient pair by index with an FMA residual, then adding to begin. Copy endpoints for index 0/count. Reject count zero and invalid indices.
- [ ] Add explicit cases at `{-3100015.5,0}`, `{2451545.0,0}`, `{8000016.5,0}`; cancellation and reversed splits; normalized equivalent `{2400000.5,51544.5}` and `{2451545,0}`; signed zero; in-place operations; boundary partitions 1/3 and powers of two; NaN/Inf in either part; DBL_MAX arithmetic overflow; and unchanged sentinels. A partition comparison must use the pair, not `part[0]+part[1]`.
- [ ] Run `cmake --build build/remediation --config Debug --target jpl_dates_tests`, then `ctest --test-dir build/remediation -C Debug -R '^jpl_dates$' --output-on-failure`, followed by the common cycle. Checkpoint: `feat(date): add compensated two-part epoch helpers`.

## Task T04: Safe metadata, conversion bounds, and valid mocks

**Dependencies:** T01, T02. **Coverage:** R01a/b/c, R08a, G13.

**Files:** Modify `src/jpl.c` header/layout/interpolation guards, `tests/jpl_test_util.h`, existing mock-based tests, `CMakeLists.txt`; create `tests/test_parser.c`.

**Interfaces:** Produces `jpl_test_mock` and `test_write_mock` from the master, with the added `uint32_t constant_count` field. Public dates remain scalar until T07; helper/mocks use arrays now and serialize only at the byte-writing boundary.

- [ ] Introduce the configurable writer with checked fopen/fwrite/fclose operations, full caller-owned record buffers, and explicit endian encoding independent of host byte order. Reuse existing pointer and header offsets. Add a baseline write/open/read assertion before using it for mutations.

```c
unsigned char header[2880] = {0};
double constants[360] = {11.0,22.0};
double records[360] = {2451545.0,2451546.0,149597870.7};
test_put_ptr(header, 3, 3, 1, 1);
jpl_test_mock mock = {
    .record_doubles=360, .advertised_records=1, .denum=441,
    .constant_count=2, .start={2451545.0,0.0}, .end={2451546.0,0.0},
    .step_days=1.0, .au_km=149597870.7, .emrat=81.30056,
    .header=header, .constants=constants, .records=records
};
JPL_TEST_CHECK(test_write_mock(path, &mock) == 0);
```

For step mutations, change only `mock.step_days`; do not derive the end date from the mutated step or the old parser may reject NaN endpoints before the unsafe step conversion is exercised.
- [ ] Repair existing raw-read mocks: encode header-consistent first/last data dates instead of `0,1`, retain recognizable coefficient values in slots after the first two doubles, and update their exact raw-read assertions. In `test_tt_tdb.c`, correct the appended mock's zero/nonfinite padding only where the intended test requires valid dates, not by changing its coefficient-table shape.
- [ ] Add table-driven header mutations `step = NAN, INFINITY, 0, -1, 0x1p-1000`; AU/EMRAT `NAN, +/-INFINITY, 0, -1`; bad dimensions; and huge record-count ratios. Run both opens. Use this test idiom for every failed open:

```c
jpl_ephemeris *e = (jpl_ephemeris *)(uintptr_t)1;
JPL_TEST_CHECK(jpl_open(path, &e) == JPL_ERROR_FORMAT);
JPL_TEST_CHECK(e == NULL);
e = (jpl_ephemeris *)(uintptr_t)1;
JPL_TEST_CHECK(jpl_open_mmap(path, &e) == JPL_ERROR_FORMAT);
JPL_TEST_CHECK(e == NULL);
```

- [ ] Add finite-query/NaN-record-start and NaN-query tests against valid files. Before T07 these call the scalar public API; migrate them in T07. Test an interior corrupt record in a three-record file so subsequent open-time first/last checks do not eliminate lazy-read validation coverage.
- [ ] Require finite ordered dates, positive finite step/AU/EMRAT, and checked coefficient span arithmetic. Before converting the interval count, bound it by `file_size / minimum_record_bytes - 2` after guarding subtraction. This bound lies below the exact-double integer limit on supported sizes; verify that fact rather than converting SIZE_MAX to double. After rounding a count, check the count and coverage reconstruction before using it.

```c
if (!isfinite(step) || step <= 0.0 ||
    !isfinite(au) || au <= 0.0 ||
    !isfinite(emrat) || emrat <= 0.0) return JPL_ERROR_FORMAT;
if (!isfinite(jd)) return JPL_ERROR_ARGUMENT; /* Replaced by pair validation in T07. */
if (!isfinite(record_start) || !isfinite(record_end) ||
    record_end <= record_start) return JPL_ERROR_FORMAT;
```

- [ ] Also reject nonfinite/zero substep and nonfinite/negative subinterval quotient before conversion. Only refresh the existing FILE cache after validated successful use. Run `jpl_parser`, existing raw/mmap/TT-TDB tests, and the common cycle; use UBSan float-cast checks in T22. Checkpoint: `fix(parser): validate metadata before arithmetic and indexing`.

## Task T05: Complete layout and byte-order candidates

**Dependencies:** T04. **Coverage:** R06a/b, R09, remaining R01c.

**Files:** Modify `src/jpl.c` parse/open/mmap/layout helpers and `tests/test_parser.c`; extend `tests/test_largefile.c` only if its valid geometry needs explicit endpoint records.

**Interfaces:** Internal byte-source and validator are private to `src/jpl.c`:

```c
typedef jpl_status (*jpl_read_bytes_fn)(void *context, size_t offset,
                                       unsigned char *out, size_t count);
typedef struct jpl_byte_source {
    void *context;
    size_t size;
    jpl_read_bytes_fn read;
} jpl_byte_source;
static jpl_status validate_layout(const jpl_byte_source *source,
                                  jpl_header *header, size_t *record_count);
```

- [ ] Build a 360-double mock with two constants `11,22`, a one-coefficient Mars pointer `{3,1,1}`, one advertised interval, and one trailing record. Assert exactly 360 doubles, two physical data records, constants `11,22`, and usable first-interval states. The old code selects 480 doubles and reads a wrong constant.

```c
JPL_TEST_CHECK(jpl_get_header(e)->record_double_count == 360);
JPL_TEST_CHECK(jpl_record_count(e) == 2);
double constants[2];
JPL_TEST_CHECK(jpl_read_constants(e, constants, 2) == JPL_OK);
JPL_TEST_CHECK(constants[0] == 11.0 && constants[1] == 22.0);
```

- [ ] Add a header-only 2880-byte mutation that must fail; a genuine classic 2856-byte-record file that must succeed without reading extension bytes from its constants record; and the saved ambiguous-preliminary BE case with `[0,1]`, step 1, DENUM 102, two constants, `{3,1,1}`. Add a deliberately ambiguous multi-candidate file that must fail rather than select whichever loop runs first.
- [ ] Implement bounded FILE and mmap byte-source callbacks. Every read checks `offset <= size` and `count <= size-offset`; mmap memcpy and FILE seek/read share error classification. Initial header reads use the classic required size, not an unconditional extension-sized read. Parse into candidate-local headers, never the published header.
- [ ] Evaluate LE and BE candidates through metadata, count, record-width, first/last/trailing endpoints, and span checks. Derive geometry from classic spans, then preserve existing valid TT-TDB decoding within the resulting header record so existing positive TT-TDB tests stay green. Until T06, a positive optional span outside the physical record may retain the existing FORMAT rejection; T06 changes that legacy-padding policy and supplies its regressions. Require record width multiple of 8, >=2856, sufficient constants/overflow names, and checked classic spans. For scheduled endpoints use private two-part arithmetic even before the public migration; promote scalar header fields locally and remove that transitional promotion in T07.
- [ ] Compare reconstructed endpoints to encoded endpoints with a documented bound derived from their binary64 encoding ULPs; never use that allowance to broaden the public coverage test. Validate first/last advertised records and any trailing record at their candidate offsets. Check every interior record when interpolated. Collapse identical candidates, reject different surviving interpretations, and propagate actual I/O/allocation failures distinctly from invalid candidates.
- [ ] Run `ctest --test-dir build/remediation -C Debug -R 'jpl_parser|jpl_fixture|jpl_largefile|jpl_mmap' --output-on-failure` after building, then common cycle. Add checks that final advertised endpoint uses the last covered interval, not an optional trailing record, during T07. Checkpoint: `fix(parser): validate complete endian and record-layout candidates`.

## Task T06: Optional datasets after physical geometry

**Dependencies:** T05. **Coverage:** R10, G02.

**Files:** Modify `src/jpl.c` optional-tail parsing, `tests/test_parser.c`, `tests/test_tt_tdb.c`.

**Interfaces:** Consumes validated record width and byte order; publishes `has_tt_tdb` and pointer only after bounded tail validation. No filename/DE-number heuristic.

- [ ] Add tests where `{1000,1,1}` at the optional TT-TDB slot of a 360-double record must leave the file readable and `has_tt_tdb == 0`. Add zeros, partial triples, INT_MAX-exceeding words, overflowing products, span beyond the record, and tail bytes absent at the header-record boundary. Confirm old positive padding causes a failure.
- [ ] Add positive assertions for appended TT-TDB start 939, coefficient count 11, four subintervals, record width 984, and a header with >400 constants placing the tail after overflow names. Give each subinterval distinct constant/linear coefficients and test values/rates, not only presence.
- [ ] Remove optional-pointer contribution from minimum classic layout size. After the candidate width is established, locate `2856 + 6*max(nconst-400,0)` with checked arithmetic, require 24 tail bytes inside that header record, and evaluate the TT-TDB span:

```c
size_t end;
candidate.has_tt_tdb = 0;
if (tail_is_inside_header &&
    tt_tdb_span_end(&tentative, &end) == 0 &&
    end <= candidate.record_double_count) {
    candidate.tt_tdb_pointer = tentative;
    candidate.has_tt_tdb = 1;
}
```

- [ ] Do not let an irrelevant T#14 extension word invalidate a separately valid TT-TDB triple without format evidence. Document that in-record random bytes cannot be distinguished from genuine metadata by geometry alone. Invalid optional triples mean absent, while nonfinite coefficients of an accepted dataset are FORMAT on use in T09.
- [ ] Build and run `jpl_parser`, `jpl_tt_tdb`, `jpl_fixture` for both configurations and run common cycle. Checkpoint: `fix(parser): ground optional time tables in physical records`.

## Task T07: Atomic two-part public API and ERFA migration

**Dependencies:** T02, T03, T06. **Coverage:** R00, R01a, L03/L05, G06/G12, B05.

**Files:** Modify all three public headers, all three existing production `.c` files, `src/jpl_private.h`, all `tests/test_*.c` callers, `tests/jpl_test_util.h`, `README.md`, `CMakeLists.txt`; create `tests/test_api_errors.c`. Keep the master interface table open while editing.

**Interfaces:** Date APIs consume/produce arrays exactly as spec section 2.1. Frame/date-conversion functions return status. Header coverage arrays replace scalars. Geometric units remain file-AU until T08; do not mix that behavioral change into this task.

- [ ] Write compile-time caller examples for every date-taking function and low-part state tests using a linear mock near `8000016.5`. Initially the array calls fail against the scalar headers. Add interval-side tests whose high parts are identical but low parts have opposite signs.

```c
const double left[2] = {2451546.0, -0x1p-40};
const double right[2] = {2451546.0, 0x1p-40};
double a[6], b[6];
JPL_TEST_CHECK(jpl_get_state(e, left, JPL_MARS, 0, a) == JPL_OK);
JPL_TEST_CHECK(jpl_get_state(e, right, JPL_MARS, 0, b) == JPL_OK);
/* The fixture places distinct constants in the two adjacent intervals. */
JPL_TEST_CHECK(a[0] == 1.0 && b[0] == 2.0);
```

- [ ] Change public coverage fields and every date parameter together. Change the five frame/date API declarations to status returns, preserving argument order for state rotations. Add `#include "jpl.h"` in `jpl_frames.h`. Update Doxygen parameters/results surgically instead of replacing old comments wholesale.
- [ ] Promote decoded scalar header/record epochs only at their decoding boundary. Replace coverage tests with normalized pair comparisons. Remove the scalar query-finiteness check introduced by T04 and validate both parts through `jpl_date_normalize`. Keep scalar durations/rates and on-disk doubles as specified.
- [ ] Refactor index selection: derive a bounded candidate quotient from a compensated difference, reconstruct candidate boundaries with `add_scaled`/`partition`, compare the full date pair to them, and adjust an estimated neighbor before access. Use a bounded search or binary search over verified intervals if correction exceeds one neighbor; never loop unbounded on malformed data. At header end explicitly choose the last advertised interval. Compute local normalized Chebyshev argument only after compensated subtraction of the selected nearby boundary.
- [ ] Convert retarded dates in the existing solver with `jpl_date_add_days(receive,-lt,emission)` now; the bounded solver arrives in T10. Convert convenience TT input to local TDB pair and propagate conversion errors.
- [ ] Implement TT/TDB conversion without scalar epochs. TT-to-TDB evaluates `eraDtdb(tt[0],tt[1],0,0,0,0)/86400` and adds it with the date helper. TDB-to-TT starts at TDB, repeatedly computes the TT candidate plus its ERFA correction, subtracts the desired TDB into a pair, and corrects TT until residual <=`1e-15` day or 10 iterations:

```c
double predicted[2], residual[2], next[2];
jpl_status st = jpl_date_add_days(candidate, correction_days, predicted);
if (st != JPL_OK) return st;
st = jpl_date_subtract(predicted, input_tdb, residual);
if (st != JPL_OK) return st;
st = jpl_date_subtract(candidate, residual, next);
if (st != JPL_OK) return st;
```

- [ ] Pass pairs directly to `eraEcm06`; retain correct transpose direction. Normalize/check date and state inputs before computing frame output in locals. Return status without modifying outputs on errors. Preserve fixed-epoch velocity semantics.
- [ ] Migrate every test epoch, fixture/reference coverage comparison, first/last stored date, and example. T03 already produces pairs; runners may use their old parsing flow only until T15, but no new scalar public adapter is permitted. Replace text-JD parsing with the T03 helper as part of this migration where required to avoid precision loss. Keep formatting-only diagnostic conversion clearly separate from computation.
- [ ] Add `test_api_errors.c` cases for NULL dates, NaN/Inf in either part, invalid target/center, target zero, SSB dates outside coverage, and all output sentinels. Preserve missing-dataset status until T09 changes it, with explicit test updates there. Test final endpoint with/without a trailing record and arbitrary equivalent splits.
- [ ] Build all targets in Debug and Release before running tests; compile every README example. Run common cycle and search live sources for scalar full-JD signatures or `jd - lt`. Do not finish with one module still exposing scalar epochs. Checkpoint: `feat(api)!: preserve two-part Julian dates end to end`.

## Task T08: One AU definition and independently derived light speed

**Dependencies:** T07, T15. **Coverage:** L01/L02.

**Files:** Modify `src/jpl_private.h`, `src/jpl.c` state scaling, `src/jpl_convenience.c`, `include/jpl.h`, `include/jpl_convenience.h`, `tests/jpl_test_util.h`, `tests/test_lighttime.c`, `tests/test_aberration.c`, `tests/test_convenience.c`, both reference runners and their test helper.

**Interfaces:** Public AU is modern; header AU is unchanged metadata. Unit conversion functions now return status with unchanged output on failure. Reference helper's actual-value conversion is explicitly updated from pre-T08 file-AU behavior.

- [ ] Write a mock with file AU `150000000.0` km and a one-coefficient position `149597870.7` km. Assert public x==1, header AU==150000000, raw coefficient unchanged, and physical light-time equals `149597870.7/299792.458/86400` days. Before the fix the position assertion fails.
- [ ] Define physical constants through exact inputs, not the old rounded c literal:

```c
#define JPL_AU_KM 149597870.7
#define JPL_DAYSEC 86400.0
#define JPL_C_KM_PER_SECOND 299792.458
#define JPL_C_AU_PER_DAY (JPL_C_KM_PER_SECOND * JPL_DAYSEC / JPL_AU_KM)
```

- [ ] Replace every file-AU division in public body-state/light-time output with modern AU. Keep header/raw constants unchanged and still validate them. Check all observer velocities, Sun distances, and aberration beta values use the same units. Change conversion helpers to status returns and transactional local output; reject NULL/nonfinite arguments and unrepresentable arithmetic.
- [ ] Change body-only reference comparisons to reference units:

```c
double actual_reference = actual_modern_au *
    (149597870.7 / header->astronomical_unit_km);
/* Only body coordinates/rates use this conversion; never T#14/15/17. */
```

- [ ] Replace the duplicated incorrect test c constant with independent expected calculations using kilometre distance and SI light speed. Add a 40-AU delay regression with a tolerance below the old 1.125-microsecond error, modern/file-AU round-trip tests, and nonmodern-AU validator fixtures. Preserve SRS-term sensitivity tests.
- [ ] Run full default tests and the available legacy/full reference tier before closing unit semantics. If optional files are unavailable, record the gate as pending, not verified. Checkpoint: `fix(units)!: standardize modern AU and exact-derived light speed`.

## Task T09: Finite transactional state results and error categories

**Dependencies:** T07, T08. **Coverage:** R08b, R01a/b, L03, G13.

**Files:** Modify `src/jpl.c` interpolation/body/state/TT-TDB paths, `include/jpl.h`, `tests/test_api_errors.c`, `tests/test_parser.c`, `tests/test_tt_tdb.c`, reference-runner missing-data tests.

**Interfaces:** Missing datasets return UNSUPPORTED; nonfinite file data FORMAT; representability failure NUMERIC. Computed output arrays and optional scalar outputs are unchanged on all failures.

- [ ] Add sentinel checks for all public state/angular/time paths. Use an interior data record with a NaN coefficient and a finite-coefficient DBL_MAX-overflow case, then repeat for both backends and endiannesses:

```c
double out[6] = {11,22,33,44,55,66}, before[6];
memcpy(before, out, sizeof out);
JPL_TEST_CHECK(jpl_get_state(e, jd, JPL_MARS, 0, out) == JPL_ERROR_FORMAT);
JPL_TEST_CHECK(memcmp(out, before, sizeof out) == 0);
```

- [ ] Validate every coefficient actually used, every accumulated state/rate, reconstructed Earth/Moon state, and final relative/scaled state. Distinguish nonfinite encoded coefficients from overflow generated by finite inputs. Leave unused padding unchecked. Do not poison or publish cache contents after a failed interpolation.

```c
if (!isfinite(record[start + coefficient])) return JPL_ERROR_FORMAT;
if (!isfinite(sum) || !isfinite(derivative)) return JPL_ERROR_NUMERIC;
/* Store into local_result; copy to caller only after all components pass. */
```

- [ ] Route angular getters through local output too; they currently write directly to caller storage. Validate TT-TDB value/rate before either optional/required output is written. Test invalid coefficients in the second/third component to catch partial publication.
- [ ] Validate dates before zero SSB paths; for missing data return UNSUPPORTED only after valid pointer/body/date checks. Update all public return documentation and the now-hardened validators; in-coverage ordinary missing-body rows still fail, while justified absent angular rows may skip.
- [ ] Verify Earth and Moon formulas, rates, boundary records, invalid EMRAT/AU rejection, and unchanged outputs on I/O/allocation failures that can be injected deterministically. Do not overpromise arbitrary allocator-failure coverage; record which construction paths are exercised. Run focused `jpl_api_errors|jpl_parser|jpl_tt_tdb` and common cycle. Checkpoint: `fix(numerics): reject nonfinite results without partial output`.

## Task T10: Safeguarded light-time with independent residuals

**Dependencies:** T09. **Coverage:** R08c/R11, G05, B01.

**Files:** Modify `src/jpl.c` light-time solver, `src/jpl_private.h` iteration constants, `include/jpl.h`, `tests/test_lighttime.c`, `tests/test_api_errors.c`.

**Interfaces:** Same two-part public signature from T07. Produces consistent state/delay at one accepted emission epoch; RANGE, FORMAT, NUMERIC and CONVERGENCE remain distinct.

- [ ] Encode `x(t)=1+0.01*(t-S)` AU with S=2451545, SSB observer, and receive date `{2451545.0,0.0057756184}`. Compute analytic delay directly from the linear model:

```c
const double c = 299792.458 * 86400.0 / 149597870.7;
const double dt = 0.0057756184;
const double expected_tau = (1.0 + 0.01 * dt) / (c + 0.01);
JPL_TEST_CHECK(jpl_get_state_lighttime(e, receive, JPL_MARS, 0, state, &tau) == JPL_OK);
JPL_TEST_CHECK(fabs(tau - expected_tau) < 1e-12);
JPL_TEST_CHECK(fabs(tau - hypot(hypot(state[0],state[1]),state[2])/c) < 1e-12);
```

- [ ] Add a receive offset slightly below `1/c` with a genuinely pre-coverage root; require RANGE and unchanged outputs. Add moving observer, stationary target, SSB target/NaN center, identical bodies, superluminal evaluated file velocity, and coverage endpoints.
- [ ] Keep the observer barycentric state fixed at reception. Evaluate a delay by compensated subtraction from reception, validated target state, finite norm/c, and residual `tau - range/c`. Return the evaluated state only with that same accepted tau. Use up to 10 fast fixed-point iterations, and fall back rather than fail when an estimate crosses coverage or fails to settle.
- [ ] Bracket the fallback using two-part emission endpoints `[coverage_start, receive]`. Equivalently bracket a two-part delay `[0, receive-start]`; do not collapse the large upper delay and lose the endpoint residual. At receive the residual is nonpositive; at start a negative residual means no in-coverage root under the documented subluminal-motion contract. Use sign-checked bisection with a two-part midpoint, checking a finite residual <=`1e-12` day, for at most 128 iterations. A midpoint that cannot advance without satisfying the residual returns CONVERGENCE.

```c
/* residual corresponds to the state evaluated at this candidate emission. */
if (!isfinite(residual)) return JPL_ERROR_NUMERIC;
if (fabs(residual) <= JPL_LIGHTTIME_TOL_DAYS) {
    memcpy(state, candidate_state, sizeof candidate_state);
    if (out_lt) *out_lt = accepted_delay;
    return JPL_OK;
}
/* Update the sign bracket; never publish candidate_state here. */
```

- [ ] Keep a private solver iteration-budget parameter for deterministic test coverage of exhaustion, without adding public options. Implement `jpl_test_lighttime_with_budget` with the exact signature in the master and guard its declaration/definition with `LIBJPL_TESTING`. When tests are ON, define that macro privately for `jpl` and `jpl_lighttime_tests`; the public wrapper supplies 10/128. A zero-budget call on a non-root case must return CONVERGENCE with unchanged outputs. A tests-OFF build must not export this hook.
- [ ] Document the exact mixed-epoch velocity formula. Test velocity independently from the delay oracle, then run focused light-time/API tests and the common cycle. Checkpoint: `fix(lighttime): bracket valid emission epochs and verify convergence`.

## Task T11: Stable standalone math and complete status behavior

**Dependencies:** T08, T09. **Coverage:** S04, G13, B02; completes frame/aberration error contracts.

**Files:** Modify `src/jpl_private.h`, `src/jpl.c` aberration functions, `src/jpl_frames.c`, `src/jpl_convenience.c` status propagation, public math declarations, `tests/test_aberration.c`, `tests/test_frames.c`, `tests/test_convenience.c`, `tests/test_api_errors.c`.

**Interfaces:** Aberration functions now return `jpl_status`; frame/time status interfaces already exist. Finite nonpositive Sun distance omits the potential term; nonfinite Sun distance is ARGUMENT.

- [ ] Add positive-scale invariance tests for `1e-200`, 1, and `1e200`; compare normalized directions rather than absolute position error. Assert representable nonzero tiny output, finite large output, unchanged velocity slots, and exact permitted aliasing:

```c
double s[6] = {1e200,0,0,1,2,3}, v[3] = {0,0.01,0}, out[6];
JPL_TEST_CHECK(jpl_apply_aberration(s, v, out) == JPL_OK);
JPL_TEST_CHECK(isfinite(out[0]) && isfinite(out[1]));
JPL_TEST_CHECK(out[1] > 0.0);
JPL_TEST_CHECK(out[3] == 1 && out[4] == 2 && out[5] == 3);
```

- [ ] Replace `sqrt(x*x+y*y+z*z)` with scale-safe norm/normalization. For a representable norm, nested `hypot` is sufficient; form direction using scaled components and scale-aware restoration so `r/rn` does not overflow before a representable coordinate is multiplied. Validate the second normalization too. A genuinely unrepresentable required result returns NUMERIC.
- [ ] Validate all standalone state/velocity inputs before zero shortcuts; reject observer norm >=c, NaN/Inf in any slot, NULL required pointers, and nonfinite Sun distance. Copy unchanged velocity into local output only after checks. Keep finite nonpositive Sun distance as explicit omission, not a path that silently accepts NaN.
- [ ] Add status propagation at every convenience and wrapper call. Preserve the published aberration formula and solar-potential SRS sensitivity; no new light-deflection model. Frame operations check finite matrices/results and do not partially publish position before velocity succeeds.
- [ ] Preserve the existing absolute frame vector at JD `{2460000.5,0}` and fixed-epoch R*v tests, plus inverse, in-place, zero vector, null matrix, and very large finite arithmetic. Add conversion overflow/unchanged-output cases. Run focused math/API tests and common cycle. Checkpoint: `fix(math)!: validate and scale standalone vector operations`.

## Task T12: Frame metadata and convenience behavior

**Dependencies:** T10, T11. **Coverage:** R02/R04/R15, L05, B03.

**Files:** Modify `include/jpl.h`, `include/jpl_convenience.h`, `src/jpl.c` header classification, `src/jpl_convenience.c`, `tests/test_convenience.c`, `tests/test_parser.c`, `docs/validation.md` frame-source table.

**Interfaces:** Produces the four-value `jpl_reference_frame` enum and header field from the master. Unknown values remain readable by core.

- [ ] Add tests for center 0 versus 12 using both a no-Sun SSB-observer mock and the committed DE441 fixture. Add EMB/EMB with EMB coefficients but no Sun, invalid dates on that same shortcut, and valid dates with missing EMB data.
- [ ] Add DENUM cases 102, 200, 202, 441, 999: core states succeed in their native frame; convenience calls for legacy/unknown return UNSUPPORTED and preserve outputs. Declare DENUM 441 in modern synthetic convenience mocks instead of relying on zero.
- [ ] Implement the explicit classification table with JPL source citations. Use the approved documented allowlist, not a numeric threshold:

```c
switch (denum) {
case 102: return JPL_FRAME_DYNAMICAL_1950;
case 200: case 202: return JPL_FRAME_DYNAMICAL_2000;
case 403: case 405: case 406: case 410: case 413:
case 421: case 422: case 423: case 430: case 431:
case 432: case 440: case 441: return JPL_FRAME_ICRF;
default: return JPL_FRAME_UNKNOWN;
}
```

- [ ] In convenience calls validate arguments and TT finiteness, convert to TDB, validate TDB coverage, then require ICRF metadata. Normalize center 0 to 12 internally when a target code is needed. After successful required-body validation, return identical-body zero or SSB-observer light-time state before Sun work. Otherwise use reception-time observer velocity and Sun distance, propagate aberration status, and publish once.
- [ ] Clarify target-zero rejection, native versus ICRS frames, and exact correction scope in public Doxygen. Test frame eligibility even for identical-body calls, so a zero vector cannot accidentally imply support for a legacy frame. Run convenience/parser/fixture tests and common cycle. Checkpoint: `fix(convenience): enforce frame identity and valid zero shortcuts`.

## Task T13: Identity-safe FILE clones and lifetime contracts

**Dependencies:** T05, T07. **Coverage:** R07, G04/G13, B06.

**Files:** Modify `src/jpl.c` FILE open/clone/close paths, `include/jpl.h`, `tests/test_mmap.c`, `tests/test_api_errors.c`.

**Interfaces:** Same public clone signature. Clone has independent FILE/cache or shared mapping as before; FILE identity mismatches return IO with NULL output.

- [ ] Add a controlled cwd regression in a dedicated CTest working directory: create `a/probe.eph` with constant 11 and `b/probe.eph` with constant 99, open an absolute-root-derived relative path from a, move to b, clone, and require 11 from both handles. Restore cwd on every test exit; do not change the agent's shell directory. The baseline clone reads 99.
- [ ] Add path replacement and symlink-retargeting tests where the OS permits them. Require either the original identity or a clean IO failure, never replacement data. Distinguish a platform-prohibited rename from a library pass and retain portable cwd coverage everywhere.
- [ ] Resolve an absolute path before fopen and open that exact resolved path. Capture identity through the open stream: POSIX `fileno`/`fstat` device+inode; Windows `_fileno`/`_get_osfhandle` plus file identity API. Store original size and metadata used for consistency. Avoid POSIX `dup` with seek/read.
- [ ] Clone by opening an independent stream, retrieving its identity, comparing to the source's live identity and expected size/metadata, and publishing only after success:

```c
/* POSIX branch: c owns its independent FILE and private refcount already. */
struct stat original, reopened;
if (fstat(fileno(ephem->file), &original) != 0 ||
    fstat(fileno(c->file), &reopened) != 0 ||
    original.st_dev != reopened.st_dev || original.st_ino != reopened.st_ino ||
    original.st_size != reopened.st_size ||
    (uintmax_t)reopened.st_size != ephem->file_size) {
    jpl_close(c);
    return JPL_ERROR_IO;
}
/* Windows compares volume/file identity and size from the opened handles. */
```

- [ ] Test original-before-clone closure and multiple siblings for FILE and mmap, checking valid survivor headers and record reads. Add NULL-output/source tests and deterministic open-failure cleanup. State that in-place content mutation is unsupported; identity validation is not snapshot isolation.
- [ ] Run mmap/API tests under common cycle and ASan configuration in T22. Record Windows/POSIX replacement-test differences. Checkpoint: `fix(clone): preserve file identity and independent reader lifetimes`.
