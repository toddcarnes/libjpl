# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Negative fixture-integrity driver (test-only, owned by T17).
# Copies the committed fixture into the build tree and applies five
# disposable mutations there; the source fixture is never modified.
# Four mutation families must fail (wrong bytes must not verify):
# flipped binary byte, 119 interior rows replaced by duplicates
# (endpoints and total count preserved), altered target/component,
# altered coordinate. The fifth family (CRLF line endings) must still
# pass: canonicalization changes CRLF to LF only. The ordered identity
# list is checked inside the duplicate and retarget families (mutated
# rows files verified against the intact text), proving the list is
# actually compared; there is no separate sixth mutation.
# The mutator runs under a portable Python 3 interpreter (PYTHON
# variable when provided, else python3/python from PATH); a missing
# interpreter fails loudly as an explicit prerequisite.
# Variables: MANIFEST, BINARY_FIXTURE, TEXT_FIXTURE, ROWS_FIXTURE,
# CHECKER (check_fixture.cmake path), FIXTURE_RUNNER
# (jpl_fixture_tests binary), WORK (unique build-tree directory),
# PYTHON (optional Python 3 interpreter path).
# Failure reporting: each case records exit code plus stderr; the first
# failing expectation aborts with FATAL_ERROR naming the case.
function(jpl_fixture_case label expect_fail)
  execute_process(COMMAND ${ARGN}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(expect_fail)
    if(NOT "${rc}" STREQUAL "1")
      message(FATAL_ERROR "fixture negative case '${label}' did not fail with exit 1 (got ${rc}):\n${out}\n${err}")
    endif()
  else()
    if(NOT "${rc}" STREQUAL "0")
      message(FATAL_ERROR "fixture negative case '${label}' expected success but got ${rc}:\n${out}\n${err}")
    endif()
  endif()
endfunction()
if(NOT DEFINED MANIFEST OR NOT DEFINED BINARY_FIXTURE OR NOT DEFINED TEXT_FIXTURE OR NOT DEFINED ROWS_FIXTURE OR NOT DEFINED CHECKER OR NOT DEFINED FIXTURE_RUNNER OR NOT DEFINED WORK)
  message(FATAL_ERROR "test_fixture_integrity.cmake needs MANIFEST, BINARY_FIXTURE, TEXT_FIXTURE, ROWS_FIXTURE, CHECKER, FIXTURE_RUNNER and WORK")
endif()
foreach(required IN ITEMS MANIFEST BINARY_FIXTURE TEXT_FIXTURE ROWS_FIXTURE CHECKER FIXTURE_RUNNER)
  if(NOT EXISTS "${${required}}")
    message(FATAL_ERROR "test_fixture_integrity.cmake: ${required} does not exist: ${${required}}")
  endif()
endforeach()
# Portable Python 3 discovery: an explicit -DPYTHON=<path> wins; otherwise
# fall back to python3/python from PATH. Fails loudly when absent.
if(NOT DEFINED PYTHON OR "${PYTHON}" STREQUAL "")
  find_program(LIBJPL_FIXTURE_PYTHON NAMES python3 python)
  if(NOT LIBJPL_FIXTURE_PYTHON)
    message(FATAL_ERROR "test_fixture_integrity.cmake needs a Python 3 interpreter (pass -DPYTHON=<path> or put python3/python on PATH)")
  endif()
  set(PYTHON "${LIBJPL_FIXTURE_PYTHON}")
elseif(NOT EXISTS "${PYTHON}")
  message(FATAL_ERROR "test_fixture_integrity.cmake: PYTHON does not exist: ${PYTHON}")
endif()
# Build-tree scratch only; the committed source files stay untouched.
file(MAKE_DIRECTORY "${WORK}")
file(COPY_FILE "${BINARY_FIXTURE}" "${WORK}/case_binary.eph")
file(COPY_FILE "${TEXT_FIXTURE}" "${WORK}/case_text.txt")
file(COPY_FILE "${ROWS_FIXTURE}" "${WORK}/case_rows.txt")
# Sanity: the intact build-tree copies must pass the hash check (using a
# throwaway manifest holding exactly the real fixture.*.sha256 lines) and
# the numerical runner before any mutation is applied.
file(STRINGS "${MANIFEST}" intact_lines)
file(WRITE "${WORK}/intact_manifest.txt" "")
foreach(intact_line IN LISTS intact_lines)
  if(intact_line MATCHES "^fixture\\.(binary|text|rows)\\.sha256=")
    file(APPEND "${WORK}/intact_manifest.txt" "${intact_line}\n")
  endif()
endforeach()
jpl_fixture_case("intact copies pass" 0
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/case_text.txt"
  "-DROWS_FIXTURE=${WORK}/case_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("intact runner passes" 0
  "${FIXTURE_RUNNER}" "${WORK}/case_binary.eph" "${WORK}/case_text.txt" "${WORK}/case_rows.txt")

# Case 1: flip one binary byte in a data record. The hash check must fail.
# Offset 18855 is the high byte of an Earth-rotation (T12) coefficient
# inside data record 1 (which starts at 2*8144 = 16288; the LE double
# 32831107.1005468 at 18848 feeds row 1 at JD 2451544.5). Read/write via
# the PYTHON interpreter above so the script stays byte-exact across
# platforms.
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${BINARY_FIXTURE}" "${WORK}/flip.eph"
                RESULT_VARIABLE rc)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture negative setup failed to copy binary for flip case")
