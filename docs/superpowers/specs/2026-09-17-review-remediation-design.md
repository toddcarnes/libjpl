# Review remediation design

Status: the maintainer approved this written specification and authorized preparation of the task-by-task implementation plan. No library fixes are implemented by this document.

Baseline: `1327b20`, internal version 0.2.0. The maintainer confirms there has been no public release. Historical references to a public release do not impose compatibility obligations.

Source: [project review](../../reviews/2026-09-17-project-review.md). This specification supersedes conflicting recommendations in older design documents for the work covered here. Older documents remain historical records.

## 1. Approved decisions and scope

- Necessary API and ABI breaking changes are permitted. Replace scalar-date interfaces directly; do not add scalar compatibility wrappers.
- All computational Julian epochs use actual arrays of two doubles. Both parts survive arithmetic and ERFA calls.
- All public AU-valued results and conversion helpers use the exact modern definition, `1 AU = 149597870.7 km`. Original header constants and raw records remain unchanged.
- Core readers retain native-frame access to legacy ephemerides. Expose frame identity; ICRS convenience functions reject legacy or unverified frames rather than applying unvalidated historical transformations.
- License first-party code under MIT, with Todd Carnes as copyright holder. Include source-file notices and expected documentation/distribution notices. Preserve third-party licensing and do not edit vendored ERFA sources.
- Implement in dependency-ordered, independently tested increments. Keep every finding, subsidiary defect, test gap, and minor documentation observation in the coverage ledger below.

Retain a dependency-free C17 static library, three installed public headers, private vendored ERFA, offline default tests, and explicitly opt-in large-data validation. Development-only validation tools do not become runtime or ordinary-build dependencies.

This effort does not add T#16, surface observatories, gravitational light deflection, Earth-orientation processing, atmospheric refraction, apparent-position derivatives, or historical-frame transformations. Documenting and testing existing scope boundaries is part of the work; expanding those capabilities is not necessary to resolve the review.

## 2. Public API and errors

### 2.1 Date and result signatures

Use `const double jd[2]` for date inputs and `double jd[2]` for date outputs. Do not replace the required array representation with a scalar, struct, or opaque heap object. Public coverage fields become `double start_jd[2]` and `double end_jd[2]`.

The API migration includes every state getter, the TT-TDB getter, light-time getter, frame matrix/state transformations, TT/TDB conversions, and planet/moon convenience functions. Representative signatures are:

```c
jpl_status jpl_get_state(const jpl_ephemeris *ephemeris,
                         const double jd_tdb[2], int target, int center,
                         double state[6]);
jpl_status jpl_tt_to_tdb(const double jd_tt[2], double jd_tdb[2]);
jpl_status jpl_tdb_to_tt(const double jd_tdb[2], double jd_tt[2]);
jpl_status jpl_frame_rotation_matrix(const double jd_tt[2], double rotation[3][3]);
```

Frame, aberration, and unit-conversion functions that currently cannot report errors return `jpl_status` while retaining their output-buffer shape. `jpl_frames.h` includes `jpl.h` for the shared status declaration, without exposing ERFA. No new installed header is needed.

Computed outputs are transactional: calculate in local storage, check success and finiteness, then copy results. On failure, all supplied computed-result outputs, including optional light-time/rate outputs, remain unchanged. Support exact input/output aliasing for date conversions, state rotations, aberration, and vector-unit conversion. Do not promise arbitrary partial overlap or overlap between distinct output buffers.

This transactional guarantee does not turn raw record/constant I/O into a copy-on-success service. Document that raw destination buffers may be partially written after I/O failure. Open/clone functions set a supplied handle output to NULL on failure. `jpl_close(NULL)` remains a no-op.

### 2.2 Error categories

Retain existing status names and add the following distinctions to the public enum and `jpl_status_string`:

| Status | Meaning |
| --- | --- |
| `JPL_ERROR_ARGUMENT` | NULL required pointer, invalid body combination, nonfinite user input, or physically invalid standalone aberration velocity |
| `JPL_ERROR_RANGE` | Finite observation/emission date outside coverage, raw record index outside bounds, or no physically supported emission solution within coverage |
| `JPL_ERROR_FORMAT` | Malformed encoded header/record, invalid file physical constants, inconsistent layout, or nonfinite file coefficients |
| `JPL_ERROR_UNSUPPORTED` | Requested dataset absent, or convenience output frame not verified as ICRF-aligned |
| `JPL_ERROR_CONVERGENCE` | A valid bounded solve fails its stopping criteria within the iteration limit |
| `JPL_ERROR_NUMERIC` | Finite validated inputs nevertheless produce an unrepresentable numerical result |
| `JPL_ERROR_IO` | Filesystem/read/mapping failure or inability to reopen the original file identity for a clone |
| `JPL_ERROR_MEMORY` | Allocation failure |

Validate required pointers, body codes, and input finiteness before date coverage, dataset checks, and zero-result shortcuts. For convenience functions, check frame eligibility after argument/observation-date validation and before producing any result. A valid in-coverage query for missing data reports UNSUPPORTED, not RANGE. Tests and reference validators must migrate with these semantics.

