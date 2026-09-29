# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# License inventory checker (test-only).
# Verifies the root MIT grant, every first-party notice enumerated in
# tests/license_inventory.cmake, sweep coverage so new eligible files
# cannot sit silently outside the inventory, and (optionally) installed
# license texts.
# Variables: SOURCE (required source tree under test),
# INVENTORY (optional inventory path; defaults to
# SOURCE/tests/license_inventory.cmake), INSTALL_PREFIX (optional
# installed prefix to verify), DATADIR (optional install data dir
# beneath the prefix; defaults to "share", matching the default
# CMAKE_INSTALL_DATADIR. CTest passes the configured data directory for
# installed-package checks rather than assuming "share").
if(NOT DEFINED SOURCE OR "${SOURCE}" STREQUAL "")
  message(FATAL_ERROR "check_licenses.cmake needs SOURCE")
endif()
if(NOT DEFINED INVENTORY OR "${INVENTORY}" STREQUAL "")
  set(INVENTORY "${SOURCE}/tests/license_inventory.cmake")
endif()
if(NOT EXISTS "${INVENTORY}")
  message(FATAL_ERROR "check_licenses.cmake: inventory does not exist: ${INVENTORY}")
endif()
if(NOT DEFINED DATADIR OR "${DATADIR}" STREQUAL "")
  set(DATADIR "share")
endif()
include("${INVENTORY}")

# 1. The complete MIT grant must exist at the root (not only an SPDX tag).
if(NOT EXISTS "${SOURCE}/LICENSE")
  message(FATAL_ERROR "Missing root license: ${SOURCE}/LICENSE")
endif()
file(READ "${SOURCE}/LICENSE" root_license)
foreach(required IN ITEMS "MIT License" "Copyright \\(c\\) 2026 Todd Carnes" "Permission is hereby granted")
  if(NOT root_license MATCHES "${required}")
    message(FATAL_ERROR "Root LICENSE is missing '${required}'")
  endif()
endforeach()

# 1b. The LICENSES.md project-license link must resolve inside the source
# root to an existing regular file (../LICENSE would escape the root).
# The link text uses backticks ([`LICENSE`](...)): capture group 2 holds
# the target (group 1 is the tick-quoted text). A later MATCH overwrites
# CMAKE_MATCH_*, so copy the target out immediately and use only the copy.
file(READ "${SOURCE}/LICENSES.md" _licenses_doc)
string(REGEX MATCH "\\[([^]]+)\\]\\(([^)]+)\\)" _license_link "${_licenses_doc}")
set(_license_target "${CMAKE_MATCH_2}")
if("${_license_target}" STREQUAL "")
  message(FATAL_ERROR "LICENSES.md carries no [`LICENSE`](...) project-license link")
endif()
get_filename_component(_license_resolved "${SOURCE}/${_license_target}" ABSOLUTE)
file(RELATIVE_PATH _license_rel "${SOURCE}" "${_license_resolved}")
if(_license_rel MATCHES "^\\.\\./" OR NOT EXISTS "${_license_resolved}" OR IS_DIRECTORY "${_license_resolved}")
  message(FATAL_ERROR "LICENSES.md project-license link '${_license_target}' does not resolve to a file inside the source root")
endif()

# 2. Every inventoried first-party file must exist and carry the notice.
foreach(rel IN LISTS JPL_LICENSE_FIRST_PARTY)
  if(NOT EXISTS "${SOURCE}/${rel}")
    message(FATAL_ERROR "Inventoried first-party file does not exist: ${rel}")
  endif()
  file(READ "${SOURCE}/${rel}" content)
  if(NOT content MATCHES "Copyright .*Todd Carnes" OR
     NOT content MATCHES "SPDX-License-Identifier: MIT")
    message(FATAL_ERROR "Missing first-party notice: ${rel}")
  endif()
endforeach()

# 3. No eligible file may sit silently outside the inventory.
# Sweep extensions cover first-party text/code plus workflow/config
# formats (yml/yaml/json/toml/ini/cfg/in); binary/tooling output stays
# exempt by directory prefix below, not per file.
file(GLOB_RECURSE candidates RELATIVE "${SOURCE}"
  "${SOURCE}/*.c" "${SOURCE}/*.h" "${SOURCE}/*.cmake" "${SOURCE}/*.py"
  "${SOURCE}/*.md" "${SOURCE}/*.txt"
  "${SOURCE}/*.yml" "${SOURCE}/*.yaml" "${SOURCE}/*.json"
  "${SOURCE}/*.toml" "${SOURCE}/*.ini" "${SOURCE}/*.cfg" "${SOURCE}/*.in"
  "${SOURCE}/CMakeLists.txt" "${SOURCE}/Doxyfile.in")
set(known ${JPL_LICENSE_FIRST_PARTY} ${JPL_LICENSE_EXEMPT_FILES})
set(unlisted "")
foreach(candidate IN LISTS candidates)
  string(REPLACE "\\" "/" candidate "${candidate}")
  if(candidate MATCHES "^(src/erfa/|docs/superpowers/|docs/reviews/|build/|build_*/|out/|\\.worktrees/|.git/|.superpowers/|docs/html/|docs/latex/|cmake-build-)" OR candidate MATCHES "/obj/" OR candidate MATCHES "/bin/")
    continue()
  endif()
  list(FIND known "${candidate}" found)
  if(found EQUAL -1)
    list(APPEND unlisted "${candidate}")
  endif()
endforeach()
if(unlisted)
  message(FATAL_ERROR "Eligible files outside the license inventory: ${unlisted}")
endif()

# 4. Installed notices, only when an install prefix is supplied.
if(DEFINED INSTALL_PREFIX AND NOT "${INSTALL_PREFIX}" STREQUAL "")
  set(notice_dir "${INSTALL_PREFIX}/${DATADIR}/licenses/libjpl")
  if(NOT EXISTS "${notice_dir}/LICENSE")
    message(FATAL_ERROR "Missing installed notice: ${notice_dir}/LICENSE")
  endif()
  file(READ "${notice_dir}/LICENSE" installed_mit)
  if(NOT installed_mit MATCHES "MIT License" OR
     NOT installed_mit MATCHES "Copyright .*Todd Carnes")
    message(FATAL_ERROR "Installed LICENSE is not the project MIT grant")
  endif()
  if(NOT EXISTS "${notice_dir}/ERFA-LICENSE")
    message(FATAL_ERROR "Missing installed notice: ${notice_dir}/ERFA-LICENSE")
  endif()
  file(READ "${notice_dir}/ERFA-LICENSE" installed_erfa)
  if(NOT installed_erfa MATCHES "NumFOCUS")
    message(FATAL_ERROR "Installed ERFA-LICENSE is not the ERFA grant")
  endif()
endif()

message(STATUS "libjpl: license check OK")
