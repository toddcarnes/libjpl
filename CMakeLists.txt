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

    # test_positions.c was superseded by the testpo harness below (Task 8):
    # tests/de441_reference.txt never existed, so jpl_position_tests could
    # not be wired to it. tests/test_testpo.c validates every testpo line
    # (including DE441 geometric states) instead.

    # Real ephemerides are optional local test data. When JPLEPH names a
    # directory containing DE441.eph, exercise the reader against it as an
    # integration test. Evaluated at configure time: re-run cmake -S/-B
    # after setting JPLEPH for newly added data to register.
    if(NOT "$ENV{JPLEPH}" STREQUAL "" AND EXISTS "$ENV{JPLEPH}/DE441.eph")
        add_executable(jpl_real_ephemeris_tests tests/test_real_ephemeris.c)
        target_link_libraries(jpl_real_ephemeris_tests PRIVATE jpl)
        target_compile_features(jpl_real_ephemeris_tests PRIVATE c_std_17)
        add_test(NAME jpl_real_de441
            COMMAND jpl_real_ephemeris_tests "$ENV{JPLEPH}/DE441.eph")


        add_executable(jpl_all_ephemerides_tests tests/test_all_ephemerides.c)
        target_link_libraries(jpl_all_ephemerides_tests PRIVATE jpl)
        target_compile_features(jpl_all_ephemerides_tests PRIVATE c_std_17)
        add_test(NAME jpl_all_ephemerides
            COMMAND jpl_all_ephemerides_tests "$ENV{JPLEPH}")
    endif()

    # JPL testpo reference-data validation. Each testpo.NNN file pairs with
    # the DENNN.eph binary in $JPLEPH; the runner (tests/test_testpo.c)
    # validates every data line via jpl_get_state (geometric, TDB) with
    # tolerance 5e-13 * fmax(1, |expected|). Configure-time registration:
    # re-run cmake -S/-B after adding binaries to pick up new pairs.
    # JPL_TESTPO env var overrides the default testpo data directory.
    if(DEFINED ENV{JPL_TESTPO} AND NOT "$ENV{JPL_TESTPO}" STREQUAL "")
        set(JPL_TESTPO_DIR "$ENV{JPL_TESTPO}")
    else()
        set(JPL_TESTPO_DIR "D:/ephem/jpl/testpo")
    endif()
    if(NOT "$ENV{JPLEPH}" STREQUAL "")
        file(GLOB JPL_TESTPO_FILES "${JPL_TESTPO_DIR}/testpo.*")
        foreach(testpo_file IN LISTS JPL_TESTPO_FILES)
            get_filename_component(testpo_name "${testpo_file}" NAME)
            string(REGEX REPLACE "^testpo\\." "" testpo_num "${testpo_name}")
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
    foreach(tgt IN ITEMS jpl_tests jpl_lighttime_tests jpl_aberration_tests jpl_frames_tests jpl_convenience_tests jpl_mmap_tests jpl_real_ephemeris_tests jpl_all_ephemerides_tests)
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
