# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

if(NOT TARGET jpl_shared)
    message(WARNING "jpl_shared is not built; skipping Python binding test")
    return()
endif()

# Configure pyproject.toml with project version
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml"
    "${CMAKE_CURRENT_BINARY_DIR}/pyproject.toml" @ONLY)

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

# Point the dynamic loader at the shared library built in this tree. A
# loader path is used instead of copying jpl.dll/libjpl.so into the source
# tree, so Debug and Release runs cannot pick up each other's artifacts.
if(WIN32)
    set(JPL_PYTHON_TEST_ENV "PATH=$<TARGET_FILE_DIR:jpl_shared>;$ENV{PATH}")
elseif(APPLE)
    set(JPL_PYTHON_TEST_ENV "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:jpl_shared>:$ENV{DYLD_LIBRARY_PATH}")
else()
    set(JPL_PYTHON_TEST_ENV "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:jpl_shared>:$ENV{LD_LIBRARY_PATH}")
endif()

add_test(NAME jpl_python_binding
    COMMAND "${CMAKE_COMMAND}" -E env "${JPL_PYTHON_TEST_ENV}"
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_python_binding.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/data"
)