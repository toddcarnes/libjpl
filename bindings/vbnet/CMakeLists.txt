# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

cmake_minimum_required(VERSION 3.20)

# Find dotnet
find_program(DOTNET_EXECUTABLE dotnet)
if(NOT DOTNET_EXECUTABLE)
    message(WARNING "dotnet not found; skipping VB.NET binding test")
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
    message(WARNING "dotnet --version failed; skipping VB.NET binding test")
    return()
endif()

# Parse major version
string(REGEX MATCH "^([0-9]+)" dotnet_major "${dotnet_version}")
if(NOT dotnet_major OR dotnet_major LESS 8)
    message(WARNING "dotnet version ${dotnet_version} < 8; skipping VB.NET binding test")
    return()
endif()

# Build the library project
add_custom_target(LibJplVb
    COMMAND "${DOTNET_EXECUTABLE}" build "${CMAKE_CURRENT_SOURCE_DIR}/src/LibJpl/LibJpl.vbproj"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building LibJpl VB.NET library"
    VERBATIM
)

# Build the test project
add_custom_target(LibJplVbTests
    COMMAND "${DOTNET_EXECUTABLE}" build "${CMAKE_CURRENT_SOURCE_DIR}/tests/LibJpl.VbTests.vbproj"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building LibJpl VB.NET tests"
    VERBATIM
)

# Add test - use a wrapper script for environment setup
if(WIN32)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_vbnet_test.bat.in"
        "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.bat" @ONLY)
    add_test(NAME jpl_vbnet_binding
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.bat"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
elseif(APPLE)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_vbnet_test.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.sh" @ONLY)
    file(COPY "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.sh"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE)
    add_test(NAME jpl_vbnet_binding
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
else()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_vbnet_test.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.sh" @ONLY)
    file(COPY "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.sh"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE)
    add_test(NAME jpl_vbnet_binding
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/run_vbnet_test.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
endif()