### 2.3 Body and output conventions

Preserve PLEPH numbering: Earth 3, SSB 12, EMB 13, nutations 14, librations 15, and TT-TDB 17. File slot 2 remains EMB and is not body code 3. Zero remains a center alias only; target zero is consistently rejected in state/light-time APIs and documentation.

Geometric and light-time body outputs are AU and AU/day in the native file frame. Nutations/librations remain radians and radians/day. TT-TDB remains seconds and seconds/day. The dedicated TT-TDB getter has no implicit ERFA fallback; presence is content-derived, never filename-derived.

Validate SSB-to-SSB dates normally. For identical non-SSB bodies, validate the required body data at reception, then return zero without requiring unrelated Sun data. In convenience calls, centers 0 and 12 behave identically and need no aberration/Sun lookup because their barycentric observer velocity is zero. Existing convenience target restrictions remain explicit.

## 3. Two-part dates and time conversion

### 3.1 Representation

The represented epoch is the mathematical sum of two finite binary64 values in the stated time scale. Support arbitrary finite input splits whose sum is representable in the internal two-part format, including reference-epoch/offset and integer-day/fraction splits. Reject overflow in normalization rather than proceeding with infinity.

Normalize internally and on date output to a nonoverlapping high-part/residual pair using error-free summation primitives. The high part may be a large Julian date; the small residual must be retained. Normalization is not permission to discard that residual after computing the high part. Signed zero is canonicalized consistently.

Do not claim that different decimal input strings or differently rounded pairs necessarily represent the same epoch. The equivalence requirement applies when the pairs represent the same value, with precision-aware comparisons for inexact construction paths.

### 3.2 Internal arithmetic

Keep a small private collection of shared date helpers for normalization, comparison, subtracting epochs, and adding/subtracting durations. They belong with private numerical helpers, not in vendored ERFA or a new public general-purpose arithmetic subsystem.

Carry a two-part difference when it affects interval selection or a boundary comparison. A scalar local interpolation argument or duration may be formed after subtracting nearby epochs with compensated arithmetic. Do not first round the full epoch or a large elapsed interval and then use it as the sole authority for indexing.

A quotient may estimate a record/subinterval index, but verify it against two-part interval endpoints and correct the candidate before use. Interior boundaries select the following interval. The final advertised coverage endpoint uses the last covered interval; a permitted trailing physical record does not extend query coverage or change this endpoint policy.

Avoid compiler modes that reassociate away compensation or assume NaNs cannot exist. The supported build uses normal IEEE binary64 semantics and no fast-math mode. Check precision on MSVC, where `long double` is not an independent higher-precision oracle.

### 3.3 File and reference boundaries

Do not change the DE format, record width, or serialized date fields. Decode scalar file dates as `{encoded_value, 0.0}` and retain both parts in computational storage. Raw-record APIs still expose the encoded scalar fields.

Migrate mock builders and reference readers so computational epochs are arrays. Scalar full-epoch values are permitted only at the serialization boundary; individual date parts, durations, and other ordinary numerical values remain doubles. Parse reference Julian-date text into a whole-day part and a fractional part with checked conversion, rather than parsing the complete decimal epoch into a double and splitting afterward. Fixed-format JPL dates do not justify introducing a general date-string API.

### 3.4 ERFA and TT/TDB

Pass both parts to ERFA. TT-to-TDB adds ERFA's geocentric correction through the compensated duration helper. TDB-to-TT solves the inverse correction using bounded iteration on two-part epochs, with a finite residual check and a maximum iteration count. Use a time-conversion residual target of `1e-15` day and at most 10 correction iterations; inability to meet it is a convergence error, not a successful rounded identity.

The solver tolerance is not an accuracy claim for the physical model. Documentation distinguishes the roughly 1.7 ms signal amplitude, ERFA's stated validation interval and model accuracy, omitted topocentric terms, ephemeris interpolation error, and arithmetic error. Do not describe ephemeris-interpolated TT-TDB as mathematically exact. Extrapolated frame/time results outside independently validated model intervals carry no new high-accuracy promise.

### 3.5 Precision acceptance

Test J2000 and epochs near both ends of DE441's long span, including negative Julian dates. Include exactly representable small duration steps such as `2^-40` day, about 79 ns, and independently constructed equivalent splits. Assert that changes lost by scalar-JD addition remain observable in date outputs and suitably scaled linear mock states.

Test either side of coverage, record, and subinterval boundaries, including differences carried only by the low part; endpoint selection; normalization carry/cancellation; NaN and both infinities in either part; NULL pointers; and in-place conversions. Use exact binary/rational constructions and pinned independent high-precision references, not only calls to production helpers or round-trip assertions.

## 4. Portable I/O and validated layouts

### 4.1 Platform boundary

Expose the required POSIX declarations before system headers and enable 64-bit file offsets on applicable 32-bit POSIX platforms. Check against the actual seek/offset type and `size_t` limits. Reject unsupported sizes cleanly; 64-bit offsets do not make files larger than `SIZE_MAX` addressable on a 32-bit build.

