# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Installed-package driver (test-only, owned by T19).
# Installs the already-built primary configuration to unique build-tree
# prefixes and verifies separate C/C++ consumers against them: default
# paths with an exact-version match, a deliberately incompatible version
# (must fail for the version reason), relocation to a prefix containing
# spaces, custom include/lib destinations from a tests-OFF child library
# build, and PIC ON/OFF static consumption (plus a shared-library link of
# the PIC archive on POSIX). Installed headers, license notices, and
# exports are inspected; no source/build checkout paths may leak into
# installed interfaces, and no m.lib may appear in Visual Studio exports.
# Variables: SOURCE (required source root), BUILD (required primary
# binary dir), CONFIG (required install configuration, e.g. Debug),
# GENERATOR (required CMake generator for child configures), VERSION
# (required exact project version), PLATFORM/TOOLSET/TOOLCHAIN_FILE/
# C_COMPILER/CXX_COMPILER/BUILD_TYPE (optional child-configure
# forwarding; empty means omit), LIBDIR/INCLUDEDIR/DATADIR (optional
# install-layout names; default lib/include/share), NEED_M (optional "m"
# when the toolchain needs libm), PIC_IS_ON (optional 1/0; gates the
# POSIX shared-link check), CHECKER/INVENTORY (optional license checker
# plus ownership inventory for installed-notice verification).
# Failure reporting: each case records exit code plus stdout/stderr; the
# first failing expectation aborts with FATAL_ERROR naming the case.
function(run_checked)
  execute_process(COMMAND ${ARGV}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT "${rc}" STREQUAL "0")
    message(FATAL_ERROR "install case failed (${rc}): ${ARGV}\n${out}\n${err}")
  endif()
endfunction()

function(check_archive_symbols libdir)
  set(lib_msvc "${libdir}/jpl.lib")
  set(lib_posix "${libdir}/libjpl.a")
  if(EXISTS "${lib_msvc}" AND NOT EXISTS "${lib_posix}")
    set(archive "${lib_msvc}")
  elseif(EXISTS "${lib_posix}" AND NOT EXISTS "${lib_msvc}")
    set(archive "${lib_posix}")
  else()
    message(FATAL_ERROR "expected exactly one installed jpl archive in ${libdir}")
  endif()
  run_checked("${CMAKE_COMMAND}"
    "-DARCHIVE=${archive}" "-DSYMBOL_TOOL=${SYMBOL_TOOL}"
    "-DTOOL_IS_DUMPBIN=${TOOL_IS_DUMPBIN}"
    -P "${SOURCE}/tests/cmake/check_archive_symbols.cmake")
endfunction()
if(NOT DEFINED SOURCE OR "${SOURCE}" STREQUAL ""
    OR NOT DEFINED BUILD OR "${BUILD}" STREQUAL ""
    OR NOT DEFINED CONFIG OR "${CONFIG}" STREQUAL ""
    OR NOT DEFINED GENERATOR OR "${GENERATOR}" STREQUAL ""
    OR NOT DEFINED VERSION OR "${VERSION}" STREQUAL "")
  message(FATAL_ERROR "test_install.cmake needs SOURCE, BUILD, CONFIG, GENERATOR and VERSION")
endif()
foreach(required IN ITEMS SOURCE BUILD)
  if(NOT EXISTS "${${required}}")
    message(FATAL_ERROR "test_install.cmake: ${required} does not exist: ${${required}}")
  endif()
endforeach()
if(NOT EXISTS "${SOURCE}/tests/consumer/CMakeLists.txt"
    OR NOT EXISTS "${SOURCE}/tests/consumer/main.c"
    OR NOT EXISTS "${SOURCE}/tests/consumer/main.cpp")
  message(FATAL_ERROR "test_install.cmake: consumer sources missing under ${SOURCE}/tests/consumer")
endif()
if(NOT DEFINED LIBDIR OR "${LIBDIR}" STREQUAL "")
  set(LIBDIR "lib")