endif()
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/mutate_fixture.py" flip "${WORK}/flip.eph"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture flip mutation failed: ${rc}\n${out}\n${err}")
endif()
jpl_fixture_case("flipped binary byte fails" 1
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/flip.eph"
  "-DTEXT_FIXTURE=${WORK}/case_text.txt"
  "-DROWS_FIXTURE=${WORK}/case_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("flipped binary runner fails" 1
  "${FIXTURE_RUNNER}" "${WORK}/flip.eph" "${WORK}/case_text.txt" "${WORK}/case_rows.txt")

# Case 2: duplicate 119 interior rows, preserving endpoints and count 121.
# The identity-list hash (and, for the mutated text, the text hash too)
# must fail; the runner alone keeps passing, which is exactly why the
# ordered identity list exists.
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/mutate_fixture.py" dup "${TEXT_FIXTURE}" "${WORK}/dup.txt" "${ROWS_FIXTURE}" "${WORK}/dup_rows.txt"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture dup mutation failed: ${rc}\n${out}\n${err}")
endif()
jpl_fixture_case("duplicated interior rows fail text hash" 1
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/dup.txt"
  "-DROWS_FIXTURE=${WORK}/case_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("duplicated interior rows fail rows hash" 1
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/case_text.txt"
  "-DROWS_FIXTURE=${WORK}/dup_rows.txt"
  -P "${CHECKER}")

# Case 3: alter one target/component pair (row 61: T1 C9 -> T2 C9).
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/mutate_fixture.py" retarget "${TEXT_FIXTURE}" "${WORK}/retarget.txt" "${ROWS_FIXTURE}" "${WORK}/retarget_rows.txt"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture retarget mutation failed: ${rc}\n${out}\n${err}")
endif()
jpl_fixture_case("altered target fails text hash" 1
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/retarget.txt"
  "-DROWS_FIXTURE=${WORK}/case_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("altered target fails rows hash" 1
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/case_text.txt"
  "-DROWS_FIXTURE=${WORK}/retarget_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("altered target runner fails" 1
  "${FIXTURE_RUNNER}" "${WORK}/case_binary.eph" "${WORK}/retarget.txt" "${WORK}/case_rows.txt")

# Case 4: alter one coordinate (row 90: fractional digit 6 -> 7, +1e-9,
# far above the 5e-13 reference tolerance).
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/mutate_fixture.py" recoord "${TEXT_FIXTURE}" "${WORK}/recoord.txt"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture recoord mutation failed: ${rc}\n${out}\n${err}")
endif()
jpl_fixture_case("altered coordinate fails text hash" 1
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/recoord.txt"
  "-DROWS_FIXTURE=${WORK}/case_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("altered coordinate runner fails" 1
  "${FIXTURE_RUNNER}" "${WORK}/case_binary.eph" "${WORK}/recoord.txt" "${WORK}/case_rows.txt")

# Case 5: CRLF line endings must still pass (canonicalization is LF-only).
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/mutate_fixture.py" crlf "${TEXT_FIXTURE}" "${WORK}/crlf.txt" "${ROWS_FIXTURE}" "${WORK}/crlf_rows.txt"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture crlf mutation failed: ${rc}\n${out}\n${err}")
endif()
jpl_fixture_case("CRLF text still passes" 0
  "${CMAKE_COMMAND}"
  "-DMANIFEST=${WORK}/intact_manifest.txt"
  "-DBINARY_FIXTURE=${WORK}/case_binary.eph"
  "-DTEXT_FIXTURE=${WORK}/crlf.txt"
  "-DROWS_FIXTURE=${WORK}/crlf_rows.txt"
  -P "${CHECKER}")
jpl_fixture_case("CRLF runner still passes" 0
  "${FIXTURE_RUNNER}" "${WORK}/case_binary.eph" "${WORK}/crlf.txt" "${WORK}/crlf_rows.txt")