Retain Windows native large-file operations and POSIX FILE/mmap implementations. Match read offsets, file sizes, and mapping lengths through checked arithmetic. Include strict C17 compilation and sparse-file large-offset tests in the platform gates.

### 4.2 Candidate validation

Use one shared validation policy for FILE and both platform mmap paths. Storage-specific operations supply bounded byte reads; they do not carry different header-validation rules. Keep refactoring limited to the reusable parsing/layout logic required for consistent behavior.

For each byte order, evaluate complete layout candidates before accepting that order:

1. Read the complete classic header and validate finite ordered dates, finite positive step, finite positive AU and Earth-Moon ratio, integer fields, and checked coefficient spans.
2. Bound the possible record count using file length and the minimum legal header-record width before any float-to-integer conversion. Never rely on a rounded cast of `SIZE_MAX` as the sole guard.
3. Consider the advertised interval count plus header/constants records, with the existing allowance for one physical trailing data record.
4. Require aligned record widths large enough for the classic 2,856-byte header, required overflow constant names, the numeric constants record, and all accepted coefficient spans. Do not require an invented 2,880-byte minimum for genuine classic records.
5. Validate first and last covered data-record bounds and any trailing-record bounds against the candidate width, step, and advertised coverage. Use an explicitly bounded allowance only for rounding already present in encoded file dates, never a tolerance that extends public query coverage.
6. Read optional extension words only inside the candidate header record. Optional bytes must not be read from the constants or data record merely because they are inside the file mapping.
7. Accept a unique validated interpretation. Reject materially different surviving layouts as FORMAT rather than choosing the first divisible width. Allocation and I/O failures remain their own errors.

Validate intermediate records when they are used, including finite ordered endpoints, consistency with their scheduled interval, and finite positive subinterval span. Opening is not a full scan of multi-gigabyte data. Every used coefficient and computed result receives the checks described in section 5.

The layout must preserve both byte orders, overflow-name headers, numeric constant counts above 400, the existing public cap on exposed constant names, missing optional angular/time datasets, valid appended TT-TDB coefficients, and the trailing-record allowance.

### 4.3 Optional-pointer policy

Classic mandatory structure remains strict. For unused classic datasets, accept the absent encodings established by actual supported JPL files; do not turn an absent dataset into a present partial span.

The extension tail is unversioned legacy padding in some files. Keep TT-TDB pointers tentative until record geometry is known. A missing tail, absent triple, unrepresentable triple, arithmetic overflow, or span outside the physical record means TT-TDB is absent, without enlarging the inferred record width or rejecting an otherwise valid classic file. A fully positive, checked, in-record TT-TDB span is present, including when it follows the last classic span. Its coefficients and results must then be valid when queried.

This policy cannot distinguish arbitrary legacy bytes that happen to form a valid in-record optional triple from genuine metadata without additional format evidence. Do not claim that it can, and do not substitute filename or DENUM-suffix heuristics. Tests pin the intended conservative geometry policy.

### 4.4 Parser regressions

Exercise FILE/mmap and both byte orders wherever applicable. Cover the saved header-only and trailing-width examples, the ambiguous big-endian header, legacy `{1000,1,1}` TT-TDB padding, valid appended TT-TDB, overflow names, truncation, nonfinite dates/steps/constants, invalid dimensions, offset overflow, and subinterval underflow.

Repair existing raw-read mocks whose data-record dates conflict with their headers. Preserve their useful byte-decoding assertions using valid dates and recognizable coefficient slots. Do not weaken production validation to accommodate invalid happy-path mocks.

## 5. Numerical and scientific behavior

### 5.1 Units and finite results

Convert stored kilometre body coefficients to modern AU, not the file's historical AU. Preserve `header.astronomical_unit_km` as original metadata and state clearly that it no longer selects the public output unit. Keep angles and TT-TDB units unchanged.

Derive light speed from exact `299792.458 km/s`, `86400 seconds/day`, and the modern AU. Keep shared constants private and eliminate duplicated incorrect test literals. Tests independently calculate physical reference values from kilometres and SI light speed.

When comparing historical testpo body coordinates, convert expected file-AU values to modern AU with `file_au / modern_au`, or convert the actual values back into reference units for comparison. The latter keeps the reference tolerance expressed in its original units. Never rescale angular or time-correction rows. Use a deliberately nonmodern mock AU to make this distinction measurable.

Validate file coefficients used by each interpolation before accumulation. Reject NaN/Inf coefficients as FORMAT. Check interpolation, Earth/Moon reconstruction, relative subtraction, unit conversion, rates, and correction results for finiteness. Arithmetic overflow from otherwise finite values reports NUMERIC, not success. Unused padding need not be finite.

### 5.2 Light-time solution

Hold the observer's position and velocity at reception. Solve for delay `tau` with emission epoch `t_receive - tau` represented as a pair and equation `tau = distance(target(emission), observer(reception)) / c`.

Retain fast fixed-point iteration for ordinary cases. If the first estimate leaves coverage or ordinary iteration fails to converge, use a safeguarded bracketed solve over emission times inside coverage. Evaluate endpoints to establish a valid bracket; return RANGE if there is no in-coverage physical solution. Keep the target's subluminal-motion assumption explicit and reject detected nonphysical/superluminal file states as FORMAT rather than using them to claim uniqueness.

