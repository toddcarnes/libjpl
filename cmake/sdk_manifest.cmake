# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

# Generates a manifest of all files under SDK_ROOT for zip packaging.
# Output: <build>/sdk_manifest.txt (one file per line, relative to SDK_ROOT)

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED ENV{SDK_ROOT})
    message(FATAL_ERROR "SDK_ROOT environment variable not set")
endif()

set(SDK_ROOT "$ENV{SDK_ROOT}")

if(NOT EXISTS "${SDK_ROOT}")
    message(FATAL_ERROR "SDK_ROOT does not exist: ${SDK_ROOT}")
endif()

file(GLOB_RECURSE SDK_FILES "${SDK_ROOT}/*")

# Convert to relative paths
set(MANIFEST_LINES)
foreach(f ${SDK_FILES})
    if(NOT IS_DIRECTORY "${f}")
        file(RELATIVE_PATH rel "${SDK_ROOT}" "${f}")
        list(APPEND MANIFEST_LINES "${rel}")
    endif()
endforeach()

# Write manifest (one file per line for tar --files-from)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sdk_manifest.txt" "")
foreach(f ${MANIFEST_LINES})
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/sdk_manifest.txt" "${f}\n")
endforeach()
list(LENGTH MANIFEST_LINES MANIFEST_COUNT)
message(STATUS "Generated SDK manifest with ${MANIFEST_COUNT} files")