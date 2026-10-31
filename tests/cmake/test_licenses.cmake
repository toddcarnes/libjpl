# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# License test driver (test-only).
# Passes the intact source and a staged install, then requires failure
# for a missing root license, a stripped SPDX header, and a missing
# installed ERFA notice. An ignored nested worktree must pass. All mutations
# use disposable build-tree copies;
# the source tree is never modified.
# Variables: SOURCE (required source root), CHECKER (required
# check_licenses.cmake path), INVENTORY (required inventory path),
# WORK (required build-tree scratch dir), DATADIR (optional install
# data dir beneath the prefix; defaults to "share", matching the
# default CMAKE_INSTALL_DATADIR. CTest forwards the configured value,
# rather than assuming "share").
function(jpl_license_case label expect_fail)
  execute_process(COMMAND ${CMAKE_COMMAND} ${ARGN}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(expect_fail)
    if(NOT rc EQUAL 1)
      message(FATAL_ERROR "license case '${label}' did not fail with exit 1 (got ${rc}):\n${out}\n${err}")
    endif()
  else()
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "license case '${label}' expected success but got ${rc}:\n${out}\n${err}")
    endif()
  endif()
endfunction()
if(NOT DEFINED SOURCE OR NOT DEFINED CHECKER OR NOT DEFINED INVENTORY OR NOT DEFINED WORK)
  message(FATAL_ERROR "test_licenses.cmake needs SOURCE, CHECKER, INVENTORY and WORK")
endif()
if(NOT DEFINED DATADIR OR "${DATADIR}" STREQUAL "")
  set(DATADIR "share")
endif()
foreach(required IN ITEMS SOURCE CHECKER INVENTORY)
  if(NOT EXISTS "${${required}}")
    message(FATAL_ERROR "test_licenses.cmake: ${required} does not exist: ${${required}}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${WORK}")

# Positive: the intact source tree passes with no install check.
jpl_license_case("intact source passes" 0
  "-DSOURCE=${SOURCE}" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Positive: an ignored project-local worktree is not first-party source.
# Build the smallest complete source copy required by the inventory check;
# the nested file deliberately has no copyright notice.
include("${INVENTORY}")
set(worktree_source "${WORK}/source-with-worktree")
file(REMOVE_RECURSE "${worktree_source}")
foreach(rel IN LISTS JPL_LICENSE_FIRST_PARTY)
  get_filename_component(rel_dir "${rel}" DIRECTORY)
  file(MAKE_DIRECTORY "${worktree_source}/${rel_dir}")
  file(COPY_FILE "${SOURCE}/${rel}" "${worktree_source}/${rel}")
endforeach()
file(COPY_FILE "${SOURCE}/LICENSE" "${worktree_source}/LICENSE")
file(MAKE_DIRECTORY "${worktree_source}/.worktrees/linked/build")
file(WRITE "${worktree_source}/.worktrees/linked/build/unlisted.c" "int generated;\n")
jpl_license_case("nested git worktree is ignored" 0
  "-DSOURCE=${worktree_source}" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Build a minimal source copy from the inventory so the sweep sees no
# unlisted files unless a case deliberately adds one.
function(jpl_license_source_copy dest)
  foreach(rel IN LISTS JPL_LICENSE_FIRST_PARTY)
    get_filename_component(rel_dir "${rel}" DIRECTORY)
    file(MAKE_DIRECTORY "${dest}/${rel_dir}")
    file(COPY_FILE "${SOURCE}/${rel}" "${dest}/${rel}")
  endforeach()
  file(COPY_FILE "${SOURCE}/LICENSE" "${dest}/LICENSE")
endfunction()

# Positive: a conventional build directory name is exempt before it is
# even configured (name-based exemption).
set(build_source "${WORK}/source-with-build")
file(REMOVE_RECURSE "${build_source}")
jpl_license_source_copy("${build_source}")
file(MAKE_DIRECTORY "${build_source}/build_review")
file(WRITE "${build_source}/build_review/generated.c" "int generated;\n")
jpl_license_case("conventional build dir is ignored" 0
  "-DSOURCE=${build_source}" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Positive: any configured build tree is exempt by marker, not by name.
file(MAKE_DIRECTORY "${build_source}/mybuild")
file(WRITE "${build_source}/mybuild/CMakeCache.txt" "# generated\n")
file(WRITE "${build_source}/mybuild/generated.c" "int generated;\n")
jpl_license_case("configured build dir of any name is ignored" 0
  "-DSOURCE=${build_source}" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Negative: the same shape without a cache marker is still source.
file(MAKE_DIRECTORY "${build_source}/plain")
file(WRITE "${build_source}/plain/unlisted.c" "int generated;\n")
jpl_license_case("plain directory is still swept" 1
  "-DSOURCE=${build_source}" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Positive: a staged prefix holding both complete texts passes.
file(MAKE_DIRECTORY "${WORK}/stage/${DATADIR}/licenses/libjpl")
file(COPY_FILE "${SOURCE}/LICENSE" "${WORK}/stage/${DATADIR}/licenses/libjpl/LICENSE")
file(COPY_FILE "${SOURCE}/src/erfa/LICENSE" "${WORK}/stage/${DATADIR}/licenses/libjpl/ERFA-LICENSE")
jpl_license_case("staged install passes" 0
  "-DSOURCE=${SOURCE}" "-DINVENTORY=${INVENTORY}"
  "-DINSTALL_PREFIX=${WORK}/stage" "-DDATADIR=${DATADIR}" -P "${CHECKER}")

# Negative: a tree without a root license must fail.
file(MAKE_DIRECTORY "${WORK}/empty")
jpl_license_case("missing root license fails" 1
  "-DSOURCE=${WORK}/empty" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Negative: a build-tree copy with the SPDX header stripped must fail.
# Only inventoried files are copied (preserving relative paths), so the
# sweep sees no unlisted files and the failure names src/jpl.c exactly.
file(REMOVE_RECURSE "${WORK}/neg")
include("${INVENTORY}")
foreach(rel IN LISTS JPL_LICENSE_FIRST_PARTY)
  get_filename_component(rel_dir "${rel}" DIRECTORY)
  file(MAKE_DIRECTORY "${WORK}/neg/${rel_dir}")
  file(COPY_FILE "${SOURCE}/${rel}" "${WORK}/neg/${rel}")
endforeach()
file(COPY_FILE "${SOURCE}/LICENSE" "${WORK}/neg/LICENSE")
file(READ "${WORK}/neg/src/jpl.c" stripped)
string(REPLACE "SPDX-License-Identifier: MIT" "SPDX-License-Identifier: NONE" stripped "${stripped}")
file(WRITE "${WORK}/neg/src/jpl.c" "${stripped}")
jpl_license_case("stripped SPDX header fails" 1
  "-DSOURCE=${WORK}/neg" "-DINVENTORY=${INVENTORY}" -P "${CHECKER}")

# Negative: a staged prefix without the ERFA notice must fail.
file(MAKE_DIRECTORY "${WORK}/stage_noerfa/${DATADIR}/licenses/libjpl")
file(COPY_FILE "${SOURCE}/LICENSE" "${WORK}/stage_noerfa/${DATADIR}/licenses/libjpl/LICENSE")
jpl_license_case("missing installed ERFA notice fails" 1
  "-DSOURCE=${SOURCE}" "-DINVENTORY=${INVENTORY}"
  "-DINSTALL_PREFIX=${WORK}/stage_noerfa" "-DDATADIR=${DATADIR}" -P "${CHECKER}")