Use finite residuals, a `1e-12` day delay tolerance, and an explicit convergence flag. Allow up to 128 safeguarded iterations, enough to bisect the supported long-span interval to that tolerance. Do not silently accept iteration exhaustion, NaNs, or an out-of-coverage emission time. Return state and delay from the same accepted emission evaluation; check the final equation residual.

Reproduce the saved receding-target boundary case and a nearby genuine pre-coverage case. Include stationary and moving observers/targets, zero range, explicit non-convergence, nonfinite coefficients, coverage endpoints, and independent analytic solutions. Report solver tolerance separately from astronomical model accuracy.

Velocity remains `v_target(emission) - v_center(reception)`. It is not the derivative of apparent position with respect to reception time. Aberration continues to leave velocity unchanged.

### 5.3 Aberration and frame math

Use scale-safe norms and normalization instead of unscaled sums of squares. Preserve tiny representable directions and avoid overflow when a norm/result remains representable. Test positions near `1e-200` and `1e200`, not only realistic AU-scale inputs; reject genuinely unrepresentable output as NUMERIC.

Validate standalone state/observer inputs, including velocity slots that are passed through. Reject observer speeds at or above light speed as ARGUMENT. A finite zero position returns a zero position with unchanged velocity after input validation. Finite nonpositive Sun distance requests omission of the potential term, preserving the wrapper convention; nonfinite Sun distance is invalid. Maintain in-place behavior and finite outputs.

Frame helpers retain `R*position` and `R*velocity` at a fixed epoch; there is no `dR/dt*position` term. Keep the current correct ERFA rotation direction and test absolute vectors, orthonormality, inverse behavior, and aliasing. A finite computed result outside a model's validated epoch interval is not proof of its physical accuracy there.

### 5.4 Frame identity and convenience API

Expose a documented `jpl_reference_frame` enum through a `reference_frame` field in `jpl_header`: unknown, dynamical equator/equinox 1950, dynamical equator/equinox 2000, and ICRF-aligned. This identifies orientation families, not a new transformation engine or a claim of identical ICRF realizations.

Classify DE102 as dynamical 1950 and DE200/202 as dynamical 2000. Use an explicit cited allowlist for ICRF-aligned releases rather than `DENUM >= threshold`. Initial documented candidates are DE403, 405, 406, 410, 413, 421, 422, 423, 430, 431, 432, 440, and 441 from JPL's export information. An entry is admitted only with its source citation in the implementation record. Other versions remain unknown until supported by evidence; this does not prevent core native-frame reads. Time-segmented variants use the same content-derived DENUM/frame classification.

All ICRS convenience calls require ICRF-aligned metadata, even identical-body calls. Unsupported frames fail explicitly. After successful argument/date/frame validation, zero-observer and identical-body paths avoid unnecessary Sun lookups. Update synthetic convenience fixtures to declare their intended frame identity rather than relying on DENUM zero.

Document light-time plus stellar aberration as the exact meaning of apparent here. The solar potential term in `_full` is not gravitational light bending. Keep the warning that adding apparent Moon/Earth legs mixes epochs and corrections.

## 6. Clone identity, ownership, and concurrency

Resolve an absolute reopen path before the original FILE open and use that resolved path for the open itself. Capture file identity from the resulting open stream, not merely from a path lookup. POSIX identity uses device/inode; Windows uses volume/file identity from the underlying handle. A FILE clone opens an independent stream at the stored path and verifies its identity against the still-open original before publishing the clone. Verify relevant size/metadata consistency too. Reopening the wrong file or being unable to reopen the original is an IO error with no clone output.

This deliberately permits safe failure after rename/deletion; clones are not promised to survive path disappearance. It fixes directory changes while preventing silent replacement. A POSIX `dup` plus seek/read is not an independent-position solution and must not be substituted.

State explicitly that ephemeris contents must remain immutable while any reader is active. Identity checks do not promise snapshot isolation against in-place edits. Retain the mmap truncation warning. Mmap clones share mapping ownership; FILE clones own independent streams and caches. Source-handle lifetime must be synchronized during cloning, and no handle may be closed while a reader is using it.

Add actual concurrent different-record reads on one mmap handle and on separate FILE clones. Test original-before-clone close order, sibling close order, surviving header/data access, and synchronized clone/close operations while another owned reference keeps the mapping alive. Do not treat closing the same live reader handle concurrently as supported behavior.

Use native Windows threads and POSIX threads in tests with `Threads::Threads` where needed, avoiding a new library runtime dependency. Threads have bounded loops, deterministic expected values, checked join results, and no sleeps used as correctness synchronization. Run race detection on a supported POSIX configuration and memory/undefined-behavior sanitizers separately.

## 7. Trustworthy tests and provenance

### 7.1 Reference readers and negative tests

Share the testpo row parser and comparison policy between fixture and full-file validators in a small test-only helper. Keep fixture-specific strictness separate from full-file headers and legitimate coverage skips. This is justified reuse: their current copies already disagree on six-token rows and nutation rates.

