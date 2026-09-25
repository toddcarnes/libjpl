# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Numerical-reference integrity checker (test-only, F16).
# Verifies canonical-LF SHA-256 values for tests/data/numerical_reference.h
# and tools/generate_numerical_reference.c against tests/data/MANIFEST.txt.
# Canonicalization: CRLF replaced with LF only; remaining bare CR fails.
# On failure emits expected vs actual values.
# Variables: MANIFEST (required manifest path), HEADER (required reference
# header path), GENERATOR (required generator source path), WORK (required
# build-tree work path for canonical copies).
if(NOT DEFINED MANIFEST OR "${MANIFEST}" STREQUAL "")
  message(FATAL_ERROR "check_numerical_reference.cmake needs MANIFEST")
endif()
if(NOT DEFINED HEADER OR "${HEADER}" STREQUAL "")
  message(FATAL_ERROR "check_numerical_reference.cmake needs HEADER")
endif()
if(NOT DEFINED GENERATOR OR "${GENERATOR}" STREQUAL "")
  message(FATAL_ERROR "check_numerical_reference.cmake needs GENERATOR")
endif()
if(NOT DEFINED WORK OR "${WORK}" STREQUAL "")
  message(FATAL_ERROR "check_numerical_reference.cmake needs WORK")
endif()
foreach(_f IN ITEMS "${MANIFEST}" "${HEADER}" "${GENERATOR}")
  if(NOT EXISTS "${_f}")
    message(FATAL_ERROR "numerical reference input does not exist: ${_f}")
  endif()
endforeach()
file(STRINGS "${MANIFEST}" _manifest_lines)
set(_ref_expected "")
set(_gen_expected "")
foreach(_line IN LISTS _manifest_lines)
  if(_line MATCHES "^numerical\\.reference\\.sha256=(.*)$")
    set(_ref_expected "${CMAKE_MATCH_1}")
  endif()
  if(_line MATCHES "^numerical\\.generator\\.sha256=(.*)$")
    set(_gen_expected "${CMAKE_MATCH_1}")
  endif()
endforeach()
if("${_ref_expected}" STREQUAL "")
  message(FATAL_ERROR "manifest lacks numerical.reference.sha256: ${MANIFEST}")
endif()
if("${_gen_expected}" STREQUAL "")
  message(FATAL_ERROR "manifest lacks numerical.generator.sha256: ${MANIFEST}")
endif()
file(MAKE_DIRECTORY "${WORK}")
# Canonicalize CRLF to LF with byte-exact replacement: read raw bytes in
# binary mode (file(READ) alone decodes newlines per-platform and would
# hash the decoded form, not the canonical source bytes). Python is
# available in this project's test tooling (fixture mutator); CMake has
# no binary-safe read primitive in 3.20.
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
  message(FATAL_ERROR "numerical reference check needs a Python3 interpreter for binary-safe canonicalization")
endif()
function(jpl_canonical_sha256 out_var src_path work_path)
  get_filename_component(_base "${work_path}" NAME)
  execute_process(COMMAND "${Python3_EXECUTABLE}"
      "-c" "import sys; d=open(sys.argv[1],'rb').read().replace(b'\\r\\n',b'\\n'); assert b'\\r' not in d, 'bare CR'; open(sys.argv[2],'wb').write(d)"
      "${src_path}" "${work_path}"
    RESULT_VARIABLE _canon_rc OUTPUT_VARIABLE _canon_out ERROR_VARIABLE _canon_err)
  if(NOT _canon_rc EQUAL 0)
    message(FATAL_ERROR "canonicalization failed for ${src_path}:\n${_canon_out}\n${_canon_err}")
  endif()
  file(SHA256 "${work_path}" _hash)
  set(${out_var} "${_hash}" PARENT_SCOPE)
endfunction()
jpl_canonical_sha256(_ref_actual "${HEADER}" "${WORK}/numerical_reference.h")
jpl_canonical_sha256(_gen_actual "${GENERATOR}" "${WORK}/generate_numerical_reference.c")
if(NOT _ref_actual STREQUAL _ref_expected)
  message(FATAL_ERROR
    "numerical reference hash mismatch: expected ${_ref_expected}, got ${_ref_actual} (${HEADER})")
endif()
if(NOT _gen_actual STREQUAL _gen_expected)
  message(FATAL_ERROR
    "numerical generator hash mismatch: expected ${_gen_expected}, got ${_gen_actual} (${GENERATOR})")
endif()
message(STATUS "libjpl: numerical reference integrity OK")
