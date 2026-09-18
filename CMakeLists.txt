cmake_minimum_required(VERSION 3.20)

project(libjpl VERSION 0.1.0 LANGUAGES C)

# Numeric code ships -O0 unless a build type is set; default to Release
# for single-config generators so the library is usable out of the box.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(jpl STATIC
    src/jpl.c
)
add_library(jpl::jpl ALIAS jpl)

target_compile_features(jpl PUBLIC c_std_17)
set_target_properties(jpl PROPERTIES
    C_EXTENSIONS OFF
)
# PIC only matters when linking this static lib into a shared object.
option(LIBJPL_PIC "Build static lib with position-independent code" ON)
if(LIBJPL_PIC)
    set_target_properties(jpl PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(jpl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

file(GLOB ERFA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/erfa/*.c")
# Only ERFA's own test drivers carry main(); anchor the exclusion so a
# future upstream file (e.g. t_erfa_c_foo.c) cannot slip into the library.
list(FILTER ERFA_SOURCES EXCLUDE REGEX "/t_erfa_c(_extra)?\\.c$")
target_sources(jpl PRIVATE ${ERFA_SOURCES})
target_include_directories(jpl PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/erfa>)
# NOTE: src/erfa is intentionally NOT installed with the 3 public headers
# below. Downstream users never need erfa.h (frames API hides it), so the
# PUBLIC include stays build-interface-only. Revisit only if a public header
# ever includes erfa.h.
target_sources(jpl PRIVATE src/jpl_frames.c)
target_sources(jpl PRIVATE src/jpl_convenience.c)
# erfaversion.c expects version macros from generated config.h (see configure.ac
# in the ERFA release); define them here since we build without autotools.
target_compile_definitions(jpl PRIVATE
    PACKAGE_VERSION=\"2.0.1\"
    PACKAGE_VERSION_MAJOR=2
    PACKAGE_VERSION_MINOR=0
    PACKAGE_VERSION_MICRO=1
    SOFA_VERSION=\"20231011\"
)

if(MSVC)
    target_compile_definitions(jpl PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(jpl PRIVATE /W4 /permissive-)
else()
    target_compile_options(jpl PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(jpl PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

include(GNUInstallDirs)
install(TARGETS jpl EXPORT libjplTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(EXPORT libjplTargets NAMESPACE jpl:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libjpl)

option(LIBJPL_BUILD_DOCS "Build Doxygen docs" OFF)
if(LIBJPL_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(docs ALL COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif()
install(FILES include/jpl.h include/jpl_frames.h include/jpl_convenience.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

option(LIBJPL_BUILD_TESTS "Build libjpl tests" ON)
if(LIBJPL_BUILD_TESTS)
    enable_testing()
    add_executable(jpl_tests tests/test_jpl.c)
    target_link_libraries(jpl_tests PRIVATE jpl)
    target_compile_features(jpl_tests PRIVATE c_std_17)
    add_test(NAME jpl_tests COMMAND jpl_tests)
    add_executable(jpl_lighttime_tests tests/test_lighttime.c)
    target_link_libraries(jpl_lighttime_tests PRIVATE jpl)
    target_compile_features(jpl_lighttime_tests PRIVATE c_std_17)
    add_test(NAME jpl_lighttime COMMAND jpl_lighttime_tests)
    add_executable(jpl_aberration_tests tests/test_aberration.c)
    target_link_libraries(jpl_aberration_tests PRIVATE jpl)
    target_compile_features(jpl_aberration_tests PRIVATE c_std_17)
    target_include_directories(jpl_aberration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa)
    add_test(NAME jpl_aberration COMMAND jpl_aberration_tests)
    add_executable(jpl_frames_tests tests/test_frames.c)
    target_link_libraries(jpl_frames_tests PRIVATE jpl)
    target_compile_features(jpl_frames_tests PRIVATE c_std_17)
    target_include_directories(jpl_frames_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa)
    add_test(NAME jpl_frames COMMAND jpl_frames_tests)
    add_executable(jpl_convenience_tests tests/test_convenience.c)
    target_link_libraries(jpl_convenience_tests PRIVATE jpl)
    target_compile_features(jpl_convenience_tests PRIVATE c_std_17)
    add_test(NAME jpl_convenience COMMAND jpl_convenience_tests)
    add_executable(jpl_mmap_tests tests/test_mmap.c)
    target_link_libraries(jpl_mmap_tests PRIVATE jpl)
    target_compile_features(jpl_mmap_tests PRIVATE c_std_17)
    add_test(NAME jpl_mmap COMMAND jpl_mmap_tests)
    add_executable(jpl_tt_tdb_tests tests/test_tt_tdb.c)
    target_link_libraries(jpl_tt_tdb_tests PRIVATE jpl)
    target_compile_features(jpl_tt_tdb_tests PRIVATE c_std_17)
    add_test(NAME jpl_tt_tdb COMMAND jpl_tt_tdb_tests)
    add_executable(jpl_fixture_tests tests/test_fixture.c)
    target_link_libraries(jpl_fixture_tests PRIVATE jpl)
    target_compile_features(jpl_fixture_tests PRIVATE c_std_17)
    add_test(NAME jpl_fixture
        COMMAND jpl_fixture_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt")

    option(LIBJPL_FETCH_EPHEMERIDES "Download JPL DE440/DE440t validation data (approx 210 MB)" OFF)
    set(LIBJPL_EPHEMERIS_DIR "${CMAKE_CURRENT_BINARY_DIR}/ephemeris" CACHE PATH "Downloaded/located JPL test data")

    # Resolves one fetch-tier file to a usable path: prefers the maintainer-local
    # copy (<src_dir>/<src_name>), else downloads <url> to
    # ${LIBJPL_EPHEMERIS_DIR}/<dest_name> and verifies it per
    # tests/data/MANIFEST.txt (exact byte size always; SHA-256 when <sha256> is
    # non-empty, enforced by file(DOWNLOAD EXPECTED_HASH)). Any failure is
    # FATAL_ERROR naming the file with expected vs actual plus the URL, so a JPL
    # re-cut fails loudly at configure time instead of half-registering suites.
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
        message(STATUS "libjpl: downloading ${dest_name} from ${url}")
        set(dest "${LIBJPL_EPHEMERIS_DIR}/${dest_name}")
        if("${sha256}" STREQUAL "")
            file(DOWNLOAD "${url}" "${dest}" STATUS dl_status SHOW_PROGRESS)
        else()
            file(DOWNLOAD "${url}" "${dest}" EXPECTED_HASH SHA256=${sha256} STATUS dl_status SHOW_PROGRESS)
        endif()
        list(GET dl_status 0 dl_code)
        list(GET dl_status 1 dl_message)
        if(NOT dl_code EQUAL 0)
            message(FATAL_ERROR
                "libjpl: failed to download ${dest_name}: ${dl_message} (URL ${url}). "
                "If JPL re-cut the file, update tests/data/MANIFEST.txt and re-run configure.")
        endif()
        if(NOT "${expected_size}" STREQUAL "")
            file(SIZE "${dest}" dl_actual_size)
            if(NOT dl_actual_size EQUAL expected_size)
                message(FATAL_ERROR
                    "libjpl: size mismatch for ${dest_name}: expected ${expected_size} bytes, "
                    "got ${dl_actual_size} bytes (URL ${url}). "
                    "If JPL re-cut the file, update tests/data/MANIFEST.txt and re-run configure.")
            endif()
        endif()
        set(${out_var} "${dest}" PARENT_SCOPE)
    endfunction()

    # JPL_TESTPO env var overrides the default testpo data directory (a
    # maintainer-local convenience that only takes effect when
    # LIBJPL_FETCH_EPHEMERIDES is ON; without the option ON nothing in this
    # file touches the disk location it names). Single assignment shared by
    # the fetch block and the sweep below.
    if(DEFINED ENV{JPL_TESTPO} AND NOT "$ENV{JPL_TESTPO}" STREQUAL "")
        set(JPL_TESTPO_DIR "$ENV{JPL_TESTPO}")
    else()
        set(JPL_TESTPO_DIR "D:/ephem/jpl/testpo")
    endif()

    # Fetch tier: when ON, resolve the four validation files (local redirect
    # per file, JPL download otherwise). When OFF, none of this runs and no
    # network is touched.
    if(LIBJPL_FETCH_EPHEMERIDES)
        file(MAKE_DIRECTORY "${LIBJPL_EPHEMERIS_DIR}")

        # Manifest-pinned values (tests/data/MANIFEST.txt, verified 2026-09-19).
        # Binaries carry sizes only: local checksums.txt hashes describe
        # different spans and MUST NOT be used (see the manifest).
        set(LIBJPL_DE440_URL "https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/linux_p1550p2650.440")
        set(LIBJPL_DE440_SIZE 102272352)
        set(LIBJPL_DE440T_URL "https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440t/linux_p1550p2650.440t")
        set(LIBJPL_DE440T_SIZE 112720608)
        set(LIBJPL_TESTPO440_URL "https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440/testpo.440")
        set(LIBJPL_TESTPO440_SHA256 "45084a7f406b8e41032e7d99dd8042d4df97a9f752a68c06d1a27af16693429f")
        set(LIBJPL_TESTPO440T_URL "https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de440t/testpo.440t")
        set(LIBJPL_TESTPO440T_SHA256 "9801f019be5c335d1e22697ea9fb5888becfd0c623a50c3372fe4491c7a3f2ad")
        set(LIBJPL_TESTPO_SIZE 858238)

        if(NOT "$ENV{JPLEPH}" STREQUAL "")
            set(LIBJPL_JPLEPH_SRC_DIR "$ENV{JPLEPH}")
        else()
            set(LIBJPL_JPLEPH_SRC_DIR "")
        endif()

        # Renamed on download to DE<nnn>.eph so suite pairing stays uniform.
        libjpl_resolve_fetch_file(LIBJPL_DE440_EPH "${LIBJPL_JPLEPH_SRC_DIR}" "DE440.eph"
            "${LIBJPL_DE440_URL}" "DE440.eph" "${LIBJPL_DE440_SIZE}" "")
        libjpl_resolve_fetch_file(LIBJPL_DE440T_EPH "${LIBJPL_JPLEPH_SRC_DIR}" "DE440t.eph"
            "${LIBJPL_DE440T_URL}" "DE440t.eph" "${LIBJPL_DE440T_SIZE}" "")
        libjpl_resolve_fetch_file(LIBJPL_TESTPO_440_FILE "${JPL_TESTPO_DIR}" "testpo.440"
            "${LIBJPL_TESTPO440_URL}" "testpo.440" "${LIBJPL_TESTPO_SIZE}" "${LIBJPL_TESTPO440_SHA256}")
        libjpl_resolve_fetch_file(LIBJPL_TESTPO_440T_FILE "${JPL_TESTPO_DIR}" "testpo.440t"
            "${LIBJPL_TESTPO440T_URL}" "testpo.440t" "${LIBJPL_TESTPO_SIZE}" "${LIBJPL_TESTPO440T_SHA256}")

        # Fetch-tier reference suites (DE440/DE440t) use fixed targets/tests,
        # so the legacy sweep below skips these numbers. Resolution above
        # FATAL_ERROR'd unless both files of each pair exist.
        if(NOT TARGET jpl_testpo_440_tests)
            add_executable(jpl_testpo_440_tests tests/test_testpo.c)
            target_link_libraries(jpl_testpo_440_tests PRIVATE jpl)
            target_compile_features(jpl_testpo_440_tests PRIVATE c_std_17)
            if(MSVC)
                target_compile_definitions(jpl_testpo_440_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
                target_compile_options(jpl_testpo_440_tests PRIVATE /W4 /permissive-)
            else()
                target_compile_options(jpl_testpo_440_tests PRIVATE -Wall -Wextra -Wpedantic)
            endif()
        endif()
        add_test(NAME jpl_testpo_440
            COMMAND jpl_testpo_440_tests "${LIBJPL_DE440_EPH}" "${LIBJPL_TESTPO_440_FILE}")
        if(NOT TARGET jpl_testpo_440t_tests)
            add_executable(jpl_testpo_440t_tests tests/test_testpo.c)
            target_link_libraries(jpl_testpo_440t_tests PRIVATE jpl)
            target_compile_features(jpl_testpo_440t_tests PRIVATE c_std_17)
            if(MSVC)
                target_compile_definitions(jpl_testpo_440t_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
                target_compile_options(jpl_testpo_440t_tests PRIVATE /W4 /permissive-)
            else()
                target_compile_options(jpl_testpo_440t_tests PRIVATE -Wall -Wextra -Wpedantic)
            endif()
        endif()
        add_test(NAME jpl_testpo_440t
            COMMAND jpl_testpo_440t_tests "${LIBJPL_DE440T_EPH}" "${LIBJPL_TESTPO_440T_FILE}")
    endif()

    # test_positions.c was superseded by the testpo harness below (Task 8):
    # tests/de441_reference.txt never existed, so jpl_position_tests could
    # not be wired to it. tests/test_testpo.c validates every testpo line
    # (including DE441 geometric states) instead.

    # Integration suites, registered from the resolved fetch-tier files so a
    # download-only build still validates real data. jpl_real_ephemeris is the
    # DENUM-agnostic smoke test (header sanity + record read + finite state)
    # against the resolved DE440 binary; jpl_all_ephemerides scans the
    # resolved download dir (download-only builds: the DE440/DE440t subset --
    # the sweep binary skips absent files, failing only when none opened;
    # local-redirect builds leave the resolved dir empty -- GLOB finds nothing
    # and only jpl_all_ephemerides_local registers); a second registration
    # scans the maintainer-local JPLEPH dir when set.
    if(LIBJPL_FETCH_EPHEMERIDES)
        if(NOT TARGET jpl_real_ephemeris_tests)
            add_executable(jpl_real_ephemeris_tests tests/test_real_ephemeris.c)
            target_link_libraries(jpl_real_ephemeris_tests PRIVATE jpl)
            target_compile_features(jpl_real_ephemeris_tests PRIVATE c_std_17)
        endif()
        add_test(NAME jpl_real_ephemeris
            COMMAND jpl_real_ephemeris_tests "${LIBJPL_DE440_EPH}")

        if(NOT TARGET jpl_all_ephemerides_tests)
            add_executable(jpl_all_ephemerides_tests tests/test_all_ephemerides.c)
            target_link_libraries(jpl_all_ephemerides_tests PRIVATE jpl)
            target_compile_features(jpl_all_ephemerides_tests PRIVATE c_std_17)
        endif()
        file(GLOB LIBJPL_RESOLVED_BINARIES "${LIBJPL_EPHEMERIS_DIR}/DE*.eph")
        if(LIBJPL_RESOLVED_BINARIES)
            add_test(NAME jpl_all_ephemerides
                COMMAND jpl_all_ephemerides_tests "${LIBJPL_EPHEMERIS_DIR}")
        endif()
        if(NOT "$ENV{JPLEPH}" STREQUAL "")
            add_test(NAME jpl_all_ephemerides_local
                COMMAND jpl_all_ephemerides_tests "$ENV{JPLEPH}")
        endif()
    endif()

    # Fetch-tier 31-file sweep: maintainer-local binaries in JPLEPH paired with
    # testpo files in JPL_TESTPO_DIR (assigned once above; env override else
    # the local default). Gated on the option so plain configures never scan;
    # per-file EXISTS checks let a partial local dir compose with the
    # downloads resolved above.
    if(LIBJPL_FETCH_EPHEMERIDES)
        file(GLOB JPL_TESTPO_FILES "${JPL_TESTPO_DIR}/testpo.*")
        foreach(testpo_file IN LISTS JPL_TESTPO_FILES)
            get_filename_component(testpo_name "${testpo_file}" NAME)
            string(REGEX REPLACE "^testpo\\." "" testpo_num "${testpo_name}")
            # 440/440t are registered by the fixed fetch-tier block above.
            if(testpo_num STREQUAL "440" OR testpo_num STREQUAL "440t")
                continue()
            endif()
            if(testpo_num MATCHES "^[0-9]+t?$" AND EXISTS "$ENV{JPLEPH}/DE${testpo_num}.eph")
                set(testpo_target "jpl_testpo_${testpo_num}_tests")
                if(NOT TARGET ${testpo_target})
                    add_executable(${testpo_target} tests/test_testpo.c)
                    target_link_libraries(${testpo_target} PRIVATE jpl)
                    target_compile_features(${testpo_target} PRIVATE c_std_17)
                    if(MSVC)
                        target_compile_definitions(${testpo_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
                        target_compile_options(${testpo_target} PRIVATE /W4 /permissive-)
                    else()
                        target_compile_options(${testpo_target} PRIVATE -Wall -Wextra -Wpedantic)
                    endif()
                endif()
                add_test(NAME jpl_testpo_${testpo_num}
                    COMMAND ${testpo_target} "$ENV{JPLEPH}/DE${testpo_num}.eph" "${testpo_file}")
            endif()
        endforeach()
    endif()

    # Warning flags for all unit-test executables (incl. integration ones
    # registered above). The NOT TARGET guard keeps this safe if a target
    # is ever removed.
    foreach(tgt IN ITEMS jpl_tests jpl_lighttime_tests jpl_aberration_tests jpl_frames_tests jpl_convenience_tests jpl_mmap_tests jpl_tt_tdb_tests jpl_fixture_tests jpl_real_ephemeris_tests jpl_all_ephemerides_tests)
        if(NOT TARGET ${tgt})
            continue()
        endif()
        if(MSVC)
            target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
            target_compile_options(${tgt} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
endif()
