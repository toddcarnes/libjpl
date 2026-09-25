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
# destination, but configure-time fetch use is sequential) and are published
# transactionally only after status, size and hash validation succeed: any
# existing destination is first moved to "<dest>.replace-backup", the temp
# is renamed into place through checked operations, and the backup is
# restored on publish failure (F07). A pre-existing backup fails loudly
# without touching either file. A failed fetch removes only the temporary
# file: an existing destination stays intact, and maintainer-local redirect
# sources are never deleted. Every failure is FATAL_ERROR naming the file
# with expected vs actual verification plus the source URL.
#
# Test-only switch LIBJPL_TEST_FORCE_PUBLISH_FAILURE (default false):
# when true, the publish step is skipped and a nonzero result synthesized
# so the rollback path is exercised without platform races. The flag is
# read from the LIBJPL_TEST_FORCE_PUBLISH_FAILURE cache/environment entry
# so `cmake -P` drivers (which forward -D as script variables) see it.

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
    set(replace_backup "${dest}.replace-backup")
    # A stale sibling backup means an earlier replacement was interrupted
    # after moving the destination aside: neither file is authoritative
    # here, so remove only the temp and fail without touching either.
    if(EXISTS "${replace_backup}")
        file(REMOVE "${fetch_tmp}")
        message(FATAL_ERROR
            "libjpl: stale replace-backup exists for ${dest_name} at ${replace_backup}; "
            "an earlier publication was interrupted. Remove or repair it and "
            "re-run configure. The destination was left intact.")
    endif()
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
    # Transactional publication (F07): back up any existing destination,
    # then publish the verified temp through checked renames (CMake 3.20
    # inspects execute_process RESULT_VARIABLE; bare file(RENAME) cannot).
    # On publish failure restore the backup; if the rollback itself fails,
    # name both paths and preserve the backup for manual recovery. The
    # backup is removed only after the new destination exists and the temp
    # is gone; sizes/hashes are re-verified after publication.
    if(EXISTS "${dest}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E rename "${dest}" "${replace_backup}"
            RESULT_VARIABLE _backup_rc)
        if(NOT _backup_rc EQUAL 0 OR NOT EXISTS "${replace_backup}" OR EXISTS "${dest}")
            file(REMOVE "${fetch_tmp}")
            message(FATAL_ERROR
                "libjpl: cannot back up stale destination ${dest} for ${dest_name}; "
                "remove or repair it and re-run configure.")
        endif()
    endif()
    if(DEFINED LIBJPL_TEST_FORCE_PUBLISH_FAILURE AND LIBJPL_TEST_FORCE_PUBLISH_FAILURE)
        set(_publish_rc 1)
    else()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E rename "${fetch_tmp}" "${dest}"
            RESULT_VARIABLE _publish_rc)
    endif()
    if(NOT _publish_rc EQUAL 0 OR NOT EXISTS "${dest}" OR EXISTS "${fetch_tmp}")
        if(DEFINED LIBJPL_TEST_FORCE_PUBLISH_FAILURE AND LIBJPL_TEST_FORCE_PUBLISH_FAILURE)
            set(_publish_detail "forced publish failure")
        else()
            set(_publish_detail "rename of ${fetch_tmp} to ${dest} failed")
        endif()
        if(EXISTS "${replace_backup}")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E rename "${replace_backup}" "${dest}"
                RESULT_VARIABLE _rollback_rc)
            file(REMOVE "${fetch_tmp}")
            if(NOT _rollback_rc EQUAL 0 OR NOT EXISTS "${dest}" OR EXISTS "${replace_backup}")
                message(FATAL_ERROR
                    "libjpl: failed to publish ${dest_name} to ${dest} (${_publish_detail}; URL ${url}) "
                    "AND failed to roll back ${replace_backup} to ${dest}. "
                    "The backup is preserved for manual recovery.")
            endif()
            file(SHA256 "${dest}" _restored_hash)
            file(SIZE "${dest}" _restored_size)
            message(FATAL_ERROR
                "libjpl: failed to publish ${dest_name} to ${dest} (${_publish_detail}; URL ${url}). "
                "The prior cache was restored (size ${_restored_size}; SHA-256 ${_restored_hash}).")
        endif()
        file(REMOVE "${fetch_tmp}")
        message(FATAL_ERROR
            "libjpl: failed to publish ${dest_name} to ${dest} (${_publish_detail}, URL ${url}). "
            "Remove any stale file there and re-run configure.")
    endif()
    if(EXISTS "${replace_backup}")
        file(REMOVE "${replace_backup}")
    endif()
    if(NOT "${expected_size}" STREQUAL "")
        file(SIZE "${dest}" _published_size)
        if(NOT _published_size EQUAL expected_size)
            message(FATAL_ERROR
                "libjpl: published size mismatch for ${dest_name}: expected ${expected_size} bytes, "
                "got ${_published_size} bytes (URL ${url}).")
        endif()
    endif()
    if(NOT "${sha256}" STREQUAL "")
        file(SHA256 "${dest}" _published_hash)
        if(NOT _published_hash STREQUAL sha256)
            message(FATAL_ERROR
                "libjpl: published hash mismatch for ${dest_name}: expected SHA-256 ${sha256}, "
                "got ${_published_hash} (URL ${url}).")
        endif()
    endif()
    set(${out_var} "${dest}" PARENT_SCOPE)
endfunction()
