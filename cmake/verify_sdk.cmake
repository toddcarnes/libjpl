# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# SDK staging verifier (test-only packaging helper).
# Confirms that a staged SDK tree actually carries the public API surface,
# the license texts, the documentation entry point, the CMake package
# files, and the static library. Compares the copied files against their
# real sources so a dropped or stale copy fails the build instead of
# shipping silently.
# Variables: SDK_ROOT (required staged tree), SOURCE (required source
# root), GENERATED_DIR (required build-tree directory holding
# jpl_export.h).
if(NOT DEFINED SDK_ROOT OR "${SDK_ROOT}" STREQUAL "")
  message(FATAL_ERROR "verify_sdk.cmake needs SDK_ROOT")
endif()
if(NOT DEFINED SOURCE OR "${SOURCE}" STREQUAL "")
  message(FATAL_ERROR "verify_sdk.cmake needs SOURCE")
endif()
if(NOT DEFINED GENERATED_DIR OR "${GENERATED_DIR}" STREQUAL "")
  message(FATAL_ERROR "verify_sdk.cmake needs GENERATED_DIR")
endif()

set(sdk_required
  "include/jpl.h"
  "include/jpl_frames.h"
  "include/jpl_convenience.h"
  "include/jpl_export.h"
  "licenses/LICENSE"
  "licenses/ERFA-LICENSE"
  "doc/README.md"
)
foreach(rel IN LISTS sdk_required)
  if(NOT EXISTS "${SDK_ROOT}/${rel}")
    message(FATAL_ERROR "SDK is missing ${rel} under ${SDK_ROOT}")
  endif()
endforeach()

set(sdk_copies
  "include/jpl.h|${SOURCE}/include/jpl.h"
  "include/jpl_frames.h|${SOURCE}/include/jpl_frames.h"
  "include/jpl_convenience.h|${SOURCE}/include/jpl_convenience.h"
  "include/jpl_export.h|${GENERATED_DIR}/jpl_export.h"
  "licenses/LICENSE|${SOURCE}/LICENSE"
  "licenses/ERFA-LICENSE|${SOURCE}/src/erfa/LICENSE"
  "doc/README.md|${SOURCE}/README.md"
)
foreach(pair IN LISTS sdk_copies)
  string(REPLACE "|" ";" pair "${pair}")
  list(GET pair 0 staged_rel)
  list(GET pair 1 source_path)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files
      "${SDK_ROOT}/${staged_rel}" "${source_path}"
    RESULT_VARIABLE compare_rc)
  if(NOT compare_rc EQUAL 0)
    message(FATAL_ERROR "SDK file ${staged_rel} does not match ${source_path}")
  endif()
endforeach()

file(GLOB_RECURSE sdk_files "${SDK_ROOT}/*")
list(FILTER sdk_files INCLUDE REGEX "/libjplConfig\\.cmake$")
list(LENGTH sdk_files config_count)
if(config_count EQUAL 0)
  message(FATAL_ERROR "SDK is missing libjplConfig.cmake under ${SDK_ROOT}")
endif()

file(GLOB_RECURSE sdk_libs "${SDK_ROOT}/*")
list(FILTER sdk_libs INCLUDE REGEX "/static/[^/]*jpl[^/]*\\.(lib|a)$")
list(LENGTH sdk_libs lib_count)
if(lib_count EQUAL 0)
  message(FATAL_ERROR "SDK is missing the static library archive under ${SDK_ROOT}")
endif()

message(STATUS "SDK staging verified: ${SDK_ROOT}")
