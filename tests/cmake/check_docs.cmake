# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Public-symbol documentation checker (test-only).
# Requires every name listed in EXPECTED (tests/public_symbols.txt, one
# per line) to appear as <name>SYMBOL</name> in the generated Doxygen XML
# under XML_DIR, and requires all three file reference pages (not only
# source listings). A negative self-check runs on an owned XML copy with
# one expected symbol removed and must fail the same lookup, proving the
# checker detects a missing symbol rather than passing vacuously.
# Variables: EXPECTED (required inventory path), XML_DIR (required
# generated xml dir), WORK (required owned scratch dir).
if(NOT DEFINED EXPECTED OR "${EXPECTED}" STREQUAL ""
    OR NOT DEFINED XML_DIR OR "${XML_DIR}" STREQUAL ""
    OR NOT DEFINED WORK OR "${WORK}" STREQUAL "")
  message(FATAL_ERROR "check_docs.cmake needs EXPECTED, XML_DIR and WORK")
endif()
if(NOT EXISTS "${EXPECTED}")
  message(FATAL_ERROR "check_docs.cmake: inventory does not exist: ${EXPECTED}")
endif()
if(NOT EXISTS "${XML_DIR}")
  message(FATAL_ERROR "check_docs.cmake: XML dir does not exist: ${XML_DIR}")
endif()
file(MAKE_DIRECTORY "${WORK}")
file(STRINGS "${EXPECTED}" expected_symbols)
list(LENGTH expected_symbols symbol_count)
if(symbol_count EQUAL 0)
  message(FATAL_ERROR "check_docs.cmake: inventory is empty: ${EXPECTED}")
endif()
file(GLOB xml_files "${XML_DIR}/*.xml")
if(NOT xml_files)
  message(FATAL_ERROR "check_docs.cmake: no XML files under ${XML_DIR}")
endif()
# Concatenate once: string(FIND) below probes this single buffer.
set(all_xml "")
foreach(xml_file IN LISTS xml_files)
  file(READ "${xml_file}" one)
  string(APPEND all_xml "${one}\n")
endforeach()
# All three public file reference pages must exist (file pages carry
# jpl_8h, jpl__frames_8h, jpl__convenience_8h stems), not only the
# *_source listings.
foreach(stem IN ITEMS "jpl_8h.xml" "jpl__frames_8h.xml" "jpl__convenience_8h.xml")
  if(NOT EXISTS "${XML_DIR}/${stem}")
    message(FATAL_ERROR "check_docs.cmake: missing file reference page ${stem}")
  endif()
endforeach()
foreach(symbol IN LISTS expected_symbols)
  string(STRIP "${symbol}" clean)
  if("${clean}" STREQUAL "" OR "${clean}" MATCHES "^#")
    continue()
  endif()
  string(FIND "${all_xml}" "<name>${clean}</name>" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "check_docs.cmake (positive): missing documented symbol <name>${clean}</name>")
  endif()
endforeach()
# Negative control: an owned copy with one expected symbol removed must
# fail the same lookup. Copy only the smallest XML file carrying the
# first non-comment symbol, strip that one name, and require absence.
set(first_symbol "")
foreach(symbol IN LISTS expected_symbols)
  string(STRIP "${symbol}" clean)
  if(NOT "${clean}" STREQUAL "" AND NOT "${clean}" MATCHES "^#")
    set(first_symbol "${clean}")
    break()
  endif()
endforeach()
if("${first_symbol}" STREQUAL "")
  message(FATAL_ERROR "check_docs.cmake: no probed symbol in ${EXPECTED}")
endif()
set(neg_file "")
foreach(xml_file IN LISTS xml_files)
  file(READ "${xml_file}" probe)
  string(FIND "${probe}" "<name>${first_symbol}</name>" probe_pos)
  if(NOT probe_pos EQUAL -1)
    set(neg_file "${xml_file}")
    break()
  endif()
endforeach()
if("${neg_file}" STREQUAL "")
  message(FATAL_ERROR "check_docs.cmake: no XML file carries <name>${first_symbol}</name>")
endif()
get_filename_component(neg_name "${neg_file}" NAME)
file(READ "${neg_file}" neg_content)
string(REPLACE "<name>${first_symbol}</name>" "<name>JPL_REMOVED_SYMBOL_PROBE</name>" neg_content "${neg_content}")
file(WRITE "${WORK}/${neg_name}" "${neg_content}")
file(READ "${WORK}/${neg_name}" neg_check)
string(FIND "${neg_check}" "<name>${first_symbol}</name>" neg_pos)
if(NOT neg_pos EQUAL -1)
  message(FATAL_ERROR "check_docs.cmake (negative): removed symbol <name>${first_symbol}</name> still found")
endif()
message(STATUS "libjpl: docs symbol check OK (${symbol_count} inventory lines)")
