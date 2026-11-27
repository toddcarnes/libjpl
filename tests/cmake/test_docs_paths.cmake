# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Docs path-with-spaces verifier (test-only).
# Copies an explicit source-file/directory whitelist into a unique owned
# "source tree" path containing spaces, configures a tests-OFF/docs-ON
# child build in a "build tree" path containing spaces, builds target
# docs, then runs the same symbol checker there. Current files are
# copied (not a git archive) so uncommitted implementation is tested.
# Excludes .git, build trees, local ephemerides, and generated outputs.
# Variables: SOURCE (required source root), CHECKER (required
# check_docs.cmake path), INVENTORY (required public_symbols.txt path),
# WORK (required owned scratch dir), GENERATOR (required CMake
# generator for the child configure), CONFIG (required build
# configuration, e.g. Debug).
if(NOT DEFINED SOURCE OR "${SOURCE}" STREQUAL ""
    OR NOT DEFINED CHECKER OR "${CHECKER}" STREQUAL ""
    OR NOT DEFINED INVENTORY OR "${INVENTORY}" STREQUAL ""
    OR NOT DEFINED WORK OR "${WORK}" STREQUAL ""
    OR NOT DEFINED GENERATOR OR "${GENERATOR}" STREQUAL ""
    OR NOT DEFINED CONFIG OR "${CONFIG}" STREQUAL "")
  message(FATAL_ERROR "test_docs_paths.cmake needs SOURCE, CHECKER, INVENTORY, WORK, GENERATOR and CONFIG")
endif()
foreach(required IN ITEMS SOURCE CHECKER INVENTORY)
  if(NOT EXISTS "${${required}}")
    message(FATAL_ERROR "test_docs_paths.cmake: ${required} does not exist: ${${required}}")
  endif()
endforeach()
string(TIMESTAMP stamp "%Y%m%dT%H%M%S" UTC)
string(RANDOM LENGTH 6 rnd)
set(SRC_COPY "${WORK}/source tree ${stamp} ${rnd}")
set(BUILD_TREE "${WORK}/build tree ${stamp} ${rnd}")
file(MAKE_DIRECTORY "${SRC_COPY}")
# Explicit whitelist: the three public headers plus the minimum build
# inputs for a tests-OFF/docs-ON child configure. No .git, no build
# trees, no ephemerides, no generated outputs are copied.
set(whitelist
  "CMakeLists.txt"
  "Doxyfile.in"
  "docs/api.dox"
  "LICENSE"
  "include/jpl.h"
  "include/jpl_frames.h"
  "include/jpl_convenience.h"
  "src/jpl.c"
  "src/jpl_date.c"
  "src/jpl_frames.c"
  "src/jpl_convenience.c"
  "src/jpl_private.h"
  "src/erfa/README-ERFA.md"
  "cmake/jpl_export.h.in"
  "cmake/jpl_version.rc.in"
  "cmake/libjplConfig.cmake.in"
  "cmake/LibjplTestData.cmake"
  "bindings/python/pyproject.toml.in")
file(GLOB erfa_sources RELATIVE "${SOURCE}" "${SOURCE}/src/erfa/*.c")
list(APPEND whitelist ${erfa_sources})
file(GLOB erfa_headers RELATIVE "${SOURCE}" "${SOURCE}/src/erfa/*.h")
list(APPEND whitelist ${erfa_headers})
foreach(rel IN LISTS whitelist)
  get_filename_component(rel_dir "${rel}" DIRECTORY)
  file(MAKE_DIRECTORY "${SRC_COPY}/${rel_dir}")
  if(NOT EXISTS "${SOURCE}/${rel}")
    message(FATAL_ERROR "test_docs_paths.cmake: whitelist file missing: ${SOURCE}/${rel}")
  endif()
  file(COPY_FILE "${SOURCE}/${rel}" "${SRC_COPY}/${rel}")
endforeach()
# Child configure: tests OFF (no test sources needed), docs ON.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${SRC_COPY}" -B "${BUILD_TREE}"
    -G "${GENERATOR}"
    "-DLIBJPL_BUILD_TESTS=OFF" "-DLIBJPL_BUILD_DOCS=ON"
  RESULT_VARIABLE cfg_rc OUTPUT_VARIABLE cfg_out ERROR_VARIABLE cfg_err)
if(NOT "${cfg_rc}" STREQUAL "0")
  message(FATAL_ERROR "docs path child configure failed (${cfg_rc}):\n${cfg_out}\n${cfg_err}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_TREE}" --config "${CONFIG}"
    --target docs --parallel
  RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
if(NOT "${build_rc}" STREQUAL "0")
  message(FATAL_ERROR "docs path child build failed (${build_rc}):\n${build_out}\n${build_err}")
endif()
set(child_xml "${BUILD_TREE}/docs/xml")
if(NOT EXISTS "${child_xml}")
  message(FATAL_ERROR "docs path child produced no XML dir: ${child_xml}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DEXPECTED=${INVENTORY}" "-DXML_DIR=${child_xml}"
    "-DWORK=${WORK}/checker-work"
    -P "${CHECKER}"
  RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
if(NOT "${check_rc}" STREQUAL "0")
  message(FATAL_ERROR "docs path symbol check failed (${check_rc}):\n${check_out}\n${check_err}")
endif()
message(STATUS "libjpl: docs path-with-spaces verification OK")