Accept the documented seven-token, six-token, and five-token forms. In the six-token form, DENUM is token zero and the fused calendar-date/JD field is token one. Parse and verify DENUM in all forms, including time-segmented labels when present in authoritative files. Use checked integer and floating conversions, reject nonfinite expected values and dates before dispatch, and check conversion overflow/underflow where it would alter the reference value.

Recognize the authoritative header/EOT structure explicitly. Within the data section, malformed rows, bad labels, unknown field combinations, overlong/truncated lines, numeric overflow, and input-read errors fail with line-number diagnostics instead of silently reducing the comparison count. The committed slice is a data-only mode with no tolerated nonblank headers. Unknown target codes are not silently skipped; explicitly supported-but-out-of-scope reference targets such as T#16 have counted, documented exclusions.

Check coverage before querying. In-coverage planetary errors always fail. Missing nutation/libration rows are skipped only when header metadata establishes absence; failure with a present dataset is an error. A T#17 row expects a present TT-TDB dataset and valid center/component conventions. Validate nutation X#3/4 through state[3]/state[4]; X#5/6 are invalid.

Ephemeris directory sweeps skip only genuine absence, not permission errors, truncation, or every IO status. Check file existence/error classification independently and retain the requirement that at least one file succeeds.

Add executable-level negative tests: the saved 100-NaN TT-TDB rows, unexpected in-coverage failures, a missing ordinary body with many other passing rows, corrupt present files beside valid files, wrong DENUM, malformed fused labels, invalid components, and deliberately perturbed references must make the appropriate runner exit nonzero. Build the runner needed for these small synthetic tests without enabling downloads.

### 7.2 Tolerances

Use one documented comparison policy in reference units. For ordinary body components and nutation/libration angles, start with an absolute `5e-13` envelope. Libration angle psi, T#15 X#3, gets only the JPL epoch factor `1 + 100*abs(JD - 2440400.5)/365.25`, not an additional factor based on the accumulated angle magnitude. Compute the epoch offset through the date helpers.

Retain the separately documented libration-rate allowance of `5e-12` as an absolute envelope, and the TT-TDB value/rate envelope of `1e-12` in the corresponding seconds units. Check these policies against pinned JPL testeph sources and the full supported reference tier. A failure triggers numerical investigation, not automatic widening. Any necessary per-component exception requires a recorded independent error analysis and a regression that fails beyond the justified bound.

No blanket tolerance relaxation may compensate for the new AU convention; convert units explicitly. Include a far-epoch psi perturbation that the old compounded tolerance accepted and the corrected policy rejects.

### 7.3 Numerical test gaps

- Assert TT-TDB midpoint value `0.001` seconds and derivative `0.00004` seconds/day in the existing linear mock, plus distinct subinterval values, endpoints, and valid appended-data layouts.
- Verify higher-order Chebyshev values/derivatives independently at record/subinterval boundaries and neighboring two-part dates. Retain Earth/EMB/Moon mass-ratio, byte-order, and frame-direction regressions.
- Add analytic moving-target/moving-observer light-time cases and absolute TT/TDB reference values with both signs of correction. A round trip or recomposition of production calls is not the sole oracle.
- Pin independent apparent-position and convenience-frame reference vectors with matching ephemeris, epoch, observer, frame, units, and correction conventions. Include a reproducible offline generation procedure and tool versions. A generic Horizons/SPICE output with different correction conventions is not an acceptable unexplained oracle.
- Test all new status categories, NULL/invalid inputs, unchanged computed outputs after failures, exact permitted aliasing, and cleanup after failed opens/clones. Keep explicit checks active in Release.

### 7.4 Fixture provenance

Record SHA-256 of the committed binary and of a canonical LF form of the reference text in the manifest. Canonicalization changes CRLF to LF only; it does not discard arbitrary whitespace or alter numerical tokens. Verify through build/test tooling without adding a runtime crypto dependency.

Preserve the committed reference-row identity sequence or an independently checked set of date/body/center/component identities, not merely count and endpoint dates. Duplicate/replaced interior rows must fail even when the row count stays 121.

Document source URLs, exact source spans and hashes, slicing/filtering commands, and the identity/version of `sub_eph` or a reproducible replacement. A hash pins the present artifact but does not prove its provenance. If original source/tool evidence cannot be recovered, record that verification as blocked and re-establish provenance from authoritative source material before closing the finding; do not invent metadata or label it verified.

The original fixture is not silently regenerated to make numerical tests pass. Any data replacement requires explicit review of provenance and reference differences. Third-party test data retain their own origin/terms and are not marked as Todd Carnes MIT source.

## 8. Licensing and distribution

### 8.1 MIT notices

Add the complete MIT text to root `LICENSE`, attributed to Todd Carnes. Use `Copyright (c) 2026 Todd Carnes` for newly authored project files and the repository's verified first-party creation years for older files; do not invent earlier years. Preserve legitimate existing contributor attribution when encountered.

