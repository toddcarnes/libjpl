# Distribution Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship correctly attributed first-party code, a discoverable relocatable CMake package, reliable test-data caching, and complete accurate public documentation.

**Architecture:** Extend the existing CMake install and test-data flow without adding a runtime dependency. Verify installed consumers separately from build-tree linking, classify source ownership explicitly, and check generated API symbols as well as Doxygen warnings.

**Tech Stack:** CMake 3.20+, CTest, C17/C++ consumer smoke tests, Doxygen HTML/XML, MIT and ERFA BSD-3-Clause notices.

**Spec:** [Approved design](../specs/2026-09-17-review-remediation-design.md); [master plan and interfaces](2026-09-17-review-remediation.md).

## Global Constraints

- License first-party code under MIT, with Todd Carnes as copyright holder. Preserve third-party licensing and do not edit vendored ERFA sources.
- All computational Julian epochs use actual arrays of two doubles. Both parts survive arithmetic and ERFA calls.
- All public AU-valued results and conversion helpers use the exact modern definition, `1 AU = 149597870.7 km`. Original header constants and raw records remain unchanged.
- Keep CMake 3.20+, C17, `C_EXTENSIONS OFF`, static `jpl`, three installed public headers, and offline default tests.
- No version bump, release, commit, or publication is authorized. Exact-version package compatibility is used while pre-release.
- No runtime/ordinary-library-build dependency is added. C++ is required only by the installed-consumer validation, not to build the C library.
- Follow the master's dependencies and common verification cycle. CMake test drivers operate only on unique owned build-tree directories and must never remove or overwrite maintainer-local ephemerides.

---

## CMake test-driver convention

Use script-mode tests with explicit `SOURCE`, `BUILD`, `CONFIG`, `GENERATOR`, `PLATFORM`, `TOOLSET`, and toolchain/compiler arguments passed through `add_test`. Pass `-DNAME=value` as one quoted argument, and add generator platform/toolset options only when nonempty. Quote every path. Use `file(MAKE_DIRECTORY)` only under a unique directory beneath `${BUILD}/test-work/`; choose a timestamp/random suffix and do not delete an unknown existing tree.

For expected-success subprocesses, define this local helper in the owning test script rather than assuming a global function:

```cmake
function(run_checked)
  execute_process(COMMAND ${ARGV}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Command failed (${result}): ${ARGV}\n${output}\n${error}")
  endif()
endfunction()
```

Expected-negative tests inspect both exit result and diagnostic; they must not pass because a command or input file was missing unexpectedly. Do not use CMake APIs newer than 3.20 without raising an explicit design issue; in particular avoid the newer RESULT option of `file(RENAME)` or newer copy options when the plain 3.20 forms suffice.

## Task T18: MIT license, source headers, and third-party notices

**Dependencies:** T00; repeat inventory after all implementation files exist. **Coverage:** R05a/b and the maintainer's explicit source-header requirement.

**Files:** Create `LICENSE`, `LICENSES.md`, `tests/license_inventory.cmake`, `tests/cmake/check_licenses.cmake`; modify all first-party C/header/build/script/config/documentation files that require notices, `README.md`, `CMakeLists.txt`. Inspect `src/erfa/LICENSE` without changing it.

**Interfaces:** Register CTest `jpl_licenses` and a script-mode checker consuming `SOURCE` plus optional `INSTALL_PREFIX`. Installed notices are `${CMAKE_INSTALL_DATADIR}/licenses/libjpl/LICENSE` and `ERFA-LICENSE`.

- [ ] Inventory owned and third-party material before adding headers. Use git history to confirm creation years and existing attribution. Classify `src/erfa/**`, the binary fixture, original JPL reference text, generated build trees, and external provenance separately. Do not mark all `tests/data` as third-party because newly generated first-party reference headers/scripts require notices.
- [ ] Add the ownership inventory and a checker that first fails on the missing root license and first-party source notices. For each first-party text/code file require the declared copyright holder and SPDX identifier; exempt full license texts and explicitly classified non-commentable/third-party files for stated reasons. Check that enumerated paths exist and that new eligible files are not silently outside the inventory.

```cmake
file(READ "${SOURCE}/src/jpl.c" content)
if(NOT content MATCHES "Copyright .*Todd Carnes" OR
   NOT content MATCHES "SPDX-License-Identifier: MIT")
  message(FATAL_ERROR "Missing first-party notice: src/jpl.c")
endif()
```

- [ ] Add the complete MIT license, not only an SPDX tag:

```text
MIT License

Copyright (c) 2026 Todd Carnes

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

If git history establishes an earlier first-party creation year, use the verified range consistently rather than inventing one. Preserve legitimate existing contributor notices.

- [ ] Add notices before includes/header guards and in appropriate native comments. Use this C form and equivalent `#`/HTML comments for build scripts/Markdown:

```c
/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
```

Cover production sources, all three public headers, private headers, tests, test utilities, generators, CMake modules, workflows, and appropriate first-party documentation. Preserve shebang placement and existing Doxygen blocks. Generated first-party source artifacts carry the generator-emitted notice. Do not modify vendored ERFA or insert comments into ephemeris/reference data.

- [ ] Explain ownership and data provenance in `LICENSES.md`; link root MIT and ERFA notices from README. Install both complete license texts unconditionally, with distinct filenames:

```cmake
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/libjpl")
install(FILES src/erfa/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/libjpl"
        RENAME ERFA-LICENSE)
```

- [ ] Add negative checker cases using build-tree copies with a removed SPDX header or missing installed ERFA notice. Require failure, then pass the intact source/install. Confirm `git diff -- src/erfa` is empty. Run `jpl_licenses`, common cycle, and later T19's installed notice check. Checkpoint: `chore(license): apply MIT attribution and distribute ERFA notices`.

## Task T19: Discoverable, relocatable installed package

**Dependencies:** T12, T18. **Coverage:** R12a/b/c, G10, installed R05b.

**Files:** Modify `CMakeLists.txt`; create `cmake/libjplConfig.cmake.in`, `tests/consumer/CMakeLists.txt`, `tests/consumer/main.c`, `tests/consumer/main.cpp`, `tests/cmake/test_install.cmake`.

**Interfaces:** Installed `find_package(libjpl CONFIG REQUIRED)` provides `jpl::jpl`; exact-version config reflects current internal project version. Register CTest `jpl_install`.

- [ ] Create the separate consumer before fixing packaging and demonstrate configure failure against a baseline installation. Its CMakeLists declares C/CXX, finds the package, and builds both executables:

```cmake
cmake_minimum_required(VERSION 3.20)
project(libjpl_consumer LANGUAGES C CXX)
if(DEFINED REQUEST_VERSION)
  find_package(libjpl ${REQUEST_VERSION} EXACT CONFIG REQUIRED)
else()
  find_package(libjpl CONFIG REQUIRED)
endif()
add_executable(consumer_c main.c)
add_executable(consumer_cpp main.cpp)
target_link_libraries(consumer_c PRIVATE jpl::jpl)
target_link_libraries(consumer_cpp PRIVATE jpl::jpl)
enable_testing()
add_test(NAME consumer_c COMMAND consumer_c)
add_test(NAME consumer_cpp COMMAND consumer_cpp)
```

- [ ] Make both source files include all three public headers and reference symbols that pull math/ERFA objects from the static archive. For `main.cpp`, replace `<math.h>` with `<cmath>` and call `std::isfinite`; use the same public API and ordinary brace initialization. Neither source includes private headers:

```c
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>
#include <math.h>
int main(void) {
    const double tt[2] = {2451545.0, 0.125};
    double tdb[2], rotation[3][3], au;
    if (jpl_tt_to_tdb(tt, tdb) != JPL_OK) return 1;
    if (jpl_frame_rotation_matrix(tt, rotation) != JPL_OK) return 1;
    if (jpl_km_to_au(149597870.7, &au) != JPL_OK || au != 1.0) return 1;
    return !isfinite(tdb[0]) || !isfinite(rotation[0][0]);
}
```

- [ ] Move `include(GNUInstallDirs)` before include-directory construction and export `$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>`. Add package config/version generation:

```cmake
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/libjplConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/libjplConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libjpl")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/libjplConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjplConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/libjplConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libjpl")
```

The template contains its MIT comment, `@PACKAGE_INIT@`, `include("${CMAKE_CURRENT_LIST_DIR}/libjplTargets.cmake")`, and `check_required_components(libjpl)`. Do not embed source/build paths.

