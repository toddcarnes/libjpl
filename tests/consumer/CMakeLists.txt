# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Separate installed-package consumer (test-only).
# Configures against an installed prefix via CMAKE_PREFIX_PATH, never
# against the source/build checkout. Builds both a C and a C++ executable
# linking jpl::jpl so C/C++ inclusion and static math/ERFA linkage are
# exercised outside the library build.
cmake_minimum_required(VERSION 3.20)
project(libjpl_consumer LANGUAGES C CXX)
# libjpl_DIR (exact config directory, needed for nonstandard install
# layouts) wins over CMAKE_PREFIX_PATH; REQUEST_VERSION selects an
# exact-version request (default: unversioned discovery). The install driver
# always passes -Dlibjpl_DIR (empty for default layouts), so an empty entry is
# removed here and prefix-path search is not shadowed by a stale
# NOTFOUND value.
if(DEFINED libjpl_DIR AND "${libjpl_DIR}" STREQUAL "")
  unset(libjpl_DIR CACHE)
endif()
if(DEFINED REQUEST_VERSION)
  find_package(libjpl ${REQUEST_VERSION} EXACT CONFIG REQUIRED)
else()
  find_package(libjpl CONFIG REQUIRED)
endif()
add_executable(consumer_c main.c)
add_executable(consumer_cpp main.cpp)
target_link_libraries(consumer_c PRIVATE jpl::jpl)
target_link_libraries(consumer_cpp PRIVATE jpl::jpl)
enable_testing()
add_test(NAME consumer_c COMMAND consumer_c)
add_test(NAME consumer_cpp COMMAND consumer_cpp)