First-party C sources, public/internal headers, tests, CMake modules, and scripts carry a short copyright notice and `SPDX-License-Identifier: MIT` in their native comment syntax, pointing to the root license where helpful. Add equivalent notices to documentation/configuration files in a non-disruptive form where appropriate. No license comments are inserted into binary fixtures or authoritative reference data.

Maintain an explicit ownership/exemption inventory for vendored ERFA, external data, generated artifacts, and formats that cannot carry comments. Do not prepend MIT headers to third-party files or modify `src/erfa`. Files introduced during implementation inherit the same policy.

README and generated documentation identify the project license and third-party terms. Install the project MIT license and the complete ERFA license under a predictable package data location such as `${CMAKE_INSTALL_DATADIR}/licenses/libjpl/`, regardless of whether documentation generation is enabled. Generated/public-header distributions retain their first-party notices.

Add a portable repository check for missing first-party headers and an install-content test for both full license texts. The exemption list is reviewed, not a blanket exclusion for tests or build tooling.

### 8.2 CMake package

Use `GNUInstallDirs` before constructing installed include paths. Install a relocatable `libjplConfig.cmake`, targets export, and version file using CMake package helpers. The `jpl::jpl` target points to the same include/lib locations used by install commands. Do not embed source/build checkout paths in installed interfaces.

Select math linkage for the actual toolchain at configure time, including clang-cl's MSVC-compatible frontend. Export the resulting requirement, not a compiler-ID expression evaluated in the consumer's context. Test both C and C++ inclusion/linkage while retaining a C17 implementation and private ERFA headers.

Use exact-version package compatibility while the project is pre-release; do not claim arbitrary minor/major ABI compatibility. The remediation itself does not publish a release or choose a new public version. Update Unreleased migration notes and let a separate authorized release step choose the version. Existing 0.2.0 metadata is an internal baseline, not evidence of a public release.

Run separate installed consumers with default paths, nondefault include/lib destinations, relocation, and prefixes containing spaces. Inspect exported interfaces and installed notices, not just whether the archive exists. Preserve optional PIC behavior for static-library embedding.

### 8.3 Fetch cache

Keep `LIBJPL_FETCH_EPHEMERIDES=OFF` completely offline. With ON, prefer explicitly configured local sources, then already-valid downloaded files, then download missing files. Environment variables redirect sources and do not independently activate the tier.

Validate downloaded-cache sizes and available hashes using the manifest as the only metadata source. Pin authoritative downloaded-binary hashes when their exact artifacts can be verified; until then, describe size-only checks honestly and retain content/reference validation. Do not apply downloaded-span hashes/sizes to maintainer-local files with different legitimate spans.

Download to a temporary destination and publish only after validation. A corrupt cached destination is diagnosed and replaced only after a successful verified fetch; an offline attempt fails clearly instead of reusing bad data or destroying a previous file before a replacement exists. Never delete maintainer-local sources. A valid populated cache must permit an offline second configure with the fetch option ON.

Test cache reuse, partial/corrupt downloads, source redirects, failures, and reconfiguration without transferring large files in the default suite. Use small controlled artifacts with the same resolver code. The opt-in real tier establishes the actual JPL download behavior.

## 9. Documentation and minor corrections

Add `@file` documentation to all three public headers and maintain full declaration documentation. Generate Doxygen XML as well as HTML so a check can compare the expected public symbol inventory with documented symbols. A warning-free build alone is insufficient. Keep license comments separate from API documentation and keep ERFA private.

Quote configured Doxygen input/output paths and use correctly quoted/VERBATIM build commands. Verify source/build paths containing spaces, not just an installation prefix with spaces.

Correct README's claim that no data ship: a small validation fixture ships, not full operational ephemerides. Correct TT/TDB versus TT/UTC wording and remove the false 2 ms accuracy floor from live headers and test comments. Explain the modern AU change, two-part-date construction, error behavior, frame metadata, and migration of time-conversion returns.

Update AGENTS.md and README test guidance to actual registered suites, the fetch-option gate, and current names. Avoid replacing the stale hard-coded six-test count with another count that immediately drifts as tests are added; enumerate tiers or derive/check the number when a count is stated. Preserve existing user instructions and the saved review.

Document mixed-epoch light-time velocity, fixed-epoch frame velocity, apparent-leg addition limits, frame/time model validity, content immutability, and synchronization/lifetime rules. Correct the fixture's byte-verbatim provenance claim to match what its checks establish. Remove stale internal task-history comments when they misstate current behavior, without rewriting unrelated historical specs/plans.

Add a migration guide and an Unreleased changelog entry. README examples must compile against all changed signatures and check status before using output. Do not accidentally teach users to calculate a scalar date first and split it afterward.

## 10. Dependency ordering and acceptance gates

The implementation plan expands these phases into small tasks; phases are not single commits or permission to skip intermediate verification.

