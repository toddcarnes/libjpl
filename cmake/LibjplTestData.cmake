# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Shared JPL test-data manifest/resolver (owned by T20, covers L06).
# Included by CMakeLists.txt (production fetch tier) and by
# tests/cmake/test_fetch_cache.cmake (offline script tests), so both use
# the exact same code. Consumes LIBJPL_EPHEMERIS_DIR (download/cache
# directory). An explicit LIBJPL_TEST_DATA_MANIFEST path always wins when
# set (script tests pass a controlled manifest per case); otherwise
# LIBJPL_TEST_DATA_DIR names the source root whose
# tests/data/MANIFEST.txt is used; otherwise the manifest defaults to
# tests/data/MANIFEST.txt resolved relative to this module, never to the
# caller's current directory. Callers whose build tree may live outside the
# source checkout must set LIBJPL_TEST_DATA_DIR to the source root before
# including this module (the T20 real-tier reconfigure caught the module
# path resolving outside the checkout).
#
# Cache policy: a cached destination is reused only when its exact byte
# size matches (when a size is supplied) and its SHA-256 matches (when a
# hash is supplied). Hashless entries get a size-only guarantee, not a
# claim of cryptographic integrity. Downloads go to a deterministic sibling
# temporary file ("<dest>.fetch-tmp", overwritten when a previous fetch
# was interrupted; not unique across concurrent resolves of the same
# destination, but configure-time fetch use is sequential) and are renamed
# into place only after status, size and hash validation succeed; a rename
# failure is fatal. A failed fetch removes only the temporary file:
# an existing destination stays intact, and maintainer-local redirect
# sources are never deleted. Every failure is FATAL_ERROR naming the file
# with expected vs actual verification plus the source URL.

# Reads one "<key>=<value>" pair from tests/data/MANIFEST.txt, which is
# the single authoritative source for download URLs/sizes/hashes (no
# such literal may live elsewhere in this file). Configure fails with
# FATAL_ERROR naming the missing key so a manifest regression breaks
# loudly instead of downloading from an empty URL.
function(libjpl_manifest_value out_var key)
    if(DEFINED LIBJPL_TEST_DATA_MANIFEST AND NOT "${LIBJPL_TEST_DATA_MANIFEST}" STREQUAL "")
        set(_libjpl_manifest "${LIBJPL_TEST_DATA_MANIFEST}")
    elseif(DEFINED LIBJPL_TEST_DATA_DIR AND NOT "${LIBJPL_TEST_DATA_DIR}" STREQUAL "")
        set(_libjpl_manifest "${LIBJPL_TEST_DATA_DIR}/tests/data/MANIFEST.txt")
    else()
        # Module lives at <root>/cmake/, so the default manifest is
        # <root>/tests/data/MANIFEST.txt regardless of the including
        # project's directory layout (CMAKE_CURRENT_LIST_DIR is the
        # module's own directory even when included from CMakeLists).
        get_filename_component(_libjpl_manifest_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
        set(_libjpl_manifest "${_libjpl_manifest_root}/tests/data/MANIFEST.txt")
    endif()
    string(REPLACE "." "\\." escaped_key "${key}")
    file(STRINGS "${_libjpl_manifest}" manifest_lines)
    foreach(manifest_line IN LISTS manifest_lines)
        if(manifest_line MATCHES "^${escaped_key}=(.*)$")
            if("${CMAKE_MATCH_1}" STREQUAL "")
                message(FATAL_ERROR
                    "libjpl: key '${key}' has an empty (zero-length) value in ${_libjpl_manifest}; "
                    "restore the machine-readable download block.")
            endif()
            set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "libjpl: key '${key}' missing from tests/data/MANIFEST.txt; "
        "restore the machine-readable download block.")
endfunction()

