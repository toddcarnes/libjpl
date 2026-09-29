# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

project(jpl_cpp_binding LANGUAGES CXX)

add_executable(jpl_cpp_binding_tests tests/test_cpp_binding.cpp)
target_link_libraries(jpl_cpp_binding_tests PRIVATE jpl::jpl_static)
target_compile_features(jpl_cpp_binding_tests PRIVATE cxx_std_17)
target_include_directories(jpl_cpp_binding_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(MSVC)
    target_compile_options(jpl_cpp_binding_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(jpl_cpp_binding_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME jpl_cpp_binding
    COMMAND jpl_cpp_binding_tests "${CMAKE_SOURCE_DIR}/tests/data/de441_2000_2010.eph")