| Phase | Work and dependencies | Gate |
| --- | --- | --- |
| P1 | Issue ledger, baseline results, trustworthy validators and independent test expectations | Negative validator tests fail for deliberate defects; existing useful tests remain active |
| P2 | Strict portability, bounded I/O, metadata checks, layout/byte-order/extension validation | FILE/mmap and both-byte-order parser regressions pass; strict POSIX execution works |
| P3 | Private compensated date helpers, atomic public/caller date migration, modern AU convention | No computational scalar-JD API remains; distant-epoch precision and unit tests pass |
| P4 | Finite results, bounded light-time, stable aberration, body shortcuts, frame policy | Analytic and independent reference cases pass; failures are classified and transactional |
| P5 | Clone identity, surviving lifetimes, true concurrent access | Replacement/cwd tests and deterministic threaded tests pass; sanitizer evidence recorded |
| P6 | MIT/third-party notices, package config/paths/linking, cache, documentation and migration notes | Installed consumers, license checks, offline-cache and Doxygen coverage tests pass |
| P7 | Cross-platform acceptance, opt-in real references, issue-by-issue resolution evidence | Every coverage-ledger row has implementation/disposition and verification evidence |

Portability work may precede Linux execution of P1 tests. Date helpers may be introduced early as a prerequisite, but the public date migration is one coordinated buildable change across library, tests, and examples. Parser and date edits to the same core file are serialized. R01 guards added before migration remain valid after migration. Tests that depend on future statuses/units are migrated with the owning change rather than left broken between completed tasks.

Independent packaging/licensing work can proceed alongside core work once its interfaces are agreed, but final install validation follows API stabilization. Actual commits, branches, worktrees, pushes, and release operations require the permissions in repository instructions; this design does not authorize them.

Mandatory acceptance configurations:

- Windows MSVC Debug and Release, with `/W4 /permissive-` and no warnings.
- Windows clang-cl build and installed-consumer linkage, without `m.lib`.
- Linux GCC and Clang strict C17, Debug and Release, with no implicit declarations or project warnings.
- macOS Clang build/tests and installed-consumer smoke test.
- A supported 32-bit POSIX build with large-offset boundary tests, including graceful rejection beyond addressable file size.
- ASan/UBSan, including floating-to-integer overflow checks where supported; TSan in a separate concurrent-test configuration.
- Offline default fixture/mocks, real FILE/mmap and endian regressions, actual concurrent access, documentation symbol coverage, license inventory, and relocated/custom-path installed consumers.
- Explicit opt-in DE440/DE440t references and available legacy/long-span/time-segmented local files. Report every missing dataset as a coverage limitation; do not count a skipped tier as a pass.

Use CMake/CTest gates and small scripts already supported by the project environment; do not add a runtime dependency for CI. Generated probes under ignored `build/review` are evidence only and must be replaced by tracked tests. Missing required toolchains/data are recorded as blockers under AGENTS.md rather than silently bypassed.

## 11. Complete coverage ledger

The following identifiers are permanent inputs to the implementation plan. R/L identifiers refer to the saved review. G identifiers split its coverage gaps, B identifiers split its contract boundaries, and S identifiers capture minor observations verified during review/planning. A combined implementation task may resolve several rows, but no row may disappear from the plan or final acceptance report.

### 11.1 Review findings

| ID | Required disposition | Design sections | Phases |
| --- | --- | --- | --- |
| R00 | Two-double arrays across APIs, coverage, arithmetic, ERFA, readers, tests, examples; disk format unchanged | 2, 3, 9 | P3 |
| R01a | Reject nonfinite query dates before integer conversions and shortcuts | 2, 3 | P2-P4 |
| R01b | Finite positive header, record, and subinterval spans | 4 | P2 |
| R01c | Geometry-bounded count/index conversion; rounded SIZE_MAX guard removed | 4 | P2 |
| R02 | Equivalent successful convenience centers 0 and 12 without invalid target zero | 2, 5 | P4 |
| R03a | Correct feature-test declarations in strict C17 POSIX builds | 4 | P2 |
| R03b | Actual offset-type limits, 32-bit large-file behavior | 4, 10 | P2, P7 |
| R04 | Exposed frame family, cited allowlist, native legacy access, explicit ICRS rejection | 5 | P4 |
| R05a | MIT grant and first-party attribution/headers everywhere applicable | 8 | P6 |
| R05b | Preserve and install full third-party notices | 8 | P6 |
| R06a | Reject overlapping/undersized header-only layouts | 4 | P2 |
| R06b | Correct trailing-record and ambiguous-width selection | 4 | P2 |
| R07 | Identity-checked independent FILE clones; safe failure on replacement | 6 | P5 |
| R08a | Validated original AU and Earth-Moon ratio | 4, 5 | P2 |
| R08b | Finite coefficients and derived states/rates, no successful NaN/Inf | 2, 5 | P4 |
| R08c | Explicit finite convergence, including NaN center/SSB case | 5 | P4 |
| R09 | Retry complete candidate validation in both byte orders | 4 | P2 |
| R10 | Tentative optional pointers bounded by established geometry | 4 | P2 |
| R11 | Recover valid near-start emission solutions; reject genuine pre-coverage roots | 5 | P4 |
| R12a | Discoverable installed config and version file | 8 | P6 |
| R12b | Consistent custom include/lib paths and relocation | 8 | P6 |
| R12c | Correct toolchain-resolved math linkage, including clang-cl | 8 | P6 |
| R13a | In-coverage ordinary-body errors fail validation | 7 | P1 |
| R13b | Missing angular skips justified by header absence | 7 | P1 |
| R13c | Finite TT-TDB reference inputs and valid fields before dispatch | 7 | P1 |
| R13d | Single justified libration-psi epoch factor, shared tolerances | 7 | P1, P7 |
| R13e | Distinguish absent files from damaged/unreadable present files | 7 | P1 |
| R14a | All public headers get file-level API reference pages | 9 | P6 |
| R14b | Check generated public-symbol coverage, not only warnings | 9 | P6 |
| R15 | Validated identical-body zero results do not require unrelated Sun data | 2, 5 | P4 |
| L01 | Correct physical light speed and independent test constants | 5 | P3-P4 |
| L02 | Modern public AU throughout, original metadata retained, legacy references converted | 5, 7 | P3 |
| L03 | Coverage/finiteness validation on SSB shortcuts | 2, 3 | P3-P4 |
| L04 | Correct TT/TDB amplitude/model/arithmetic accuracy and README time scales | 3, 9 | P3, P6 |
| L05 | Target zero consistently invalid; center zero consistently SSB | 2, 9 | P4, P6 |
| L06 | Verified download-cache reuse and offline second configure | 8 | P6 |