- [ ] Resolve math linkage at configure time. On MSVC-compatible frontends, rely on the CRT and do not request m. Else use `CheckSymbolExists` to test `sin` with current default libraries; if unavailable, `CheckLibraryExists` for `m` and add the concrete `m` dependency to `jpl`. Fail with a clear configuration error if neither works. Preserve static dependency propagation; no consumer-evaluated compiler-ID generator expression remains.
- [ ] Implement the install driver using unique build-tree prefixes. Install the already-built configuration, configure/build/test the separate consumer with CMAKE_PREFIX_PATH, and request the exact current version. A deliberately incompatible version must fail package discovery for the version reason. Supply compiler/toolchain/generator settings consistently to child configurations.
- [ ] Move the owned install prefix to a new path containing spaces and configure a fresh consumer there; it must pass with no old prefix. Then create a tests-OFF child library build with `-DCMAKE_INSTALL_INCLUDEDIR=headers/jpl -DCMAKE_INSTALL_LIBDIR=custom-lib`, install it, and repeat consumption. Do not mutate the primary build cache to test custom destinations.
- [ ] Inspect installed headers/notices and exports. Require only the three public headers, full MIT/ERFA texts, no checkout paths, and no `m.lib` in clang-cl. Test PIC ON/OFF normal static consumption; on POSIX, add a small shared-library consumer for PIC ON. Run `jpl_install`, common cycle, and T22 toolchain variants. Checkpoint: `build(package): install relocatable config and consumer tests`.

## Task T20: Verified download-cache reuse

**Dependencies:** T15, T17. **Coverage:** L06 and offline test-data policy.

**Files:** Create `cmake/LibjplTestData.cmake`, `tests/cmake/test_fetch_cache.cmake`; move the existing manifest/resolver functions from `CMakeLists.txt` into that module without changing call sites; modify `tests/data/MANIFEST.txt` only for actually verified metadata.

**Interfaces:** Preserve exact existing function signatures:

```cmake
libjpl_manifest_value(out_var key)
libjpl_resolve_fetch_file(out_var src_dir src_name url dest_name expected_size sha256)
```

The resolver consumes `LIBJPL_EPHEMERIS_DIR`. Register CTest `jpl_fetch_cache`; scripts pass a controlled source/manifest location rather than depending on the caller's current directory. No network runs while the fetch option is OFF.

- [ ] Add a four-byte controlled artifact and its real computed hash in a build-tree test. The initial fetch uses a platform-correct file URL. Rename the source within that owned test directory, then call the resolver again: a valid destination must be reused without accessing the unavailable URL. The old resolver attempts another download and fails.
- [ ] Extract existing functions into the module, keep manifest key parsing and local redirect behavior, and add the cache decision before download:

```cmake
set(dest "${LIBJPL_EPHEMERIS_DIR}/${dest_name}")
set(cache_valid FALSE)
if(EXISTS "${dest}")
  file(SIZE "${dest}" actual_size)
  if("${expected_size}" STREQUAL "" OR actual_size EQUAL expected_size)
    if("${sha256}" STREQUAL "")
      set(cache_valid TRUE)
    else()
      file(SHA256 "${dest}" actual_hash)
      if(actual_hash STREQUAL sha256)
        set(cache_valid TRUE)
      endif()
    endif()
  endif()
endif()
if(cache_valid)
  set(${out_var} "${dest}" PARENT_SCOPE)
  return()
endif()
```

- [ ] Download to a unique sibling temporary file. Check download status, exact size when supplied, and hash when supplied; only then rename into the destination using CMake 3.20-compatible operations. A failed fetch leaves an existing destination intact and produces a diagnostic naming source URL, expected/actual verification, and file. Never delete local redirected sources. Return the final path only after successful validation/publication.
- [ ] Add cases for truncated cache, same-size wrong-hash cache, interrupted/partial temporary file, unavailable URL, successful replacement, zero/missing manifest value, and source redirects with a deliberately different legitimate span/size. Hashless cache entries get the documented size-only guarantee, not a false claim of cryptographic integrity.
- [ ] Make small script tests exercise the same resolver used by production CMake. Use fresh directories, file URLs and deliberate file renames to avoid a web-server dependency. Verify configuration with FETCH OFF does not even evaluate redirect directories or download URLs. Verify the real fetch tier later with a populated cache and network unavailable; store actual artifacts/spans/hashes in validation evidence.
- [ ] Obtain hashes for exact published binary artifacts only when the source/span is verified. Do not copy hashes from differently spanned local DE files. Leave size-only policy explicit if exact hashes cannot be established; content/reference tests remain required. Run `jpl_fetch_cache`, common cycle, and opt-in real-tier reconfigure. Checkpoint: `build(data): reuse verified ephemerides without destructive fetches`.

## Task T21: Complete public documentation and minor corrections

**Dependencies:** T12, T16, T18, T19, T20. **Coverage:** R14a/b, L04/L05, B01-B06, S01-S03.