endif()
if(NOT DEFINED INCLUDEDIR OR "${INCLUDEDIR}" STREQUAL "")
  set(INCLUDEDIR "include")
endif()
if(NOT DEFINED DATADIR OR "${DATADIR}" STREQUAL "")
  set(DATADIR "share")
endif()

# Unique owned work tree beneath the primary binary dir; never delete an
# unknown existing tree, and never touch maintainer-local ephemerides.
string(TIMESTAMP stamp "%Y%m%dT%H%M%S" UTC)
string(RANDOM LENGTH 6 rnd)
set(WORK "${BUILD}/test-work/install-${CONFIG}-${stamp}-${rnd}")
file(MAKE_DIRECTORY "${WORK}")

# Child-configure forwarding, shared by consumer and library builds.
# Generator platform/toolset/toolchain/compilers stay consistent with the
# primary configuration; empty values are omitted.
set(child_gen_args "")
list(APPEND child_gen_args -G "${GENERATOR}")
if(DEFINED PLATFORM AND NOT "${PLATFORM}" STREQUAL "")
  list(APPEND child_gen_args -A "${PLATFORM}")
endif()
if(DEFINED TOOLSET AND NOT "${TOOLSET}" STREQUAL "")
  list(APPEND child_gen_args -T "${TOOLSET}")
