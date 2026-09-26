# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# libjpl fixture integrity checker (test-only).
# Validates the committed fixture against manifest hashes. Variables:
# MANIFEST (tests/data/MANIFEST.txt path), BINARY_FIXTURE
# (de441_2000_2010.eph path), TEXT_FIXTURE (testpo_441_slice.txt path),
# ROWS_FIXTURE (fixture_rows.txt identity-list path), FIXTURE_TOOL (test-only
# C executable), WORK (owned canonical-copy directory). Manifest and the
# checker run on LF checkouts and CRLF checkouts because the reference
# text (and the identity list) is hashed after CRLF-to-LF normalization
# only; no other whitespace is discarded. Fails loudly on any mismatch.
# Manifest keys are parsed here from MANIFEST (see below), so the hashes
# live in exactly one place.
if(NOT DEFINED MANIFEST OR NOT DEFINED BINARY_FIXTURE OR NOT DEFINED TEXT_FIXTURE OR NOT DEFINED ROWS_FIXTURE
    OR NOT DEFINED WORK OR "${WORK}" STREQUAL "" OR NOT DEFINED FIXTURE_TOOL OR NOT EXISTS "${FIXTURE_TOOL}")
  message(FATAL_ERROR "check_fixture.cmake needs MANIFEST, BINARY_FIXTURE, TEXT_FIXTURE, ROWS_FIXTURE, FIXTURE_TOOL and WORK")
endif()
foreach(required IN ITEMS MANIFEST BINARY_FIXTURE TEXT_FIXTURE ROWS_FIXTURE)
  if(NOT EXISTS "${${required}}")
    message(FATAL_ERROR "check_fixture.cmake: ${required} does not exist: ${${required}}")
  endif()
endforeach()
file(SHA256 "${BINARY_FIXTURE}" actual_binary)
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${FIXTURE_TOOL}" canon-copy "${TEXT_FIXTURE}" "${WORK}/text.canonical"
  RESULT_VARIABLE text_rc ERROR_VARIABLE text_err)
if(NOT "${text_rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture text canonicalization failed: ${TEXT_FIXTURE}\n${text_err}")
endif()
execute_process(COMMAND "${FIXTURE_TOOL}" canon-copy "${ROWS_FIXTURE}" "${WORK}/rows.canonical"
  RESULT_VARIABLE rows_rc ERROR_VARIABLE rows_err)
if(NOT "${rows_rc}" STREQUAL "0")
  message(FATAL_ERROR "fixture rows canonicalization failed: ${ROWS_FIXTURE}\n${rows_err}")
endif()
file(SHA256 "${WORK}/text.canonical" actual_text)
file(SHA256 "${WORK}/rows.canonical" actual_rows)
file(STRINGS "${MANIFEST}" manifest_lines)
set(expected_binary "")
set(expected_text "")
set(expected_rows "")
foreach(manifest_line IN LISTS manifest_lines)
  if(manifest_line MATCHES "^fixture\\.binary\\.sha256=(.+)$")
    set(expected_binary "${CMAKE_MATCH_1}")
  elseif(manifest_line MATCHES "^fixture\\.text\\.sha256=(.+)$")
    set(expected_text "${CMAKE_MATCH_1}")
  elseif(manifest_line MATCHES "^fixture\\.rows\\.sha256=(.+)$")
    set(expected_rows "${CMAKE_MATCH_1}")
  endif()
endforeach()
if(expected_binary STREQUAL "" OR expected_text STREQUAL "" OR expected_rows STREQUAL "")
  message(FATAL_ERROR "check_fixture.cmake: manifest '${MANIFEST}' lacks fixture.*.sha256 keys")
endif()
if(NOT actual_binary STREQUAL expected_binary)
  message(FATAL_ERROR "Committed fixture integrity mismatch: binary ${BINARY_FIXTURE} hashes ${actual_binary}, expected ${expected_binary}")
endif()
if(NOT actual_text STREQUAL expected_text)
  message(FATAL_ERROR "Committed fixture integrity mismatch: text ${TEXT_FIXTURE} canonical hash ${actual_text}, expected ${expected_text}")
endif()
if(NOT actual_rows STREQUAL expected_rows)
  message(FATAL_ERROR "Committed fixture integrity mismatch: identity list ${ROWS_FIXTURE} canonical hash ${actual_rows}, expected ${expected_rows}")
endif()
message(STATUS "libjpl: fixture integrity OK (binary ${actual_binary}, text ${actual_text}, rows ${actual_rows})")
