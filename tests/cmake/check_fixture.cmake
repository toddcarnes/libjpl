# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# libjpl fixture integrity checker (test-only, owned by T17).
# Validates the committed fixture against manifest hashes. Variables:
# MANIFEST (tests/data/MANIFEST.txt path), BINARY_FIXTURE
# (de441_2000_2010.eph path), TEXT_FIXTURE (testpo_441_slice.txt path),
# ROWS_FIXTURE (fixture_rows.txt identity-list path). Manifest and the
# checker run on LF checkouts and CRLF checkouts because the reference
# text (and the identity list) is hashed after CRLF-to-LF normalization
# only; no other whitespace is discarded. Fails loudly on any mismatch.
# Manifest keys are parsed here from MANIFEST (see below), so the hashes
# live in exactly one place.
if(NOT DEFINED MANIFEST OR NOT DEFINED BINARY_FIXTURE OR NOT DEFINED TEXT_FIXTURE OR NOT DEFINED ROWS_FIXTURE)
  message(FATAL_ERROR "check_fixture.cmake needs MANIFEST, BINARY_FIXTURE, TEXT_FIXTURE and ROWS_FIXTURE")
endif()
foreach(required IN ITEMS MANIFEST BINARY_FIXTURE TEXT_FIXTURE ROWS_FIXTURE)
  if(NOT EXISTS "${${required}}")
    message(FATAL_ERROR "check_fixture.cmake: ${required} does not exist: ${${required}}")
  endif()
endforeach()
file(SHA256 "${BINARY_FIXTURE}" actual_binary)
file(READ "${TEXT_FIXTURE}" text)
string(REPLACE "\r\n" "\n" canonical "${text}")
string(SHA256 actual_text "${canonical}")
file(READ "${ROWS_FIXTURE}" rows_text)
string(REPLACE "\r\n" "\n" rows_canonical "${rows_text}")
string(SHA256 actual_rows "${rows_canonical}")
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
