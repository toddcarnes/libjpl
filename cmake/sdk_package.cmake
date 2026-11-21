# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

# Creates SDK zip archive from staged directory.
# Usage: cmake -P sdk_package.cmake -DSDK_ROOT=<path> -DZIP_PATH=<path> -DMANIFEST_PATH=<path>

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SDK_ROOT OR NOT DEFINED ZIP_PATH OR NOT DEFINED MANIFEST_PATH)
    message(FATAL_ERROR "SDK_ROOT, ZIP_PATH, and MANIFEST_PATH must be set")
endif()

# Strip quotes from paths (passed from CMake command line with quotes)
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" SDK_ROOT "${SDK_ROOT}")
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" ZIP_PATH "${ZIP_PATH}")
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" MANIFEST_PATH "${MANIFEST_PATH}")
    # Normalize path separators
    string(REPLACE "\\" "/" SDK_ROOT "${SDK_ROOT}")
    string(REPLACE "\\" "/" ZIP_PATH "${ZIP_PATH}")
    string(REPLACE "\\" "/" MANIFEST_PATH "${MANIFEST_PATH}")

    message(STATUS "SDK_ROOT: ${SDK_ROOT}")
    message(STATUS "ZIP_PATH: ${ZIP_PATH}")
    message(STATUS "MANIFEST_PATH: ${MANIFEST_PATH}")

# Generate manifest
file(GLOB_RECURSE SDK_FILES "${SDK_ROOT}/*")
list(LENGTH SDK_FILES SDK_FILE_COUNT)
message(STATUS "GLOB found ${SDK_FILE_COUNT} entries under ${SDK_ROOT}")

set(MANIFEST_LINES)
foreach(f ${SDK_FILES})
    if(NOT IS_DIRECTORY "${f}")
        file(RELATIVE_PATH rel "${SDK_ROOT}" "${f}")
        list(APPEND MANIFEST_LINES "${rel}")
    endif()
endforeach()

file(WRITE "${MANIFEST_PATH}" "")
foreach(f ${MANIFEST_LINES})
    file(APPEND "${MANIFEST_PATH}" "${f}\n")
endforeach()

list(LENGTH MANIFEST_LINES MANIFEST_COUNT)
message(STATUS "Generated manifest with ${MANIFEST_COUNT} files")

# Verify manifest exists and is readable
if(NOT EXISTS "${MANIFEST_PATH}")
    message(FATAL_ERROR "Manifest file not created: ${MANIFEST_PATH}")
endif()

# Create zip (run from SDK_ROOT so paths in manifest are relative)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${ZIP_PATH}" --format=zip "--files-from=${MANIFEST_PATH}"
        WORKING_DIRECTORY "${SDK_ROOT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )

if(result)
    message(FATAL_ERROR "Failed to create zip: ${error}")
endif()

message(STATUS "Created ${ZIP_PATH}")