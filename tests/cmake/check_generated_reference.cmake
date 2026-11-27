# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Deterministic generator reproduction check (test-only).
# Runs the independent ERFA-only reference generator and requires its
# canonical output to match the checked-in numerical reference byte for
# byte. Variables: GENERATOR, REFERENCE, FIXTURE_TOOL, WORK.
if(NOT DEFINED GENERATOR OR "${GENERATOR}" STREQUAL ""
    OR NOT DEFINED REFERENCE OR "${REFERENCE}" STREQUAL ""
    OR NOT DEFINED FIXTURE_TOOL OR "${FIXTURE_TOOL}" STREQUAL ""
    OR NOT DEFINED WORK OR "${WORK}" STREQUAL "")
  message(FATAL_ERROR "check_generated_reference.cmake needs GENERATOR, REFERENCE, FIXTURE_TOOL and WORK")
endif()
foreach(required IN ITEMS "${GENERATOR}" "${REFERENCE}" "${FIXTURE_TOOL}")
  if(NOT EXISTS "${required}")
    message(FATAL_ERROR "check_generated_reference.cmake: input does not exist: ${required}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${WORK}")
set(generated "${WORK}/numerical_reference.generated.h")
set(generated_lf "${WORK}/numerical_reference.generated.lf.h")
set(reference_lf "${WORK}/numerical_reference.reference.lf.h")
if(EXISTS "${generated}") 
  file(REMOVE "${generated}")
endif()
execute_process(COMMAND "${GENERATOR}" --output "${generated}"
  RESULT_VARIABLE gen_rc OUTPUT_VARIABLE gen_out ERROR_VARIABLE gen_err)
if(NOT gen_rc EQUAL 0)
  message(FATAL_ERROR "reference generator failed (${gen_rc}):\n${gen_out}\n${gen_err}")
endif()
execute_process(COMMAND "${FIXTURE_TOOL}" canon-copy "${generated}" "${generated_lf}"
  RESULT_VARIABLE canon_rc OUTPUT_VARIABLE canon_out ERROR_VARIABLE canon_err)
if(NOT canon_rc EQUAL 0)
  message(FATAL_ERROR "canonicalization failed for ${generated}:\n${canon_out}\n${canon_err}")
endif()
execute_process(COMMAND "${FIXTURE_TOOL}" canon-copy "${REFERENCE}" "${reference_lf}"
  RESULT_VARIABLE canon_rc OUTPUT_VARIABLE canon_out ERROR_VARIABLE canon_err)
if(NOT canon_rc EQUAL 0)
  message(FATAL_ERROR "canonicalization failed for ${REFERENCE}:\n${canon_out}\n${canon_err}")
endif()
file(SHA256 "${generated_lf}" generated_hash)
file(SHA256 "${reference_lf}" reference_hash)
if(NOT generated_hash STREQUAL reference_hash)
  message(FATAL_ERROR
    "generated numerical reference does not reproduce the checked-in header: "
    "generated ${generated_hash}, reference ${reference_hash}")
endif()
message(STATUS "libjpl: generated numerical reference reproduces ${reference_hash}")
