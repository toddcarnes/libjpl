# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Verified download-cache reuse tests (test-only, owned by T20, covers L06).
# Uses the production resolver from cmake/LibjplTestData.cmake (included via
# RESOLVER, which the CMakeLists registration sets to the source-tree
# module) against disposable build-tree directories only. All artifacts are
# a four-byte controlled file plus small manifest copies; no large transfer
# and no web-server dependency (platform-correct file:// URLs plus
# deliberate file renames stand in for unavailable URLs). Fresh WORK
# subdirectories isolate every case. Requires RESOLVER (module path) and
# WORK (unique build-tree directory); LIBJPL_TEST_DATA_MANIFEST arrives
# per-case from throwaway manifests inside WORK, while LIBJPL_TEST_DATA_DIR
# optionally names the source root (forwarded by the CMakeLists driver so
# out-of-tree build dirs resolve the real manifest correctly).
#
# Cases (each aborts with FATAL_ERROR naming the case on mismatch):
#   1. reuse: initial file-URL fetch, source renamed away, valid cache
#      reused without touching the unavailable URL.
#   2. truncated: short cache + unavailable URL fails at download; dest
#      intact. The size-mismatch diagnostic is exercised via the
#      download-verify path by the replacement case's corrupt source below
#      only if reachable; here the unavailable URL fires first by design.
#   3. wrong-hash: same-size wrong-content cache + unavailable URL fails at
#      download; dest intact.
#   3b. bad-download-size: live source with wrong byte count fails the
#      size check with a "size mismatch" diagnostic; dest intact.
#   3c. bad-download-hash: live same-size wrong-content source fails the
#      hash check with a "hash mismatch" diagnostic; dest intact.
#   4. partial tmp: stale "<dest>.fetch-tmp" never validates as cache;
#      valid fetch replaces it and publishes.
#   5. unavailable: missing dest + unavailable URL fails; no dest created.
#   6. replacement: corrupt dest replaced by a successful verified fetch.
#   7. missing manifest key: zero/missing value fails loudly naming the key
#      (empty DEST on lookup, FATAL on required metadata).
#   7b. zero-length value: "key=" present but empty fails naming the key.
#   8. redirect: legitimate local redirect with a deliberately different
#      span/size is used as-is (download metadata never applied to it).
#   9. size-only: hashless cache entry reused on size alone (documented
#      size-only guarantee, no false integrity claim).
#
# Failure reporting: each case records exit code plus stdout/stderr; the
# first failing expectation aborts naming the case. These checks run with
# LIBJPL_FETCH_EPHEMERIDES=OFF (default suite): they touch only file://
# URLs in disposable directories and never the network. The FETCH-OFF
# non-evaluation of redirect directories/download URLs in CMakeLists.txt is
# structural (none of that code runs when the option is OFF); this script
# does not unit-test that claim. Limitation: no case here reconfigures
# CMakeLists.txt itself with FETCH OFF against a live redirect dir.
function(jpl_cache_case label expect_fail)
  execute_process(COMMAND ${ARGN}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(expect_fail)
    if(NOT "${rc}" STREQUAL "1")
      message(FATAL_ERROR "fetch-cache case '${label}' did not fail with exit 1 (got ${rc}):\n${out}\n${err}")
    endif()
  else()
    if(NOT "${rc}" STREQUAL "0")
      message(FATAL_ERROR "fetch-cache case '${label}' expected success but got ${rc}:\n${out}\n${err}")
    endif()
  endif()
endfunction()
# Assert that a failed case's combined output names the expected diagnostic.
function(jpl_cache_expect_fail label pattern)
  execute_process(COMMAND ${ARGN}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT "${rc}" STREQUAL "1")
    message(FATAL_ERROR "fetch-cache case '${label}' did not fail with exit 1 (got ${rc}):\n${out}\n${err}")
  endif()
  set(combined "${out}\n${err}")
  if(NOT combined MATCHES "${pattern}")
    message(FATAL_ERROR "fetch-cache case '${label}' stderr/stdout missing '${pattern}':\n${combined}")
  endif()
endfunction()
if(NOT DEFINED RESOLVER OR NOT DEFINED WORK)
  message(FATAL_ERROR "test_fetch_cache.cmake needs RESOLVER and WORK")
endif()
if(NOT EXISTS "${RESOLVER}")
  message(FATAL_ERROR "test_fetch_cache.cmake: RESOLVER does not exist: ${RESOLVER}")
endif()
file(MAKE_DIRECTORY "${WORK}")

# Shared four-byte payload with its real computed hash (computed once from
# the actual bytes, then pinned in the throwaway manifest: the new-module
# resolver verifies SHA-256 rather than trusting any literal).
file(MAKE_DIRECTORY "${WORK}/origin")
file(WRITE "${WORK}/origin/payload.bin" "ABCD")
file(SHA256 "${WORK}/origin/payload.bin" PAYLOAD_HASH)
file(SIZE "${WORK}/origin/payload.bin" PAYLOAD_SIZE)
file(WRITE "${WORK}/manifest.txt" "cache.bin.size=${PAYLOAD_SIZE}\ncache.bin.sha256=${PAYLOAD_HASH}\n")
file(TO_CMAKE_PATH "${WORK}/origin/payload.bin" _origin_p)
if(_origin_p MATCHES "^/")
  set(ORIGIN_URL "file://${_origin_p}")
else()
  set(ORIGIN_URL "file:///${_origin_p}")
endif()

# Case helper layout: each case owns "${WORK}/<name>/{src,cache}".
# Download cases pass an empty source dir so the file:// URL is actually
# fetched; only the redirect case passes a live local source.
# 1. Reuse: initial fetch via the file URL, then rename the source away so
#    the URL is unavailable; the second resolve must reuse the valid cache.
file(MAKE_DIRECTORY "${WORK}/reuse/src" "${WORK}/reuse/cache")
file(COPY_FILE "${WORK}/origin/payload.bin" "${WORK}/reuse/src/payload.bin")
file(TO_CMAKE_PATH "${WORK}/reuse/src/payload.bin" _reuse_p)
if(_reuse_p MATCHES "^/")
  set(_reuse_url "file://${_reuse_p}")
else()
  set(_reuse_url "file:///${_reuse_p}")
endif()
file(WRITE "${WORK}/reuse/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"${_reuse_url}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"reuse resolved: \${OUT}\")\n")
jpl_cache_case("reuse initial fetch" 0
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/reuse/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/reuse/resolve.cmake")
file(RENAME "${WORK}/reuse/src/payload.bin" "${WORK}/reuse/src/payload.bin.hidden")
jpl_cache_case("reuse valid cache offline" 0
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/reuse/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/reuse/resolve.cmake")

# 2. Truncated cache: 2-byte dest + unavailable URL must fail at the
#    download step (unavailable URL); dest intact.
file(MAKE_DIRECTORY "${WORK}/trunc/cache")
file(WRITE "${WORK}/trunc/cache/cache.bin" "AB")
file(WRITE "${WORK}/trunc/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"file:///no/such/dir/payload.bin\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"trunc resolved: \${OUT}\")\n")
jpl_cache_expect_fail("truncated cache fails" "failed to download cache\\.bin"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/trunc/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/trunc/resolve.cmake")
file(SIZE "${WORK}/trunc/cache/cache.bin" _trunc_size)
if(NOT _trunc_size EQUAL 2)
  message(FATAL_ERROR "truncated-cache case destroyed the destination (size ${_trunc_size}, expected 2)")
endif()
if(EXISTS "${WORK}/trunc/cache/cache.bin.fetch-tmp")
  message(FATAL_ERROR "truncated-cache case left a stray temporary file")
endif()

# 3. Same-size wrong-hash cache: "WXYZ" (4 bytes, wrong hash) must fail at
#    the download step (unavailable URL); dest intact.
file(MAKE_DIRECTORY "${WORK}/wronghash/cache")
file(WRITE "${WORK}/wronghash/cache/cache.bin" "WXYZ")
file(WRITE "${WORK}/wronghash/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"file:///no/such/dir/payload.bin\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"wronghash resolved: \${OUT}\")\n")
jpl_cache_expect_fail("wrong-hash cache fails" "failed to download cache\\.bin"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/wronghash/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/wronghash/resolve.cmake")
file(READ "${WORK}/wronghash/cache/cache.bin" _wrong_content)
if(NOT _wrong_content STREQUAL "WXYZ")
  message(FATAL_ERROR "wrong-hash case destroyed the destination (content '${_wrong_content}')")
endif()

# 3b. Bad-download size: live 2-byte source vs 4-byte expectation must fail
#     with the size-mismatch diagnostic; no dest published.
file(MAKE_DIRECTORY "${WORK}/badsize/src" "${WORK}/badsize/cache")
file(WRITE "${WORK}/badsize/src/payload.bin" "AB")
file(TO_CMAKE_PATH "${WORK}/badsize/src/payload.bin" _badsize_p)
if(_badsize_p MATCHES "^/")
  set(_badsize_url "file://${_badsize_p}")
else()
  set(_badsize_url "file:///${_badsize_p}")
endif()
file(WRITE "${WORK}/badsize/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"${_badsize_url}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"badsize resolved: \${OUT}\")\n")
jpl_cache_expect_fail("bad download size fails" "size mismatch for cache\\.bin"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/badsize/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/badsize/resolve.cmake")
if(EXISTS "${WORK}/badsize/cache/cache.bin")
  message(FATAL_ERROR "bad-download-size case published an unverified destination")
endif()
if(EXISTS "${WORK}/badsize/cache/cache.bin.fetch-tmp")
  message(FATAL_ERROR "bad-download-size case left a stray temporary file")
endif()

# 3c. Bad-download hash: live same-size wrong-content source must fail with
#     the hash-mismatch diagnostic; no dest published.
file(MAKE_DIRECTORY "${WORK}/badhash/src" "${WORK}/badhash/cache")
file(WRITE "${WORK}/badhash/src/payload.bin" "WXYZ")
file(TO_CMAKE_PATH "${WORK}/badhash/src/payload.bin" _badhash_p)
if(_badhash_p MATCHES "^/")
  set(_badhash_url "file://${_badhash_p}")
else()
  set(_badhash_url "file:///${_badhash_p}")
endif()
file(WRITE "${WORK}/badhash/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"${_badhash_url}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"badhash resolved: \${OUT}\")\n")
jpl_cache_expect_fail("bad download hash fails" "hash mismatch for cache\\.bin"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/badhash/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/badhash/resolve.cmake")
if(EXISTS "${WORK}/badhash/cache/cache.bin")
  message(FATAL_ERROR "bad-download-hash case published an unverified destination")
endif()
if(EXISTS "${WORK}/badhash/cache/cache.bin.fetch-tmp")
  message(FATAL_ERROR "bad-download-hash case left a stray temporary file")
endif()

# 4. Interrupted/partial temporary file: a stale "AB" fetch-tmp must never
#    validate as the cache; a good source then publishes over it.
file(MAKE_DIRECTORY "${WORK}/partial/src" "${WORK}/partial/cache")
file(COPY_FILE "${WORK}/origin/payload.bin" "${WORK}/partial/src/payload.bin")
file(WRITE "${WORK}/partial/cache/cache.bin.fetch-tmp" "AB")
file(TO_CMAKE_PATH "${WORK}/partial/src/payload.bin" _partial_p)
if(_partial_p MATCHES "^/")
  set(_partial_url "file://${_partial_p}")
else()
  set(_partial_url "file:///${_partial_p}")
endif()
file(WRITE "${WORK}/partial/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"${_partial_url}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"partial resolved: \${OUT}\")\n")
jpl_cache_case("partial tmp replaced by verified fetch" 0
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/partial/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/partial/resolve.cmake")
file(SHA256 "${WORK}/partial/cache/cache.bin" _partial_hash)
if(NOT _partial_hash STREQUAL PAYLOAD_HASH)
  message(FATAL_ERROR "partial-tmp case published an unverified destination (hash ${_partial_hash})")
endif()
file(SIZE "${WORK}/partial/cache/cache.bin" _partial_size)
if(NOT _partial_size EQUAL PAYLOAD_SIZE)
  message(FATAL_ERROR "partial-tmp case published a truncated destination (size ${_partial_size})")
endif()

# 5. Unavailable URL with no cache: must fail, no destination created.
file(MAKE_DIRECTORY "${WORK}/missing/cache")
file(WRITE "${WORK}/missing/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"file:///no/such/dir/payload.bin\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"missing resolved: \${OUT}\")\n")
jpl_cache_expect_fail("unavailable URL fails" "failed to download cache\\.bin"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/missing/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/missing/resolve.cmake")
if(EXISTS "${WORK}/missing/cache/cache.bin")
  message(FATAL_ERROR "unavailable-URL case created an unverified destination")
endif()

# 6. Successful replacement: corrupt dest replaced by a verified fetch.
file(MAKE_DIRECTORY "${WORK}/replace/src" "${WORK}/replace/cache")
file(COPY_FILE "${WORK}/origin/payload.bin" "${WORK}/replace/src/payload.bin")
file(WRITE "${WORK}/replace/cache/cache.bin" "WXYZ")
file(TO_CMAKE_PATH "${WORK}/replace/src/payload.bin" _replace_p)
if(_replace_p MATCHES "^/")
  set(_replace_url "file://${_replace_p}")
else()
  set(_replace_url "file:///${_replace_p}")
endif()
file(WRITE "${WORK}/replace/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"${_replace_url}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"replace resolved: \${OUT}\")\n")
jpl_cache_case("corrupt cache replaced" 0
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/replace/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/replace/resolve.cmake")
file(SHA256 "${WORK}/replace/cache/cache.bin" _replace_hash)
if(NOT _replace_hash STREQUAL PAYLOAD_HASH)
  message(FATAL_ERROR "replacement case left a corrupt destination (hash ${_replace_hash})")
endif()

# 7. Zero/missing manifest value: an empty manifest must fail loudly naming
#    the missing key instead of fetching from an empty URL.
file(MAKE_DIRECTORY "${WORK}/nokey/cache")
file(WRITE "${WORK}/nokey/empty.txt" "# no keys\n")
file(WRITE "${WORK}/nokey/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_manifest_value(URL \"cache.bin.url\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"\${URL}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"nokey resolved: \${OUT}\")\n")
jpl_cache_expect_fail("missing manifest key fails" "key 'cache\\.bin\\.url' missing"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/nokey/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/nokey/empty.txt"
  -P "${WORK}/nokey/resolve.cmake")

# 7b. Zero-length manifest value: "key=" present but empty must fail loudly
#     naming the key instead of fetching from an empty URL.
file(MAKE_DIRECTORY "${WORK}/emptyval/cache")
file(WRITE "${WORK}/emptyval/zero.txt" "cache.bin.url=\n")
file(WRITE "${WORK}/emptyval/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_manifest_value(URL \"cache.bin.url\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"\${URL}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "message(STATUS \"emptyval resolved: \${OUT}\")\n")
jpl_cache_expect_fail("zero-length manifest value fails" "key 'cache\\.bin\\.url' has an empty"
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/emptyval/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/emptyval/zero.txt"
  -P "${WORK}/emptyval/resolve.cmake")

# 8. Source redirect with a deliberately different legitimate span/size:
#    a maintainer-local file is used as-is; download metadata (size 4) is
#    never applied to the 10-byte local span.
file(MAKE_DIRECTORY "${WORK}/redirect/src" "${WORK}/redirect/cache")
file(WRITE "${WORK}/redirect/src/other.bin" "0123456789")
file(WRITE "${WORK}/redirect/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"${WORK}/redirect/src\" \"other.bin\" \"${ORIGIN_URL}\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"${PAYLOAD_HASH}\")\n"
  "if(NOT OUT STREQUAL \"${WORK}/redirect/src/other.bin\")\n"
  "  message(FATAL_ERROR \"redirect did not use the local source: \${OUT}\")\n"
  "endif()\n"
  "message(STATUS \"redirect resolved: \${OUT}\")\n")
jpl_cache_case("local redirect used as-is" 0
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/redirect/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/redirect/resolve.cmake")
if(EXISTS "${WORK}/redirect/cache/cache.bin")
  message(FATAL_ERROR "redirect case downloaded over a legitimate local source")
endif()
if(NOT EXISTS "${WORK}/redirect/src/other.bin")
  message(FATAL_ERROR "redirect case deleted the maintainer-local source")
endif()

# 9. Hashless cache entry: size-only reuse (documented guarantee, no false
#    integrity claim). A 4-byte dest with an empty hash reuses offline;
#    content is NOT hash-checked by design (see module header).
file(MAKE_DIRECTORY "${WORK}/sizecache/cache")
file(WRITE "${WORK}/sizecache/cache/cache.bin" "WXYZ")
file(WRITE "${WORK}/sizecache/resolve.cmake"
  "include(\"${RESOLVER}\")\n"
  "libjpl_resolve_fetch_file(OUT \"\" \"\" \"file:///no/such/dir/payload.bin\" \"cache.bin\" \"${PAYLOAD_SIZE}\" \"\")\n"
  "message(STATUS \"sizecache resolved: \${OUT}\")\n")
jpl_cache_case("hashless size-only reuse" 0
  "${CMAKE_COMMAND}"
  "-DLIBJPL_EPHEMERIS_DIR=${WORK}/sizecache/cache"
  "-DLIBJPL_TEST_DATA_MANIFEST=${WORK}/manifest.txt"
  -P "${WORK}/sizecache/resolve.cmake")

message(STATUS "libjpl: fetch-cache script tests OK")
