# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Negative validator driver (test-only, owned by T15).
# Requires the right failure (exit 1 + diagnostic category), not just any
# nonzero exit. Variables: RUNNER, FAULT_RUNNER (optional), EPH, TESTPO,
# EXPECT (regex matched against stderr, empty means success expected),
# SWEEP_DIR (for sweep mode), SWEEP_GOOD_DIR (control dir that must pass).
# EXPECT arrives as a bracket argument ([==[...]==]) so regex metacharacters
# (pipes, dots, stars) survive -D intact; strip the brackets here.
function(jpl_validator_expect out_var raw)
  if("${raw}" MATCHES "^\\[==\\[(.*)\\]==\\]$")
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  else()
    set(${out_var} "${raw}" PARENT_SCOPE)
  endif()
endfunction()
if(NOT DEFINED RUNNER OR NOT DEFINED EPH OR NOT DEFINED TESTPO)
  message(FATAL_ERROR "test_validators.cmake needs RUNNER, EPH and TESTPO")
endif()
if(NOT EXISTS "${RUNNER}")
  message(FATAL_ERROR "runner does not exist: ${RUNNER}")
endif()

# Consumes the corpus generated once per run by the CTest fixture setup test
# jpl_validator_data_setup (FIXTURES_SETUP jpl_validator_corpus); this driver
# never regenerates or truncates it (F01). Every case requires
# FIXTURES_REQUIRED jpl_validator_corpus via CMakeLists.txt. WORK only
# supplies paths. Fail immediately when the expected corpus file is absent,
# except the fixture read-error case (ROWS set), whose EPH/TESTPO/ROWS are
# committed source fixtures, not generated files.
if(DEFINED ROWS AND NOT "${ROWS}" STREQUAL "")
  # Source-fixture case: no generated corpus needed; validate sources exist.
elseif(DEFINED WORK)
  if(NOT EXISTS "${EPH}")
    message(FATAL_ERROR "validator corpus missing (run jpl_validator_data_setup first): ${EPH}")
  endif()
  if(NOT DEFINED SWEEP_DIR AND NOT EXISTS "${TESTPO}")
    message(FATAL_ERROR "validator corpus missing (run jpl_validator_data_setup first): ${TESTPO}")
  endif()
endif()

if(DEFINED SWEEP_DIR)
  # Sweep mode: the mixed directory must fail (corrupt DE102 file
  # present), while the good-only control directory must pass.
  execute_process(COMMAND "${RUNNER}" "${SWEEP_DIR}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT DEFINED SWEEP_GOOD_DIR)
    message(FATAL_ERROR "sweep mode needs SWEEP_GOOD_DIR")
  endif()
  if(NOT "${rc}" STREQUAL "1")
    message(FATAL_ERROR "sweep with corrupt file did not fail with exit 1 (got ${rc}):\n${out}\n${err}")
  endif()
  string(REPLACE "\n" " " err_flat "${err}")
  if(NOT "${err_flat}" MATCHES "failed to open|invalid header|could not decode|too short|cannot stat")
    message(FATAL_ERROR "sweep did not report a present-file failure: ${rc}\n${out}\n${err}")
  endif()
  execute_process(COMMAND "${RUNNER}" "${SWEEP_GOOD_DIR}"
                  RESULT_VARIABLE good_rc
                  OUTPUT_VARIABLE good_out
                  ERROR_VARIABLE good_err)
  if(NOT "${good_rc}" STREQUAL "0")
    message(FATAL_ERROR "good-only sweep failed: ${good_rc}\n${good_out}\n${good_err}")
  endif()
  return()
endif()

jpl_validator_expect(expect_clean "${EXPECT}")
# Runners taking an extra identity-list argument (fixture runner) receive
# it via ROWS; ordinary two-argument runners ignore the unset variable.
if("${expect_clean}" STREQUAL "")
  if(DEFINED ROWS AND NOT "${ROWS}" STREQUAL "")
    execute_process(COMMAND "${RUNNER}" "${EPH}" "${TESTPO}" "${ROWS}"
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  else()
    execute_process(COMMAND "${RUNNER}" "${EPH}" "${TESTPO}"
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  endif()
  if(NOT "${rc}" STREQUAL "0")
    message(FATAL_ERROR "expected success but got ${rc}\n${out}\n${err}")
  endif()
  return()
endif()

if(DEFINED FAULT_RUNNER AND NOT "${FAULT_RUNNER}" STREQUAL "")
  if("${expect_clean}" STREQUAL "read error")
    set(RUNNER "${FAULT_RUNNER}")
    if(NOT EXISTS "${RUNNER}")
      message(FATAL_ERROR "fault runner does not exist: ${RUNNER}")
    endif()
  endif()
endif()

if(DEFINED ROWS AND NOT "${ROWS}" STREQUAL "")
  execute_process(COMMAND "${RUNNER}" "${EPH}" "${TESTPO}" "${ROWS}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
else()
  execute_process(COMMAND "${RUNNER}" "${EPH}" "${TESTPO}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
endif()
if(NOT "${rc}" STREQUAL "1")
  message(FATAL_ERROR "expected exit 1 for '${expect_clean}' but got ${rc}\n${out}\n${err}")
endif()
string(REPLACE "\n" " " err_flat "${err}")
if(NOT "${err_flat}" MATCHES "${expect_clean}")
  message(FATAL_ERROR "expected diagnostic '${expect_clean}' but got:\n${out}\n${err}")
endif()