**Files:** Modify all public-header Doxygen comments, `Doxyfile.in`, `CMakeLists.txt`, `README.md`, `AGENTS.md`, `CHANGELOG.md`, live inaccurate test comments; create `docs/migration-two-part-dates.md`, `tests/public_symbols.txt`, `tests/cmake/check_docs.cmake`, `tests/cmake/test_docs_paths.cmake`, `tests/examples.c`; update `docs/validation.md`.

**Interfaces:** Doxygen emits HTML and XML; CTest `jpl_docs_symbols` and `jpl_docs_paths` exist when docs are enabled. `jpl_examples` compiles/runs ordinary examples. Public symbol inventory is explicit and checked against actual headers at review time.

- [ ] Add a generated-symbol test before changing extraction. Populate `tests/public_symbols.txt` with every public function, enum, and struct, including the new frame enum/statuses. Check `<name>jpl_planet_position</name>` and `<name>jpl_frame_rotation_matrix</name>` in generated XML; the baseline docs omit them. Also require all three file reference pages, not only source listings.
- [ ] Add `@file` documentation to `jpl_frames.h` and `jpl_convenience.h` without removing existing declaration descriptions. Complete each public parameter/status/note contract. Add generated documentation license information pointing to full project/ERFA notices, without exposing ERFA declarations as public API.
- [ ] Quote configured paths and enable XML:

```text
OUTPUT_DIRECTORY = "@CMAKE_CURRENT_BINARY_DIR@/docs"
INPUT = "@CMAKE_CURRENT_SOURCE_DIR@/include"
GENERATE_HTML = YES
GENERATE_XML = YES
GENERATE_LATEX = NO
WARN_AS_ERROR = YES
```

Use `add_custom_target(docs ALL COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" VERBATIM)`. The symbol checker uses `file(STRINGS)` for the expected inventory and `string(FIND)` against XML names, failing on any missing entry. Add a negative test on an owned XML copy with one expected symbol removed.

- [ ] Implement `test_docs_paths.cmake` by copying an explicit source-file/directory whitelist into a unique owned `source tree` path, configuring a tests-OFF/docs-ON child build in `build tree`, and building target docs. Copy current files rather than a git archive so uncommitted implementation is tested. Exclude `.git`, build trees, local ephemerides, and generated outputs. Require XML/public pages and run the same symbol checker there.
- [ ] Write a migration guide with before/after signatures, status-returning time/frame/math changes, two-part construction, low-part-preserving arithmetic expectations, modern AU semantics, frame restrictions, and error/output behavior. Explain that raw on-disk epochs remain scalar and that already-rounded scalar input cannot be repaired by splitting. No scalar wrapper is offered.
- [ ] Correct every live minor statement: TT-TDB is millisecond-scale, not 70 seconds; no false 2 ms model floor; interpolation/model errors differ from arithmetic precision; ephemeris corrections are not mathematically exact; only a small fixture ships; environment variables redirect but do not enable fetch; actual suite names replace stale names; no hard-coded stale count. Historical reviewed statements remain labeled historical instead of silently rewriting history.
- [ ] Document exact mixed-epoch velocity, fixed-epoch R*v, body-center observer and correction scope, legacy/unknown rejection, frame/time-model validity intervals, approximate apparent-leg addition, FILE/mmap immutability and synchronization, clone failure after path disappearance, and raw-versus-computed output-on-error behavior.
- [ ] Compile README examples from `tests/examples.c` with status checks before output use. The basic pattern must be:

```c
const double tt[2] = {2451545.0, 0.125};
double tdb[2], state[6];
if (jpl_tt_to_tdb(tt, tdb) != JPL_OK) return 1;
if (jpl_get_state(ephemeris, tdb, JPL_EARTH, JPL_SUN, state) != JPL_OK) return 1;
```

The example executable receives the committed fixture path through CTest arguments and opens/closes it itself. Include each public header individually in consumer smoke tests and retain C++ linkage tests. Add Unreleased migration notes without changing project version or publishing.

- [ ] Configure docs with `cmake -S . -B build/remediation -DLIBJPL_FETCH_EPHEMERIDES=OFF -DLIBJPL_BUILD_DOCS=ON`, build docs and examples, then run `ctest --test-dir build/remediation -C Debug -R 'jpl_docs|jpl_examples|jpl_licenses' --output-on-failure` and common cycle. Re-run the license inventory after new docs/scripts exist. Checkpoint: `docs(api): publish complete two-part-date and validation contracts`.