endif()
set(child_toolchain_args "")
if(DEFINED TOOLCHAIN_FILE AND NOT "${TOOLCHAIN_FILE}" STREQUAL "")
  list(APPEND child_toolchain_args "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
endif()
if(DEFINED C_COMPILER AND NOT "${C_COMPILER}" STREQUAL "")
  list(APPEND child_toolchain_args "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(DEFINED CXX_COMPILER AND NOT "${CXX_COMPILER}" STREQUAL "")
  list(APPEND child_toolchain_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
if(DEFINED BUILD_TYPE AND NOT "${BUILD_TYPE}" STREQUAL "")
  list(APPEND child_toolchain_args "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
endif()

# --- Case 1: install the already-built primary configuration. ---
set(PREFIX "${WORK}/prefix")
run_checked("${CMAKE_COMMAND}" --install "${BUILD}"
  --prefix "${PREFIX}" --config "${CONFIG}")

# --- Case 2: inspect the default install (headers, notices, exports). ---
file(GLOB installed_headers RELATIVE "${PREFIX}/${INCLUDEDIR}" "${PREFIX}/${INCLUDEDIR}/*.h")
list(SORT installed_headers)
if(NOT installed_headers STREQUAL "jpl.h;jpl_convenience.h;jpl_frames.h")
  message(FATAL_ERROR "installed headers are not exactly the three public headers: ${installed_headers}")
endif()
if(DEFINED CHECKER AND NOT "${CHECKER}" STREQUAL ""
    AND DEFINED INVENTORY AND NOT "${INVENTORY}" STREQUAL "")
  run_checked("${CMAKE_COMMAND}"
    "-DSOURCE=${SOURCE}" "-DINVENTORY=${INVENTORY}"
    "-DINSTALL_PREFIX=${PREFIX}" "-DDATADIR=${DATADIR}" -P "${CHECKER}")
else()
  message(FATAL_ERROR "test_install.cmake needs CHECKER and INVENTORY for installed-notice verification")
endif()
set(config_dir "${PREFIX}/${LIBDIR}/cmake/libjpl")
foreach(cfg_file IN ITEMS libjplConfig.cmake libjplConfigVersion.cmake libjplTargets.cmake)
  if(NOT EXISTS "${config_dir}/${cfg_file}")
    message(FATAL_ERROR "missing installed package file: ${config_dir}/${cfg_file}")
  endif()
endforeach()
file(READ "${config_dir}/libjplConfigVersion.cmake" version_content)
string(REPLACE "." "\\." escaped_version "${VERSION}")
if(NOT version_content MATCHES "${escaped_version}")
  message(FATAL_ERROR "installed version file does not mention ${VERSION}")
endif()
if(NOT version_content MATCHES "ExactVersion")
  message(FATAL_ERROR "installed version file is not exact-version compatibility")
endif()
file(READ "${config_dir}/libjplConfig.cmake" config_content)
foreach(leak IN ITEMS "${SOURCE}" "${BUILD}")
  string(FIND "${config_content}" "${leak}" leak_pos)
  if(NOT leak_pos EQUAL -1)
    message(FATAL_ERROR "installed config leaks a checkout path (${leak}): ${config_dir}/libjplConfig.cmake")
  endif()
endforeach()
file(GLOB export_files "${config_dir}/libjplTargets*.cmake")
foreach(export_file IN LISTS export_files)
  file(READ "${export_file}" export_content)
  string(FIND "${export_content}" "${SOURCE}" checkout_pos)
  if(NOT checkout_pos EQUAL -1)
    message(FATAL_ERROR "installed export leaks the source checkout: ${export_file}")
  endif()
  string(FIND "${export_content}" "${BUILD}" build_pos)
  if(NOT build_pos EQUAL -1)
    message(FATAL_ERROR "installed export leaks the build tree: ${export_file}")
  endif()
  if(GENERATOR MATCHES "Visual Studio")
    string(FIND "${export_content}" "m.lib" mlib_pos)
    if(NOT mlib_pos EQUAL -1)
      message(FATAL_ERROR "installed export references m.lib on a Visual Studio generator: ${export_file}")
    endif()
  endif()
endforeach()
file(READ "${config_dir}/libjplTargets.cmake" targets_content)
if(NOT targets_content MATCHES "INTERFACE_INCLUDE_DIRECTORIES")
  message(FATAL_ERROR "installed targets export carries no include interface")
endif()
check_archive_symbols("${PREFIX}/${LIBDIR}")

# --- Case 3: default-prefix consumer, exact current version. ---
# Search roots are plain prefixes: find_package appends lib/cmake etc.
# itself. A custom LIBDIR layout needs libjpl_DIR pointed at the exact
# config directory instead.
set(default_prefix_path "${PREFIX}")
set(default_libjpl_dir "")
if(NOT "${LIBDIR}" STREQUAL "lib")
  set(default_libjpl_dir "${PREFIX}/${LIBDIR}/cmake/libjpl")
endif()
run_checked("${CMAKE_COMMAND}" -S "${SOURCE}/tests/consumer" -B "${WORK}/consumer-default"
  ${child_gen_args} ${child_toolchain_args}
  "-DCMAKE_PREFIX_PATH=${default_prefix_path}" "-Dlibjpl_DIR=${default_libjpl_dir}" "-DREQUEST_VERSION=${VERSION}")
run_checked("${CMAKE_COMMAND}" --build "${WORK}/consumer-default"
  --config "${CONFIG}" --parallel)
run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}/consumer-default"
  -C "${CONFIG}" --output-on-failure)

# --- Case 4: deliberately incompatible versions fail for the version reason.
# Exact 0.2.0 and 0.3.1 requests must both fail (F09: the 0.3.0 breaking API
# is distinct from tagged 0.2.0 and from any newer version). ---
foreach(_bad_version IN ITEMS 0.2.0 0.3.1 9999.0.0)
  string(REPLACE "." "_" _bad_tag "${_bad_version}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}/tests/consumer" -B "${WORK}/consumer-badversion-${_bad_tag}"
      ${child_gen_args} ${child_toolchain_args}
      "-DCMAKE_PREFIX_PATH=${default_prefix_path}" "-Dlibjpl_DIR=${default_libjpl_dir}" "-DREQUEST_VERSION=${_bad_version}"
    RESULT_VARIABLE bad_rc OUTPUT_VARIABLE bad_out ERROR_VARIABLE bad_err)
  if("${bad_rc}" STREQUAL "0")
    message(FATAL_ERROR "incompatible version request ${_bad_version} unexpectedly configured")
  endif()
  string(REPLACE "\n" " " bad_flat "${bad_out} ${bad_err}")
  if(NOT bad_flat MATCHES "not compatible with the version requested|exactly matches requested version")
    message(FATAL_ERROR "incompatible version ${_bad_version} failed for the wrong reason (expected a version diagnostic):\n${bad_out}\n${bad_err}")
  endif()
endforeach()

# --- Case 5: relocate the owned prefix to a path with spaces. ---
set(SPACE_PREFIX "${WORK}/relocated prefix")
file(RENAME "${PREFIX}" "${SPACE_PREFIX}")
if(EXISTS "${PREFIX}")
  message(FATAL_ERROR "relocated prefix still exists at the old path: ${PREFIX}")
endif()
if(NOT EXISTS "${SPACE_PREFIX}/${LIBDIR}/cmake/libjpl/libjplConfig.cmake")
  message(FATAL_ERROR "relocated prefix lost its package config: ${SPACE_PREFIX}")
endif()
# Recompute the exact config dir under the relocated prefix: the pre-move
# default_libjpl_dir still names the old PREFIX, which no longer exists.
set(space_libjpl_dir "")
if(NOT "${LIBDIR}" STREQUAL "lib")
  set(space_libjpl_dir "${SPACE_PREFIX}/${LIBDIR}/cmake/libjpl")
endif()
run_checked("${CMAKE_COMMAND}" -S "${SOURCE}/tests/consumer" -B "${WORK}/consumer-spaces"
  ${child_gen_args} ${child_toolchain_args}
  "-DCMAKE_PREFIX_PATH=${SPACE_PREFIX}" "-Dlibjpl_DIR=${space_libjpl_dir}" "-DREQUEST_VERSION=${VERSION}")
run_checked("${CMAKE_COMMAND}" --build "${WORK}/consumer-spaces"
  --config "${CONFIG}" --parallel)
run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}/consumer-spaces"
  -C "${CONFIG}" --output-on-failure)

