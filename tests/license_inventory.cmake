# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# First-party license inventory.
# Every path below is relative to the source root, must exist, and must
# carry "Copyright .*Todd Carnes" plus "SPDX-License-Identifier: MIT" in
# its native comment syntax (C block, "#" script, or HTML comment).
# Verified first-party creation year for all entries is 2026 (sole author
# Todd Carnes; initial commit 2026-09-15); no earlier year is claimed.
# JPL_LICENSE_EXEMPT_FILES lists files that must exist but must NOT gain
# a notice, each for a stated reason. The first-party count is kept in
# one place only: the list length below (no duplicated literal counts
# elsewhere); report counts must derive from this list.
# Sweep-exempt directory prefixes
# (checked in tests/cmake/check_licenses.cmake, not listed per file):
#   src/erfa/          - vendored third-party ERFA (BSD-3, never modified)
#   docs/specs/        - historical design records, preserved verbatim
#   docs/plans/        - historical implementation plans, preserved verbatim
#   docs/reviews/      - saved review plus progress ledger, verbatim
#   build/, out/, cmake-build-*/ - generated build trees, no notices
#   .git/, .worktrees/, .superpowers/, docs/html/, docs/latex/ - tooling output
set(JPL_LICENSE_FIRST_PARTY
  "CMakeLists.txt"
  "Doxyfile.in"
  ".gitignore"
  "README.md"
  "BUILDING.md"
  "LICENSES.md"
  "CHANGELOG.md"
  "AGENTS.md"
  "GLOSSARY.md"
  "docs/api.dox"
  "docs/validation.md"
  "docs/migration-two-part-dates.md"
  "docs/agents/issue-tracker.md"
  "docs/agents/triage-labels.md"
  "docs/agents/domain.md"
  "include/jpl.h"
  "include/jpl_frames.h"
  "include/jpl_convenience.h"
  "include/jpl_testing.h"
  "src/jpl.c"
  "src/jpl_date.c"
  "src/jpl_frames.c"
  "src/jpl_convenience.c"
  "src/jpl_private.h"
  "tests/jpl_test_util.h"
  "tests/jpl_testpo_util.h"
  "tests/jpl_testpo_util.c"
  "tests/examples.c"
  "tests/public_symbols.txt"
  "tests/make_validator_data.c"
  "tests/test_jpl.c"
  "tests/test_lighttime.c"
  "tests/jpl_fixture.h"
  "tests/jpl_fixture.c"
  "tests/test_fixture_builder.c"
  "tests/test_aberration.c"
  "tests/test_frames.c"
  "tests/test_convenience.c"
  "tests/test_mmap.c"
  "tests/test_threads.c"
  "tests/test_tt_tdb.c"
  "tests/test_parser.c"
  "tests/test_api_errors.c"
  "tests/test_numeric.c"
  "tests/test_dates.c"
  "tests/test_largefile.c"
  "tests/test_testpo_parser.c"
  "tests/test_fixture.c"
  "tests/test_testpo.c"
  "tests/test_real_ephemeris.c"
  "tests/test_horizons.c"
  "tests/test_all_ephemerides.c"
  "tests/data/numerical_reference.h"
  "tests/data/numerical_reference.md"
  "tests/data/MANIFEST.txt"
  "tests/data/PROVENANCE.md"
  "tools/generate_numerical_reference.c"
  "tools/fixture_tool.c"
  "tests/license_inventory.cmake"
  "cmake/libjplConfig.cmake.in"
  "cmake/jpl_export.h.in"
  "cmake/jpl_version.rc.in"
  "cmake/LibjplTestData.cmake"
  "cmake/sdk_manifest.cmake"
  "cmake/sdk_package.cmake"
  "cmake/verify_sdk.cmake"
  ".github/workflows/ci.yml"
  ".github/workflows/ephemerides.yml"
  "tests/consumer/CMakeLists.txt"
  "tests/consumer/main.c"
  "tests/consumer/main.cpp"
  "tests/cmake/check_licenses.cmake"
  "tests/cmake/test_licenses.cmake"
  "tests/cmake/check_fixture.cmake"
  "tests/cmake/check_docs.cmake"
  "tests/cmake/test_docs_paths.cmake"
  "tests/cmake/test_fixture_integrity.cmake"
  "tests/cmake/test_validators.cmake"
  "tests/cmake/test_fetch_cache.cmake"
  "tests/cmake/check_workflows.cmake"
  "tests/cmake/check_numerical_reference.cmake"
  "tests/cmake/check_generated_reference.cmake"
  "tests/cmake/check_horizons_reference.cmake"
  "tests/cmake/test_horizons_compare.cmake"
  "tests/cmake/check_archive_symbols.cmake"
  "tests/cmake/test_fixture_tool.cmake"
  "tests/cmake/test_docs_only.cmake"
  "tests/cmake/test_install.cmake"
  "bindings/cpp/CMakeLists.txt"
  "bindings/cpp/include/jpl/jpl.hpp"
  "bindings/cpp/include/jpl/ephemeris.hpp"
  "bindings/cpp/tests/test_cpp_binding.cpp"
  "bindings/python/CMakeLists.txt"
  "bindings/python/pyproject.toml"
  "bindings/python/pyproject.toml.in"
  "bindings/python/src/libjpl/__init__.py"
  "bindings/python/src/libjpl/_libjpl.py"
  "bindings/python/src/libjpl/ephemeris.py"
  "bindings/python/tests/test_python_binding.py"
  "bindings/csharp/CMakeLists.txt"
  "bindings/csharp/src/LibJpl/LibJpl.csproj"
  "bindings/csharp/src/LibJpl/JplEnums.cs"
  "bindings/csharp/src/LibJpl/Native.cs"
  "bindings/csharp/src/LibJpl/JplEphemeris.cs"
  "bindings/csharp/src/LibJpl/LibJpl.cs"
  "bindings/csharp/tests/LibJpl.Tests.csproj"
  "bindings/csharp/tests/Program.cs"
  "bindings/vbnet/CMakeLists.txt"
  "bindings/vbnet/src/LibJpl/LibJpl.vbproj"
  "bindings/vbnet/src/LibJpl/LibJpl.vb"
  "bindings/vbnet/src/LibJpl/JplEphemeris.vb"
  "bindings/vbnet/src/LibJpl/JplEnums.vb"
  "bindings/vbnet/src/LibJpl/Native.vb"
  "bindings/vbnet/tests/LibJpl.VbTests.vbproj"
  "bindings/vbnet/tests/Program.vb"
  "bindings/cpp/examples/planet_positions.cpp"
  "bindings/cpp/examples/README.md"
  "bindings/python/examples/README.md"
  "bindings/python/examples/planet_positions.py"
  "bindings/csharp/examples/README.md"
  "bindings/csharp/examples/PlanetPositions.cs"
  "bindings/vbnet/examples/README.md"
  "bindings/vbnet/examples/PlanetPositions.vb"
  "bindings/c/examples/README.md"
  "bindings/CONTRACT.md"
)
# Exempt files and why:
#   LICENSE - the full MIT grant itself (checked separately for completeness).
#   src/erfa/LICENSE - third-party ERFA BSD-3 grant (checked on install only).
#   tests/data/de441_2000_2010.eph - binary fixture, non-commentable.
#   tests/data/testpo_441_slice.txt - third-party JPL reference data, byte-pinned.
#   tests/data/horizons_de441.txt - third-party JPL observer reference data, byte-pinned.
#   tests/data/fixture_rows.txt - hashed ordered identity list, parser-read.
set(JPL_LICENSE_EXEMPT_FILES
  "LICENSE"
  "src/erfa/LICENSE"
  "tests/data/de441_2000_2010.eph"
  "tests/data/testpo_441_slice.txt"
  "tests/data/horizons_de441.txt"
  "tests/data/fixture_rows.txt"
  "tests/data/numerical_reference.lf.h"
)
