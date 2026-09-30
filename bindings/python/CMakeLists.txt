# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

# Configure pyproject.toml with project version
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml"
    "${CMAKE_CURRENT_BINARY_DIR}/pyproject.toml" @ONLY)

# Configure _libjpl.py with project version
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/libjpl/_libjpl.py"
    "${CMAKE_CURRENT_BINARY_DIR}/src/libjpl/_libjpl.py" @ONLY)

# Probe for cffi at configure time
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
    message(WARNING "Python3 not found; skipping Python binding test")
    return()
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import cffi"
    RESULT_VARIABLE cffi_result
    OUTPUT_QUIET ERROR_QUIET
)
if(NOT cffi_result EQUAL 0)
    message(WARNING "cffi not importable; skipping Python binding test")
    return()
endif()

# Set up library path for the shared library - use add_custom_command at build time
if(WIN32)
    # On Windows, copy jpl.dll to the tests directory at build time
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/tests/jpl.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/../../Debug/jpl.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/jpl.dll"
        COMMENT "Copying jpl.dll for Python binding test"
    )
    add_custom_target(copy_jpl_dll_for_python DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/jpl.dll")
    set(PYTHON_TEST_ENV "PATH=${CMAKE_CURRENT_SOURCE_DIR}/tests;$ENV{PATH}")
elseif(APPLE)
    set(PYTHON_TEST_ENV "DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/../../:$ENV{DYLD_LIBRARY_PATH}")
else()
    # On Linux, the shared library is in the top-level build directory
    set(PYTHON_TEST_ENV "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/../../:$ENV{LD_LIBRARY_PATH}")
endif()

add_test(NAME jpl_python_binding
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_python_binding.py"
    ENVIRONMENT ${PYTHON_TEST_ENV}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/data"
)