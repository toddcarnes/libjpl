# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
cmake_minimum_required(VERSION 3.20)

project(libjpl VERSION 0.2.0 LANGUAGES C)

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
target_sources(jpl PRIVATE src/jpl_date.c)
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

# Math linkage is resolved at configure time for the actual toolchain and
# then exported as a concrete dependency (T19/R12c). MSVC-compatible
# frontends rely on the CRT and need no m. Elsewhere, test whether sin
# links with the current default libraries; if not, test for libm and
# use the concrete target. Anything else is a configuration error. No
# consumer-evaluated compiler-ID generator expression remains.
if(MSVC)
    set(JPL_NEED_M_LIB "")
else()
    include(CheckSymbolExists)
    include(CheckLibraryExists)
    check_symbol_exists(sin "math.h" JPL_HAVE_SIN_DEFAULT)
    if(JPL_HAVE_SIN_DEFAULT)
        set(JPL_NEED_M_LIB "")
    else()
        check_library_exists(m sin "" JPL_HAVE_LIBM)
        if(JPL_HAVE_LIBM)
            set(JPL_NEED_M_LIB "m")
        else()
            message(FATAL_ERROR
                "libjpl: sin() is unavailable with default libraries and no libm was found; "
                "provide a toolchain with a C99 math library.")
        endif()
    endif()
endif()
if(JPL_NEED_M_LIB)
    target_link_libraries(jpl PUBLIC "${JPL_NEED_M_LIB}")
endif()

