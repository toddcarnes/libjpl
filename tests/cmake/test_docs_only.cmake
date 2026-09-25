# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Docs-only configure matrix (test-only, F15).
# Creates a disposable nested build with docs ON / tests OFF, builds the
# docs target, lists its tests, and requires exactly jpl_docs_symbols and
# jpl_docs_paths while rejecting ordinary executables (jpl_tests,
# jpl_parser). Variables: SOURCE (required source root), WORK (required
# disposable build directory; must not equal SOURCE).
if(NOT DEFINED SOURCE OR "${SOURCE}" STREQUAL "")
  message(FATAL_ERROR "test_docs_only.cmake needs SOURCE")
endif()
if(NOT DEFINED WORK OR "${WORK}" STREQUAL "")
  message(FATAL_ERROR "test_docs_only.cmake needs WORK")
endif()
if("${WORK}" STREQUAL "${SOURCE}")
  message(FATAL_ERROR "test_docs_only.cmake: WORK must not equal SOURCE")
endif()
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK}"
    -DLIBJPL_BUILD_DOCS=ON -DLIBJPL_BUILD_TESTS=OFF -DLIBJPL_FETCH_EPHEMERIDES=OFF
  RESULT_VARIABLE cfg_rc OUTPUT_VARIABLE cfg_out ERROR_VARIABLE cfg_err)
if(NOT cfg_rc EQUAL 0)
  message(FATAL_ERROR "docs-only configure failed:\n${cfg_out}\n${cfg_err}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${WORK}" --target docs
  RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_err)
if(NOT build_rc EQUAL 0)
  message(FATAL_ERROR "docs-only docs build failed:\n${build_out}\n${build_err}")
endif()
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" -N --test-dir "${WORK}"
  RESULT_VARIABLE list_rc OUTPUT_VARIABLE list_out ERROR_VARIABLE list_err)
if(NOT list_rc EQUAL 0)
  message(FATAL_ERROR "docs-only ctest -N failed:\n${list_out}\n${list_err}")
endif()
if(NOT list_out MATCHES "jpl_docs_symbols")
  message(FATAL_ERROR "docs-only build lacks jpl_docs_symbols:\n${list_out}")
endif()
if(NOT list_out MATCHES "jpl_docs_paths")
  message(FATAL_ERROR "docs-only build lacks jpl_docs_paths:\n${list_out}")
endif()
if(list_out MATCHES "jpl_tests" OR list_out MATCHES "jpl_parser")
  message(FATAL_ERROR "docs-only build registered ordinary tests:\n${list_out}")
endif()
# Multi-config generators (Visual Studio) need an explicit -C flag; forward
# the parent's configuration when known, else try Debug then Release.
set(_doc_configs "")
if(DEFINED ENV{JPL_DOCS_ONLY_CONFIG} AND NOT "$ENV{JPL_DOCS_ONLY_CONFIG}" STREQUAL "")
  list(APPEND _doc_configs "$ENV{JPL_DOCS_ONLY_CONFIG}")
endif()
list(APPEND _doc_configs Debug Release "")
set(_tested 0)
foreach(_cfg IN LISTS _doc_configs)
  if(_cfg STREQUAL "")
    execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}" --output-on-failure
      RESULT_VARIABLE test_rc OUTPUT_VARIABLE test_out ERROR_VARIABLE test_err)
  else()
    execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}" -C "${_cfg}" --output-on-failure
      RESULT_VARIABLE test_rc OUTPUT_VARIABLE test_out ERROR_VARIABLE test_err)
  endif()
  if(test_rc EQUAL 0)
    set(_tested 1)
    break()
  endif()
  if(NOT test_out MATCHES "Test not available without configuration")
    message(FATAL_ERROR "docs-only tests failed:\n${test_out}\n${test_err}")
  endif()
endforeach()
if(NOT _tested)
  message(FATAL_ERROR "docs-only tests failed in every configuration:\n${test_out}\n${test_err}")
endif()
message(STATUS "libjpl: docs-only matrix OK")
