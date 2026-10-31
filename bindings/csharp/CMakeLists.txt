# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

cmake_minimum_required(VERSION 3.20)

if(NOT TARGET jpl_shared)
    message(WARNING "jpl_shared is not built; skipping C# binding test")
    return()
endif()

# Find dotnet
find_program(DOTNET_EXECUTABLE dotnet)
if(NOT DOTNET_EXECUTABLE)
    message(WARNING "dotnet not found; skipping C# binding test")
    return()
endif()

# Check dotnet version
execute_process(
    COMMAND "${DOTNET_EXECUTABLE}" --version
    RESULT_VARIABLE dotnet_version_rc
    OUTPUT_VARIABLE dotnet_version
    ERROR_QUIET
)
if(NOT dotnet_version_rc EQUAL 0)
    message(WARNING "dotnet --version failed; skipping C# binding test")
    return()
endif()

# Parse major version
string(REGEX MATCH "^([0-9]+)" dotnet_major "${dotnet_version}")
if(NOT dotnet_major OR dotnet_major LESS 8)
    message(WARNING "dotnet version ${dotnet_version} < 8; skipping C# binding test")
    return()
endif()

# Build the library project; the managed configuration follows the CMake
# configuration so a Release run never exercises Debug assemblies.
add_custom_target(LibJpl
    COMMAND "${DOTNET_EXECUTABLE}" build "${CMAKE_CURRENT_SOURCE_DIR}/src/LibJpl/LibJpl.csproj"
        --configuration $<CONFIG>
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building LibJpl C# library"
    VERBATIM
)

# Build the test project
add_custom_target(LibJplTests
    COMMAND "${DOTNET_EXECUTABLE}" build "${CMAKE_CURRENT_SOURCE_DIR}/tests/LibJpl.Tests.csproj"
        --configuration $<CONFIG>
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building LibJpl C# tests"
    VERBATIM
)
add_dependencies(LibJplTests LibJpl jpl_shared)

# Point the loader at the shared library built in this tree instead of
# copying jpl.dll into the source tree.
if(WIN32)
    set(JPL_CSHARP_TEST_ENV "PATH=$<TARGET_FILE_DIR:jpl_shared>;$ENV{PATH}")
elseif(APPLE)
    set(JPL_CSHARP_TEST_ENV "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:jpl_shared>:$ENV{DYLD_LIBRARY_PATH}")
else()
    set(JPL_CSHARP_TEST_ENV "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:jpl_shared>:$ENV{LD_LIBRARY_PATH}")
endif()

add_test(NAME jpl_csharp_binding
    COMMAND "${CMAKE_COMMAND}" -E env "${JPL_CSHARP_TEST_ENV}"
        "${DOTNET_EXECUTABLE}" run --configuration $<CONFIG>
        --project "${CMAKE_CURRENT_SOURCE_DIR}/tests/LibJpl.Tests.csproj" --
        "${CMAKE_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
