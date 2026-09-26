# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

if(NOT DEFINED RUNNER OR NOT EXISTS "${RUNNER}" OR
   NOT DEFINED EPH OR NOT EXISTS "${EPH}" OR
   NOT DEFINED REF OR NOT EXISTS "${REF}" OR
   NOT DEFINED WORK OR "${WORK}" STREQUAL "")
  message(FATAL_ERROR "Horizons mutation check needs RUNNER, EPH, REF, WORK")
endif()
file(MAKE_DIRECTORY "${WORK}")
file(READ "${REF}" original)
string(REPLACE "222.447119268" "223.447119268" changed "${original}")
if(changed STREQUAL original)
  message(FATAL_ERROR "Horizons reference lacks the Moon RA anchor")
endif()
file(WRITE "${WORK}/changed_reference.txt" "${changed}")
execute_process(COMMAND "${RUNNER}" "${EPH}" "${WORK}/changed_reference.txt"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc EQUAL 0 OR NOT err MATCHES "Horizons body 10 TT")
  message(FATAL_ERROR "Horizons comparison did not reject shifted Moon RA:\n${out}\n${err}")
endif()
string(REPLACE "223.3148557" "224.3148557" changed_lon "${original}")
if(changed_lon STREQUAL original)
  message(FATAL_ERROR "Horizons reference lacks the Moon ecliptic anchor")
endif()
file(WRITE "${WORK}/changed_ecliptic.txt" "${changed_lon}")
execute_process(COMMAND "${RUNNER}" "${EPH}" "${WORK}/changed_ecliptic.txt"
  RESULT_VARIABLE lon_rc OUTPUT_VARIABLE lon_out ERROR_VARIABLE lon_err)
if(lon_rc EQUAL 0 OR NOT lon_err MATCHES "Horizons legacy ecliptic")
  message(FATAL_ERROR "Horizons comparison did not reject shifted Moon ecliptic longitude:\n${lon_out}\n${lon_err}")
endif()
message(STATUS "libjpl: independent apparent-position comparison detects changed RA and ecliptic longitude")
