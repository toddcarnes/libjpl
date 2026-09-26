# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Check the *defined* symbols of an installed jpl archive. A raw byte scan
# would also match strings and undefined references.
if(NOT DEFINED ARCHIVE OR NOT EXISTS "${ARCHIVE}" OR
   NOT DEFINED SYMBOL_TOOL OR NOT EXISTS "${SYMBOL_TOOL}" OR
   NOT DEFINED TOOL_IS_DUMPBIN)
  message(FATAL_ERROR "check_archive_symbols.cmake needs ARCHIVE, SYMBOL_TOOL and TOOL_IS_DUMPBIN")
endif()
if(TOOL_IS_DUMPBIN)
  execute_process(COMMAND "${SYMBOL_TOOL}" /symbols "${ARCHIVE}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE symbols ERROR_VARIABLE err)
else()
  execute_process(COMMAND "${SYMBOL_TOOL}" -g "${ARCHIVE}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE symbols ERROR_VARIABLE err)
endif()
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "could not inspect ${ARCHIVE}: ${rc}\n${err}")
endif()
string(REPLACE "\n" ";" lines "${symbols}")
foreach(line IN LISTS lines)
  if(TOOL_IS_DUMPBIN)
    # UNDEF entries are references, not definitions; SECT entries are real.
    if(line MATCHES "SECT[0-9A-F]+.*External[ \t]+\\|[ \t]+_?jpl_test_[A-Za-z0-9_]+")
      message(FATAL_ERROR "installed archive ${ARCHIVE} defines a testing symbol: ${line}")
    endif()
  elseif(line MATCHES "[ \t][A-TV-Z][ \t]+_?jpl_test_[A-Za-z0-9_]+")
    # nm uses U for undefined symbols; uppercase other types are global
    # definitions on ELF and Mach-O.
    message(FATAL_ERROR "installed archive ${ARCHIVE} defines a testing symbol: ${line}")
  endif()
endforeach()