# --- Case 6: custom include/lib destinations from a tests-OFF child build. ---
# A child library build (never the primary cache) proves nondefault
# GNUInstallDirs destinations flow into installed interfaces.
run_checked("${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK}/lib-custom"
  ${child_gen_args} ${child_toolchain_args}
  "-DLIBJPL_BUILD_TESTS=OFF"
  "-DCMAKE_INSTALL_INCLUDEDIR=headers/jpl"
  "-DCMAKE_INSTALL_LIBDIR=custom-lib")
run_checked("${CMAKE_COMMAND}" --build "${WORK}/lib-custom"
  --config "${CONFIG}" --parallel)
set(CUSTOM_PREFIX "${WORK}/custom prefix")
run_checked("${CMAKE_COMMAND}" --install "${WORK}/lib-custom"
  --prefix "${CUSTOM_PREFIX}" --config "${CONFIG}")
check_archive_symbols("${CUSTOM_PREFIX}/custom-lib")
file(GLOB custom_headers RELATIVE "${CUSTOM_PREFIX}/headers/jpl" "${CUSTOM_PREFIX}/headers/jpl/*.h")
list(SORT custom_headers)
if(NOT custom_headers STREQUAL "jpl.h;jpl_convenience.h;jpl_frames.h")
  message(FATAL_ERROR "custom-destination headers are not exactly the three public headers: ${custom_headers}")
endif()
if(NOT EXISTS "${CUSTOM_PREFIX}/custom-lib/cmake/libjpl/libjplConfig.cmake")
  message(FATAL_ERROR "custom-destination package config missing under custom-lib/cmake/libjpl")