# Resolves one fetch-tier file to a usable path: prefers the maintainer-local
# copy (<src_dir>/<src_name>), reuses an already-valid downloaded cache
# entry, else downloads <url> to ${LIBJPL_EPHEMERIS_DIR}/<dest_name> and
# verifies it per tests/data/MANIFEST.txt (exact byte size always; SHA-256
# when <sha256> is non-empty, so hashless binaries carry a size-only
# guarantee). Maintainer-local sources are used as-is: downloaded-span
# sizes/hashes are never applied to files with different legitimate spans.
# Any download/validation failure is FATAL_ERROR naming the file with
# expected vs actual plus the URL, leaving any existing cached destination
# intact, so a JPL re-cut fails loudly at configure time instead of
# half-registering suites.
function(libjpl_resolve_fetch_file out_var src_dir src_name url dest_name expected_size sha256)
    if(NOT "${src_dir}" STREQUAL "")
        set(candidate "${src_dir}/${src_name}")
    else()
        set(candidate "")
    endif()
    if(NOT "${candidate}" STREQUAL "" AND EXISTS "${candidate}")
        # Skip downloads: use the maintainer-local copy.
        message(STATUS "libjpl: using local ${dest_name} from ${candidate}")
        set(${out_var} "${candidate}" PARENT_SCOPE)
        return()
    endif()
    set(dest "${LIBJPL_EPHEMERIS_DIR}/${dest_name}")
    set(cache_valid FALSE)
    if(EXISTS "${dest}")
      file(SIZE "${dest}" actual_size)
      if("${expected_size}" STREQUAL "" OR actual_size EQUAL expected_size)
        if("${sha256}" STREQUAL "")
          set(cache_valid TRUE)
        else()
          file(SHA256 "${dest}" actual_hash)
          if(actual_hash STREQUAL sha256)
            set(cache_valid TRUE)
          endif()
        endif()
      endif()
    endif()
    if(cache_valid)
        message(STATUS "libjpl: reusing cached ${dest_name} from ${dest}")
        set(${out_var} "${dest}" PARENT_SCOPE)
        return()
    endif()
    message(STATUS "libjpl: downloading ${dest_name} from ${url}")
    file(MAKE_DIRECTORY "${LIBJPL_EPHEMERIS_DIR}")
    # Deterministic sibling temporary file: an interrupted earlier fetch
    # leaves a partial "<dest>.fetch-tmp" that is never probed as the cache
    # (only "<dest>" is); the download below overwrites it and only a fully
    # validated file is renamed into place (CMake 3.20-compatible
    # operations throughout). Sequential configure-time use only: concurrent
    # resolves of the same destination would share this name.
    set(fetch_tmp "${dest}.fetch-tmp")
    file(DOWNLOAD "${url}" "${fetch_tmp}" STATUS dl_status SHOW_PROGRESS)
    list(GET dl_status 0 dl_code)
    list(GET dl_status 1 dl_message)
    if(NOT dl_code EQUAL 0 OR NOT EXISTS "${fetch_tmp}")
        file(REMOVE "${fetch_tmp}")
        message(FATAL_ERROR
            "libjpl: failed to download ${dest_name}: ${dl_message} (URL ${url}). "
            "An existing cached file was left intact. "
            "If JPL re-cut the file, update tests/data/MANIFEST.txt and re-run configure.")
    endif()
    if(NOT "${expected_size}" STREQUAL "")
        file(SIZE "${fetch_tmp}" dl_actual_size)
        if(NOT dl_actual_size EQUAL expected_size)
            file(REMOVE "${fetch_tmp}")
            message(FATAL_ERROR
                "libjpl: size mismatch for ${dest_name}: expected ${expected_size} bytes, "
                "got ${dl_actual_size} bytes (URL ${url}). "
                "An existing cached file was left intact. "
                "If JPL re-cut the file, update tests/data/MANIFEST.txt and re-run configure.")
        endif()
    endif()
    if(NOT "${sha256}" STREQUAL "")
        file(SHA256 "${fetch_tmp}" dl_actual_hash)
        if(NOT dl_actual_hash STREQUAL sha256)
            file(REMOVE "${fetch_tmp}")
            message(FATAL_ERROR
                "libjpl: hash mismatch for ${dest_name}: expected SHA-256 ${sha256}, "
                "got ${dl_actual_hash} (URL ${url}). "
                "An existing cached file was left intact. "
                "If JPL re-cut the file, update tests/data/MANIFEST.txt and re-run configure.")
        endif()
    endif()
    if(EXISTS "${dest}")
        file(REMOVE "${dest}")
        if(EXISTS "${dest}")
            file(REMOVE "${fetch_tmp}")
            message(FATAL_ERROR
                "libjpl: cannot replace stale destination ${dest} for ${dest_name}; "
                "remove or repair it and re-run configure.")
        endif()
    endif()
    file(RENAME "${fetch_tmp}" "${dest}")
    if(NOT EXISTS "${dest}" OR EXISTS "${fetch_tmp}")
        message(FATAL_ERROR
            "libjpl: failed to publish ${dest_name} to ${dest} (URL ${url}). "
            "Remove any stale file there and re-run configure.")
    endif()
    set(${out_var} "${dest}" PARENT_SCOPE)
endfunction()