### 11.2 Coverage gaps and preserved contracts

| ID | Required disposition | Design sections |
| --- | --- | --- |
| G01 | Exact TT-TDB mock values/rates, not mere finiteness | 7 |
| G02 | TT-TDB subinterval/endpoint/appended-layout assertions | 4, 7 |
| G03 | Real shared-mmap and independent-FILE concurrent reads | 6 |
| G04 | Clone survives original and sibling closure; documented synchronization | 6 |
| G05 | Analytic moving-body light-time and independent residual | 5, 7 |
| G06 | Absolute time-conversion references and two-part precision | 3, 7 |
| G07 | Artifact digests plus recoverable source/tool provenance | 7 |
| G08 | Interior fixture row identities/coverage and honest provenance claim | 7, 9 |
| G09 | Durable cross-platform, sanitizer, concurrency CI gates | 10 |
| G10 | Separate installed C/C++ consumers and package/version/path tests | 8, 10 |
| G11 | Independent apparent-position/frame references with matching conventions | 5, 7 |
| G12 | Higher-order interpolation/derivatives and adjacent boundary epochs | 3, 7 |
| G13 | Invalid inputs, output-on-error, cleanup, and physical metadata mutations | 2, 4, 7 |
| B01 | Exact mixed-epoch velocity formula, not apparent tracking rate | 5, 9 |
| B02 | Fixed-epoch R*v, no frame derivative term | 5, 9 |
| B03 | Limited apparent-place corrections and observer definition | 1, 5, 9 |
| B04 | File coverage is not a model-accuracy guarantee | 3, 5, 9 |
| B05 | Scalar-JD precision loss is a defect, not an accepted limitation | 1, 3 |
| B06 | Preserve apparent-leg addition warning and immutability/lifetime rules | 5, 6, 9 |

### 11.3 Supplemental minor observations

These observations are part of the remediation scope, not reasons to overwrite the historical review or claim additional runtime reproductions. Their current evidence is source inspection unless stated in the saved review.

| ID | Observation and location at baseline | Required disposition |
| --- | --- | --- |
| S01 | README.md:35 says no data ship despite the committed fixture section | Correct operational-data versus test-fixture distinction, section 9 |
| S02 | AGENTS.md contains stale test counts, names, and environment-only activation | Align live instructions with actual option-gated tiers, section 9 |
| S03 | Doxyfile.in:2-3 contains unquoted configured paths | Quote values and test source/build paths with spaces, section 9 |
| S04 | src/jpl_private.h:46-47 and aberration square vectors without scaling | Scale-safe norms, invalid-velocity behavior, representability/alias tests, section 5 |
| S05 | tests/test_testpo.c:156-164 skips nutation rates already supported by fixture mapping | Validate X#3/4 rates and reject nonexistent components, section 7 |
| S06 | tests/test_fixture.c:136-143 checks DENUM in the wrong field of six-token rows | Correct shared parser and test all three row forms, section 7 |
| S07 | tests/test_testpo.c silently skips malformed rows, lacks DENUM/range checks, and does not check read errors | Strict data-section grammar and negative executable tests, section 7 |

## 12. Completion and handoff

The implementation plan must give every ledger row one or more owning tasks with exact files, prerequisites, regression cases, verification commands, and expected outcomes. Separate API migration work from scientific-model changes so regressions can be attributed. Preserve repository changes made by the maintainer or other sessions.

Review the completed specification for missing IDs, contradictions, undefined public behavior, and unsupported numerical claims before plan generation. Concrete algorithm choices in this document are engineering decisions implementing the approved contracts; changes to approved units, representation, licensing, or legacy-frame scope require maintainer review.

Record fixes in the saved review's resolution log only after code and tracked regressions exist and verification was actually run. Include the finding ID, fix revision when available, test names, configurations, and any remaining blocked evidence. A design document, written plan, skipped tier, or default Windows pass is not proof that every issue is resolved.