endif()
file(READ "${CUSTOM_PREFIX}/custom-lib/cmake/libjpl/libjplTargets.cmake" custom_targets)
string(FIND "${custom_targets}" "headers/jpl" custom_inc_pos)
if(custom_inc_pos EQUAL -1)
  message(FATAL_ERROR "custom-destination export does not reference headers/jpl")
endif()
if(DEFINED CHECKER AND NOT "${CHECKER}" STREQUAL "")
  run_checked("${CMAKE_COMMAND}"
    "-DSOURCE=${SOURCE}" "-DINVENTORY=${INVENTORY}"
    "-DINSTALL_PREFIX=${CUSTOM_PREFIX}" "-DDATADIR=${DATADIR}" -P "${CHECKER}")
endif()
run_checked("${CMAKE_COMMAND}" -S "${SOURCE}/tests/consumer" -B "${WORK}/consumer-custom"
  ${child_gen_args} ${child_toolchain_args}
  "-Dlibjpl_DIR=${CUSTOM_PREFIX}/custom-lib/cmake/libjpl" "-DREQUEST_VERSION=${VERSION}")
run_checked("${CMAKE_COMMAND}" --build "${WORK}/consumer-custom"
  --config "${CONFIG}" --parallel)
run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}/consumer-custom"
  -C "${CONFIG}" --output-on-failure)

# --- Case 7: PIC OFF static consumption. ---
run_checked("${CMAKE_COMMAND}" -S "${SOURCE}" -B "${WORK}/lib-nopic"
  ${child_gen_args} ${child_toolchain_args}
  "-DLIBJPL_BUILD_TESTS=OFF" "-DLIBJPL_PIC=OFF")
run_checked("${CMAKE_COMMAND}" --build "${WORK}/lib-nopic"
  --config "${CONFIG}" --parallel)
set(NOPIC_PREFIX "${WORK}/nopic-prefix")
run_checked("${CMAKE_COMMAND}" --install "${WORK}/lib-nopic"
  --prefix "${NOPIC_PREFIX}" --config "${CONFIG}")
run_checked("${CMAKE_COMMAND}" -S "${SOURCE}/tests/consumer" -B "${WORK}/consumer-nopic"
  ${child_gen_args} ${child_toolchain_args}
  "-DCMAKE_PREFIX_PATH=${NOPIC_PREFIX}" "-DREQUEST_VERSION=${VERSION}")
run_checked("${CMAKE_COMMAND}" --build "${WORK}/consumer-nopic"
  --config "${CONFIG}" --parallel)
run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}/consumer-nopic"
  -C "${CONFIG}" --output-on-failure)

# --- Case 8 (POSIX only): the PIC static archive links into a shared object. ---
if(UNIX AND DEFINED PIC_IS_ON AND "${PIC_IS_ON}")
  if(NOT DEFINED C_COMPILER OR "${C_COMPILER}" STREQUAL "")
    message(FATAL_ERROR "test_install.cmake needs C_COMPILER for the POSIX shared-link check")
  endif()
  if(APPLE)
    set(shared_suffix ".dylib")
  else()
    set(shared_suffix ".so")
  endif()
  set(shared_args "${C_COMPILER}" -I "${SPACE_PREFIX}/${INCLUDEDIR}"
    "${SOURCE}/tests/consumer/main.c" "${SPACE_PREFIX}/${LIBDIR}/libjpl.a")
  if(DEFINED NEED_M AND NOT "${NEED_M}" STREQUAL "")
    list(APPEND shared_args "-l${NEED_M}")
  endif()
  list(APPEND shared_args -shared -o "${WORK}/libinstallcheck${shared_suffix}")
  execute_process(COMMAND ${shared_args}
    RESULT_VARIABLE shared_rc OUTPUT_VARIABLE shared_out ERROR_VARIABLE shared_err)
  if(NOT "${shared_rc}" STREQUAL "0")
    message(FATAL_ERROR "PIC static archive did not link into a shared object:\n${shared_out}\n${shared_err}")
  endif()
endif()

message(STATUS "libjpl: install verification OK")