# T18's unconditional license installs live below; GNUInstallDirs stays
# with them (and before the installed include construction) so directory
# variables expand identically for library, header, notice, and export
# destinations. The installed include interface follows the include
# command for the same reason.
include(GNUInstallDirs)
# GNUInstallDirs is established before installed include construction, so
# custom -DCMAKE_INSTALL_INCLUDEDIR values flow into the exported interface.
target_include_directories(jpl
    PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
install(TARGETS jpl EXPORT libjplTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(EXPORT libjplTargets NAMESPACE jpl:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libjpl)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/libjplConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/libjplConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libjpl")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/libjplConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjplConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/libjplConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libjpl")

option(LIBJPL_BUILD_DOCS "Build Doxygen docs" OFF)
if(LIBJPL_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)
  add_custom_target(docs ALL COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" VERBATIM)
  # Public-symbol coverage (T21/R14b): XML plus the file-page check in
  # tests/cmake/check_docs.cmake (with an owned-copy negative control).
  # The docs path-with-spaces check (tests/cmake/test_docs_paths.cmake)
  # copies the current source whitelist and rebuilds docs there.
  set(JPL_DOCS_WORK "${CMAKE_CURRENT_BINARY_DIR}/docs_check")
  add_test(NAME jpl_docs_symbols
      COMMAND ${CMAKE_COMMAND}
          "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/public_symbols.txt"
          "-DXML_DIR=${CMAKE_CURRENT_BINARY_DIR}/docs/xml"
          "-DWORK=${JPL_DOCS_WORK}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_docs.cmake")
  set(JPL_DOCS_PATHS_WORK "${CMAKE_CURRENT_BINARY_DIR}/docs_paths")
  add_test(NAME jpl_docs_paths
      COMMAND ${CMAKE_COMMAND}
          "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
          "-DCHECKER=${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_docs.cmake"
          "-DINVENTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests/public_symbols.txt"
          "-DWORK=${JPL_DOCS_PATHS_WORK}"
          "-DGENERATOR=${CMAKE_GENERATOR}"
          "-DCONFIG=$<CONFIG>"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_docs_paths.cmake")
endif()
install(FILES include/jpl.h include/jpl_frames.h include/jpl_convenience.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/libjpl")
install(FILES src/erfa/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/libjpl"
        RENAME ERFA-LICENSE)

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
    target_include_directories(jpl_lighttime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    target_compile_definitions(jpl PRIVATE LIBJPL_TESTING)
    target_compile_definitions(jpl_lighttime_tests PRIVATE LIBJPL_TESTING)
    add_test(NAME jpl_lighttime COMMAND jpl_lighttime_tests)
    add_executable(jpl_aberration_tests tests/test_aberration.c)
    target_link_libraries(jpl_aberration_tests PRIVATE jpl)
    target_compile_features(jpl_aberration_tests PRIVATE c_std_17)
    target_include_directories(jpl_aberration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    add_test(NAME jpl_aberration COMMAND jpl_aberration_tests)
    add_executable(jpl_frames_tests tests/test_frames.c)
    target_link_libraries(jpl_frames_tests PRIVATE jpl)
    target_compile_features(jpl_frames_tests PRIVATE c_std_17)
    target_include_directories(jpl_frames_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    add_test(NAME jpl_frames COMMAND jpl_frames_tests)
    add_executable(jpl_convenience_tests tests/test_convenience.c)
    target_link_libraries(jpl_convenience_tests PRIVATE jpl)
    target_compile_features(jpl_convenience_tests PRIVATE c_std_17)
    target_include_directories(jpl_convenience_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    add_test(NAME jpl_convenience COMMAND jpl_convenience_tests)
    add_executable(jpl_mmap_tests tests/test_mmap.c)
    target_link_libraries(jpl_mmap_tests PRIVATE jpl)
    target_compile_features(jpl_mmap_tests PRIVATE c_std_17)
    add_test(NAME jpl_mmap COMMAND jpl_mmap_tests)
    find_package(Threads REQUIRED)
    add_executable(jpl_threads_tests tests/test_threads.c)
    target_link_libraries(jpl_threads_tests PRIVATE jpl Threads::Threads)
    target_compile_features(jpl_threads_tests PRIVATE c_std_17)
    add_test(NAME jpl_threads COMMAND jpl_threads_tests)
    set_tests_properties(jpl_threads PROPERTIES TIMEOUT 120)
    add_executable(jpl_tt_tdb_tests tests/test_tt_tdb.c)
    target_link_libraries(jpl_tt_tdb_tests PRIVATE jpl)
    target_compile_features(jpl_tt_tdb_tests PRIVATE c_std_17)
    add_test(NAME jpl_tt_tdb COMMAND jpl_tt_tdb_tests)
    add_executable(jpl_parser_tests tests/test_parser.c)
    target_link_libraries(jpl_parser_tests PRIVATE jpl)
    target_compile_features(jpl_parser_tests PRIVATE c_std_17)
    target_include_directories(jpl_parser_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_definitions(jpl_parser_tests PRIVATE LIBJPL_TESTING)
    add_test(NAME jpl_parser COMMAND jpl_parser_tests)
    add_executable(jpl_api_errors_tests tests/test_api_errors.c)
    target_link_libraries(jpl_api_errors_tests PRIVATE jpl)
    target_compile_features(jpl_api_errors_tests PRIVATE c_std_17)
    target_include_directories(jpl_api_errors_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_definitions(jpl_api_errors_tests PRIVATE LIBJPL_TESTING)
    add_test(NAME jpl_api_errors COMMAND jpl_api_errors_tests)
    # Private date arithmetic (T02/R00): two-part helpers with sentinel
    # preservation; linked through jpl, private headers via src/.
    add_executable(jpl_dates_tests tests/test_dates.c)
    target_link_libraries(jpl_dates_tests PRIVATE jpl)
    target_compile_features(jpl_dates_tests PRIVATE c_std_17)
    target_include_directories(jpl_dates_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME jpl_dates COMMAND jpl_dates_tests)
    # Declared large-file I/O (T01/R03b): sparse 3.2 GiB fixture with a
    # beyond-INT32 last-record offset; 32-bit address-space cases stay
    # inside the test as explicit skips/rejections, not omissions.
    add_executable(jpl_largefile tests/test_largefile.c)
    target_link_libraries(jpl_largefile PRIVATE jpl)
    target_compile_features(jpl_largefile PRIVATE c_std_17)
    target_include_directories(jpl_largefile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_definitions(jpl_largefile PRIVATE LIBJPL_TESTING)
    add_test(NAME jpl_largefile COMMAND jpl_largefile)
    # Multi-gigabyte sparse fixture: generous timeout catches hangs without
    # asserting timing.
    set_tests_properties(jpl_largefile PROPERTIES TIMEOUT 600)
    # Checked testpo row parsing (T03): shared test-only parser plus
    # component/tolerance policy exercised before the live runners.
    add_executable(jpl_testpo_parser_tests tests/test_testpo_parser.c tests/jpl_testpo_util.c)
    target_link_libraries(jpl_testpo_parser_tests PRIVATE jpl)
    target_compile_features(jpl_testpo_parser_tests PRIVATE c_std_17)
    target_include_directories(jpl_testpo_parser_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME jpl_testpo_parser COMMAND jpl_testpo_parser_tests)
    add_executable(jpl_numeric_tests tests/test_numeric.c)
    target_link_libraries(jpl_numeric_tests PRIVATE jpl)
    target_compile_features(jpl_numeric_tests PRIVATE c_std_17)
    target_include_directories(jpl_numeric_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa ${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/data)
    target_compile_definitions(jpl_numeric_tests PRIVATE LIBJPL_TESTING)
    add_test(NAME jpl_numeric COMMAND jpl_numeric_tests)
    # README-compiled examples (T21): ordinary usage patterns with status
    # checks, run against the committed fixture.
    add_executable(jpl_examples tests/examples.c)
    target_link_libraries(jpl_examples PRIVATE jpl)
    target_compile_features(jpl_examples PRIVATE c_std_17)
    add_test(NAME jpl_examples
        COMMAND jpl_examples "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph")
    # Independent reference generator (test development only): compiled
    # with upstream ERFA sources directly, never linked to libjpl. Not
    # part of ALL; invoke explicitly to write a NEW header path only.
    add_executable(jpl_generate_reference EXCLUDE_FROM_ALL tools/generate_numerical_reference.c ${ERFA_SOURCES})
    target_compile_features(jpl_generate_reference PRIVATE c_std_17)
    target_include_directories(jpl_generate_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/erfa)
    target_compile_definitions(jpl_generate_reference PRIVATE
        PACKAGE_VERSION=\"2.0.1\"
        PACKAGE_VERSION_MAJOR=2
        PACKAGE_VERSION_MINOR=0
        PACKAGE_VERSION_MICRO=1
        SOFA_VERSION=\"20231011\"
    )
    if(JPL_NEED_M_LIB)
        target_link_libraries(jpl_generate_reference PRIVATE "${JPL_NEED_M_LIB}")
    endif()
    add_executable(jpl_fixture_tests tests/test_fixture.c tests/jpl_testpo_util.c)
    target_link_libraries(jpl_fixture_tests PRIVATE jpl)
    target_compile_features(jpl_fixture_tests PRIVATE c_std_17)
    target_include_directories(jpl_fixture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME jpl_fixture
        COMMAND jpl_fixture_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fixture_rows.txt")
    # Fixture integrity (T17): CMake hash validation of the committed
    # binary, canonical reference text (CRLF-to-LF only), and ordered
    # identity list against tests/data/MANIFEST.txt. No new library crypto
    # dependency; file(SHA256) is configure/test-time tooling only.
    # The three hash variables below are informational only: the checker
    # reads the authoritative values from the manifest itself, so the
    # manifest stays the single source of truth. They are intentionally
    # NOT referenced with ${...} anywhere, which keeps the file free of
    # "unused variable" diagnostics while documenting the key names.
    set(LIBJPL_FIXTURE_MANIFEST_KEYS
        "fixture.binary.sha256;fixture.text.sha256;fixture.rows.sha256")
    add_test(NAME jpl_fixture_integrity
        COMMAND ${CMAKE_COMMAND}
            "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/MANIFEST.txt"
            "-DBINARY_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
            "-DTEXT_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt"
            "-DROWS_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fixture_rows.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_fixture.cmake")
    # Negative fixture integrity (T17): disposable build-tree copies with a
    # flipped binary byte, duplicated interior rows, altered
    # target/component, altered coordinate (all must fail), plus a CRLF
    # re-encoding (must pass). Never mutates the source fixture.
    set(JPL_FIXTURE_NEGATIVE_WORK "${CMAKE_CURRENT_BINARY_DIR}/fixture_negative")
    add_test(NAME jpl_fixture_integrity_negative
        COMMAND ${CMAKE_COMMAND}
            "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/MANIFEST.txt"
            "-DBINARY_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
            "-DTEXT_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt"
            "-DROWS_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fixture_rows.txt"
            "-DCHECKER=${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_fixture.cmake"
            "-DFIXTURE_RUNNER=$<TARGET_FILE:jpl_fixture_tests>"
            "-DWORK=${JPL_FIXTURE_NEGATIVE_WORK}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_fixture_integrity.cmake")
    # jpl_testpo_tests builds whenever tests are enabled (T15): only its
    # large-data registrations stay opt-in; small synthetic validator
    # data below runs offline.
    add_executable(jpl_testpo_tests tests/test_testpo.c tests/jpl_testpo_util.c)
    target_link_libraries(jpl_testpo_tests PRIVATE jpl)
    target_compile_features(jpl_testpo_tests PRIVATE c_std_17)
    target_include_directories(jpl_testpo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    # Fault-injection variants force the post-read ferror branch so the
    # read-error path is exercised portably. Registered only for the
    # negative read-error cases; the normal runners have no fault behavior.
    add_executable(jpl_testpo_fault_tests tests/test_testpo.c tests/jpl_testpo_util.c)
    target_link_libraries(jpl_testpo_fault_tests PRIVATE jpl)
    target_compile_features(jpl_testpo_fault_tests PRIVATE c_std_17)
    target_include_directories(jpl_testpo_fault_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_definitions(jpl_testpo_fault_tests PRIVATE JPL_TEST_READER_FAULT=1)
    add_executable(jpl_fixture_fault_tests tests/test_fixture.c tests/jpl_testpo_util.c)
    target_link_libraries(jpl_fixture_fault_tests PRIVATE jpl)
    target_compile_features(jpl_fixture_fault_tests PRIVATE c_std_17)
    target_include_directories(jpl_fixture_fault_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_definitions(jpl_fixture_fault_tests PRIVATE JPL_TEST_READER_FAULT=1)
    # Small synthetic validator inputs (T15): generator emits mock
    # ephemerides plus framed runner files; each negative case must exit
    # 1 with its diagnostic category. Bracket-argument EXPECT values keep
    # regex metacharacters (pipes, dots, stars) intact through -D.
    add_executable(jpl_validator_data tests/make_validator_data.c)
    target_link_libraries(jpl_validator_data PRIVATE jpl)
    target_compile_features(jpl_validator_data PRIVATE c_std_17)
    target_include_directories(jpl_validator_data PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    set(JPL_VALIDATOR_WORK "${CMAKE_CURRENT_BINARY_DIR}/validator_data")
    add_test(NAME jpl_validator_data_setup
        COMMAND jpl_validator_data "${JPL_VALIDATOR_WORK}")
    set_tests_properties(jpl_validator_data_setup PROPERTIES
        FIXTURES_SETUP jpl_validator_corpus)
    add_test(NAME jpl_validator_good
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/good.testpo"
            "-DEXPECT="
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_nan
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/nan.testpo"
            "-DEXPECT=[==[nonfinite|invalid.*row]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_missing_body
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/missing.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/missing.testpo"
            "-DEXPECT=[==[reference mismatch]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_angular_present
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/angular.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/angular.testpo"
            "-DEXPECT=[==[reference mismatch]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_denum
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/denum.testpo"
            "-DEXPECT=[==[denum mismatch]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_malformed6
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/malformed6.testpo"
            "-DEXPECT=[==[invalid.*row]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_badcomp
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/badcomp.testpo"
            "-DEXPECT=[==[invalid.*row]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_overflow_pos
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/overflow_pos.testpo"
            "-DEXPECT=[==[invalid.*row]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_overflow_neg
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/overflow_neg.testpo"
            "-DEXPECT=[==[invalid.*row]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_overlong
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/overlong.testpo"
            "-DEXPECT=[==[invalid.*row|invalid row]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_farpsi
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/farpsi.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/farpsi.testpo"
            "-DEXPECT=[==[reference mismatch]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_ttdbwrong
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/ttdbwrong.testpo"
            "-DEXPECT=[==[reference mismatch]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_duplicate_only
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/duplicate_only.testpo"
            "-DEXPECT=[==[insufficient reference coverage]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_read_error
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_testpo_tests>"
            "-DFAULT_RUNNER=$<TARGET_FILE:jpl_testpo_fault_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/good.testpo"
            "-DEXPECT=[==[read error]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_fixture_read_error
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_fixture_fault_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/de441_2000_2010.eph"
            "-DTESTPO=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/testpo_441_slice.txt"
            "-DROWS=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fixture_rows.txt"
            "-DEXPECT=[==[read error]==]"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    add_test(NAME jpl_validator_sweep
        COMMAND ${CMAKE_COMMAND}
            "-DRUNNER=$<TARGET_FILE:jpl_all_ephemerides_tests>"
            "-DGENERATOR=$<TARGET_FILE:jpl_validator_data>"
            "-DWORK=${JPL_VALIDATOR_WORK}"
            "-DEPH=${JPL_VALIDATOR_WORK}/good.eph"
            "-DTESTPO=${JPL_VALIDATOR_WORK}/good.testpo"
            "-DEXPECT="
            "-DSWEEP_DIR=${JPL_VALIDATOR_WORK}/sweep"
            "-DSWEEP_GOOD_DIR=${JPL_VALIDATOR_WORK}/sweep_good"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_validators.cmake")
    # The shared corpus is generated once by jpl_validator_data_setup; every
    # validator case consumes it read-only (F01), including the
    # source-fixture read-error case.
    set_tests_properties(
        jpl_validator_good jpl_validator_nan jpl_validator_missing_body
        jpl_validator_angular_present jpl_validator_denum jpl_validator_malformed6
        jpl_validator_badcomp jpl_validator_overflow_pos jpl_validator_overflow_neg
        jpl_validator_overlong jpl_validator_farpsi jpl_validator_ttdbwrong
        jpl_validator_duplicate_only
        jpl_validator_read_error jpl_fixture_read_error jpl_validator_sweep
        PROPERTIES FIXTURES_REQUIRED jpl_validator_corpus)

    add_executable(jpl_real_ephemeris_tests tests/test_real_ephemeris.c)
    target_link_libraries(jpl_real_ephemeris_tests PRIVATE jpl)
    target_compile_features(jpl_real_ephemeris_tests PRIVATE c_std_17)
    target_include_directories(jpl_real_ephemeris_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_executable(jpl_all_ephemerides_tests tests/test_all_ephemerides.c)
    target_link_libraries(jpl_all_ephemerides_tests PRIVATE jpl)
    target_compile_features(jpl_all_ephemerides_tests PRIVATE c_std_17)
    target_include_directories(jpl_all_ephemerides_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    # License notices (T18): positive pass over the intact source tree,
    # plus build-tree negative cases (missing root license, stripped
    # SPDX header, missing installed ERFA notice) that must fail.
    # The two license installs stay unconditional; the two
    # script-variable lines below are the only install-path literals, so
    # T19 must derive from these exact paths instead of duplicating them.
    # JPL_NOTICE_DATADIR derives from CMAKE_INSTALL_DATADIR (GNUInstallDirs)
    # rather than hardcoding "share", so nonstandard datadirs flow to T19.
    set(JPL_NOTICE_DATADIR "${CMAKE_INSTALL_DATADIR}/licenses/libjpl")
    set(JPL_LICENSE_WORK "${CMAKE_CURRENT_BINARY_DIR}/license_negative")
    add_test(NAME jpl_licenses
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DCHECKER=${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_licenses.cmake"
            "-DINVENTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests/license_inventory.cmake"
            "-DWORK=${JPL_LICENSE_WORK}"
            "-DDATADIR=${CMAKE_INSTALL_DATADIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_licenses.cmake")

    option(LIBJPL_FETCH_EPHEMERIDES "Download JPL DE440/DE440t validation data (approx 210 MB)" OFF)
    set(LIBJPL_EPHEMERIS_DIR "${CMAKE_CURRENT_BINARY_DIR}/ephemeris" CACHE PATH "Downloaded/located JPL test data")
    # Source root for the shared module below: required when the build tree
    # lives outside the source checkout (module-relative lookup alone can
    # resolve elsewhere).
    set(LIBJPL_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    # Shared manifest/resolver (T20/L06): production fetch tier and the
    # offline jpl_fetch_cache script tests include the exact same module.
    # Fetch OFF keeps every configure offline: no download URL is touched,
    # and no redirect directory under this block is evaluated (nothing below
    # "if(LIBJPL_FETCH_EPHEMERIDES)" runs; that non-evaluation is structural
    # and asserted only as the absence of download activity, not as a
    # resolver-level unit test).
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibjplTestData.cmake")
    # jpl_fetch_cache exercises the production resolver above against
    # disposable build-tree artifacts (four-byte payload, file URLs,
    # renames) instead of large transfers; FETCH OFF therefore runs it
    # with no network. Controlled source/manifest locations arrive via
    # -D (never the caller's current directory). The driver passes the
    # real source root explicitly so build trees outside the source tree
    # resolve the manifest correctly.
    set(JPL_FETCH_CACHE_WORK "${CMAKE_CURRENT_BINARY_DIR}/fetch_cache")
    add_test(NAME jpl_fetch_cache
        COMMAND ${CMAKE_COMMAND}
            "-DRESOLVER=${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibjplTestData.cmake"
            "-DLIBJPL_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DWORK=${JPL_FETCH_CACHE_WORK}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_fetch_cache.cmake")

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

        # Download values come from tests/data/MANIFEST.txt (URLs/sizes
        # verified 2026-09-18, binary SHA-256 verified 2026-09-19 against the
        # exact downloaded artifacts); CMakeLists carries no literals for them.
        libjpl_manifest_value(LIBJPL_DE440_URL "de440.bin.url")
        libjpl_manifest_value(LIBJPL_DE440_SIZE "de440.bin.size")
        libjpl_manifest_value(LIBJPL_DE440_SHA256 "de440.bin.sha256")
        libjpl_manifest_value(LIBJPL_DE440T_URL "de440t.bin.url")
        libjpl_manifest_value(LIBJPL_DE440T_SIZE "de440t.bin.size")
        libjpl_manifest_value(LIBJPL_DE440T_SHA256 "de440t.bin.sha256")
        libjpl_manifest_value(LIBJPL_TESTPO440_URL "testpo.440.url")
        libjpl_manifest_value(LIBJPL_TESTPO440_SHA256 "testpo.440.sha256")
        libjpl_manifest_value(LIBJPL_TESTPO440T_URL "testpo.440t.url")
        libjpl_manifest_value(LIBJPL_TESTPO440T_SHA256 "testpo.440t.sha256")
        libjpl_manifest_value(LIBJPL_TESTPO_SIZE "testpo.440.size")
        libjpl_manifest_value(LIBJPL_TESTPO440T_SIZE "testpo.440t.size")

        if(NOT "$ENV{JPLEPH}" STREQUAL "")
            set(LIBJPL_JPLEPH_SRC_DIR "$ENV{JPLEPH}")
        else()
            set(LIBJPL_JPLEPH_SRC_DIR "")
        endif()

        # Renamed on download to DE<nnn>.eph so suite pairing stays uniform.
        libjpl_resolve_fetch_file(LIBJPL_DE440_EPH "${LIBJPL_JPLEPH_SRC_DIR}" "DE440.eph"
            "${LIBJPL_DE440_URL}" "DE440.eph" "${LIBJPL_DE440_SIZE}" "${LIBJPL_DE440_SHA256}")
        libjpl_resolve_fetch_file(LIBJPL_DE440T_EPH "${LIBJPL_JPLEPH_SRC_DIR}" "DE440t.eph"
            "${LIBJPL_DE440T_URL}" "DE440t.eph" "${LIBJPL_DE440T_SIZE}" "${LIBJPL_DE440T_SHA256}")
        libjpl_resolve_fetch_file(LIBJPL_TESTPO_440_FILE "${JPL_TESTPO_DIR}" "testpo.440"
            "${LIBJPL_TESTPO440_URL}" "testpo.440" "${LIBJPL_TESTPO_SIZE}" "${LIBJPL_TESTPO440_SHA256}")
        libjpl_resolve_fetch_file(LIBJPL_TESTPO_440T_FILE "${JPL_TESTPO_DIR}" "testpo.440t"
            "${LIBJPL_TESTPO440T_URL}" "testpo.440t" "${LIBJPL_TESTPO440T_SIZE}" "${LIBJPL_TESTPO440T_SHA256}")

        # Single testpo runner: one jpl_testpo_tests binary serves every
        # DE registration below (fixed 440/440t plus the sweep). The
        # duplicate-name skip in the sweep loop (440/440t) guards add_test
        # name collision, which CMake treats as FATAL_ERROR.
        # (The binary is defined once above so offline builds compile every
        # test source; only registrations are gated.)
        target_include_directories(jpl_testpo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        add_test(NAME jpl_testpo_440
            COMMAND jpl_testpo_tests "${LIBJPL_DE440_EPH}" "${LIBJPL_TESTPO_440_FILE}")
        add_test(NAME jpl_testpo_440t
            COMMAND jpl_testpo_tests "${LIBJPL_DE440T_EPH}" "${LIBJPL_TESTPO_440T_FILE}")
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
        add_test(NAME jpl_real_ephemeris
            COMMAND jpl_real_ephemeris_tests "${LIBJPL_DE440_EPH}")

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

    # Fetch-tier sweep over the remaining testpo files: maintainer-local
    # binaries in JPLEPH paired with testpo files in JPL_TESTPO_DIR
    # (assigned once above; env override else the local default). Gated on
    # the option so plain configures never scan; per-file EXISTS checks let
    # a partial local dir compose with the downloads resolved above. All
    # registrations share the single jpl_testpo_tests binary built above.
    if(LIBJPL_FETCH_EPHEMERIDES)
        file(GLOB JPL_TESTPO_FILES "${JPL_TESTPO_DIR}/testpo.*")
        foreach(testpo_file IN LISTS JPL_TESTPO_FILES)
            get_filename_component(testpo_name "${testpo_file}" NAME)
            string(REGEX REPLACE "^testpo\\." "" testpo_num "${testpo_name}")
            # 440/440t are registered by the fixed fetch-tier block above;
            # skipping them here avoids an add_test name collision
            # (duplicate add_test names are FATAL_ERROR).
            if(testpo_num STREQUAL "440" OR testpo_num STREQUAL "440t")
                continue()
            endif()
            if(testpo_num MATCHES "^[0-9]+t?$" AND EXISTS "$ENV{JPLEPH}/DE${testpo_num}.eph")
                add_test(NAME jpl_testpo_${testpo_num}
                    COMMAND jpl_testpo_tests "$ENV{JPLEPH}/DE${testpo_num}.eph" "${testpo_file}")
            endif()
        endforeach()
    endif()

    # Warning flags for all unit-test executables (incl. integration ones
    # registered above). The NOT TARGET guard keeps this safe if a target
    # is ever removed.
    foreach(tgt IN ITEMS jpl_tests jpl_lighttime_tests jpl_aberration_tests jpl_frames_tests jpl_convenience_tests jpl_mmap_tests jpl_threads_tests jpl_tt_tdb_tests jpl_parser_tests jpl_api_errors_tests jpl_dates_tests jpl_largefile jpl_testpo_parser_tests jpl_numeric_tests jpl_examples jpl_generate_reference jpl_fixture_tests jpl_fixture_fault_tests jpl_testpo_tests jpl_testpo_fault_tests jpl_validator_data jpl_real_ephemeris_tests jpl_all_ephemerides_tests)
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

    # Installed package (T19): relocatable find_package config plus a
    # separate-driver C/C++ consumer covering default, version-mismatch,
    # relocated/spaces, custom-destination, and PIC paths. The driver
    # derives notice paths from JPL_NOTICE_DATADIR exactly; no install-path
    # literal is duplicated here. Toolchain settings flow to child
    # configurations; empty generator platform/toolset are omitted.
    string(REPLACE "/licenses/libjpl" "" JPL_INSTALL_DATADIR "${JPL_NOTICE_DATADIR}")
    set(JPL_INSTALL_GENERATOR "${CMAKE_GENERATOR}")
    set(JPL_INSTALL_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
    set(JPL_INSTALL_TOOLSET "${CMAKE_GENERATOR_TOOLSET}")
    set(JPL_INSTALL_TOOLCHAIN "")
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        set(JPL_INSTALL_TOOLCHAIN "${CMAKE_TOOLCHAIN_FILE}")
    endif()
    set(JPL_INSTALL_C_COMPILER "${CMAKE_C_COMPILER}")
    set(JPL_INSTALL_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    set(JPL_INSTALL_CXX_COMPILER "")
    if(DEFINED CMAKE_CXX_COMPILER AND NOT "${CMAKE_CXX_COMPILER}" STREQUAL "")
        set(JPL_INSTALL_CXX_COMPILER "${CMAKE_CXX_COMPILER}")
    endif()
    if(LIBJPL_PIC)
        set(JPL_INSTALL_PIC_IS_ON 1)
    else()
        set(JPL_INSTALL_PIC_IS_ON 0)
    endif()
    if("${JPL_NEED_M_LIB}" STREQUAL "m")
        set(JPL_INSTALL_NEED_M "m")
    else()
        set(JPL_INSTALL_NEED_M "")
    endif()
    add_test(NAME jpl_install
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DBUILD=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCONFIG=$<CONFIG>"
            "-DGENERATOR=${JPL_INSTALL_GENERATOR}"
            "-DVERSION=${PROJECT_VERSION}"
            "-DPLATFORM=${JPL_INSTALL_PLATFORM}"
            "-DTOOLSET=${JPL_INSTALL_TOOLSET}"
            "-DTOOLCHAIN_FILE=${JPL_INSTALL_TOOLCHAIN}"
            "-DC_COMPILER=${JPL_INSTALL_C_COMPILER}"
            "-DCXX_COMPILER=${JPL_INSTALL_CXX_COMPILER}"
            "-DBUILD_TYPE=${JPL_INSTALL_BUILD_TYPE}"
            "-DLIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DINCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
            "-DDATADIR=${JPL_INSTALL_DATADIR}"
            "-DNEED_M=${JPL_INSTALL_NEED_M}"
            "-DPIC_IS_ON=${JPL_INSTALL_PIC_IS_ON}"
            "-DCHECKER=${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_licenses.cmake"
            "-DINVENTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests/license_inventory.cmake"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/test_install.cmake")
endif()
