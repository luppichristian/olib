cmake_minimum_required(VERSION 3.19)
project(olib VERSION 1.0.0 LANGUAGES C CXX)

#
# Configuration Options
#

option(OLIB_BUILD_SHARED "Build olib as a shared library" OFF)
option(OLIB_BUILD_TESTS "Build olib tests" ON)
option(OLIB_BUILD_EXAMPLES "Build olib examples" ON)
option(OLIB_BUILD_CLI "Build olib-convert CLI utility" ON)
option(OLIB_GTEST_FETCH "Fetch GoogleTest if not found" ON)

#
# C Standard
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

#
# C++ Standard (for tests)
#

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

#
# Library Sources
#

file(GLOB_RECURSE OLIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)
file(GLOB_RECURSE OLIB_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

#
# Test Sources
#

file(GLOB_RECURSE OLIB_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

#
# Build Library (Static or Shared)
#

if(OLIB_BUILD_SHARED)
    add_library(olib SHARED ${OLIB_SOURCES} ${OLIB_HEADERS})
    target_compile_definitions(olib PRIVATE OLIB_EXPORTS)
    target_compile_definitions(olib PUBLIC OLIB_SHARED)
    set_target_properties(olib PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
else()
    add_library(olib-static STATIC ${OLIB_SOURCES} ${OLIB_HEADERS})
    add_library(olib::static ALIAS olib-static)
    set_target_properties(olib-static PROPERTIES
        VERSION ${PROJECT_VERSION}
        OUTPUT_NAME olib
    )
endif()

#
# Set Common Properties for the Active Target
#

if(OLIB_BUILD_SHARED)
    set(OLIB_TARGET olib)
else()
    set(OLIB_TARGET olib-static)
endif()

target_include_directories(${OLIB_TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

#
# Tests (Optional)
#

if(OLIB_BUILD_TESTS)
    if(OLIB_GTEST_FETCH)
        message(STATUS "Fetching GoogleTest framework...")

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    enable_testing()

    add_executable(olib-tests
        ${OLIB_TEST_SOURCES}
    )

    target_include_directories(olib-tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(olib-tests
        PRIVATE
            ${OLIB_TARGET}
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(olib-tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    message(STATUS "Building tests with GoogleTest")
endif()

#
# Examples (Optional)
#

if(OLIB_BUILD_EXAMPLES)
    # List of example programs
    set(EXAMPLE_PROGRAMS
        basic
        json
        yaml
        xml
        toml
        text
        binary
    )

    # Create executable for each example
    foreach(example ${EXAMPLE_PROGRAMS})
        add_executable(olib-example-${example}
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/${example}.c
        )

        target_include_directories(olib-example-${example}
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/examples
        )

        target_link_libraries(olib-example-${example}
            PRIVATE
                ${OLIB_TARGET}
        )

        # Set output directory for examples
        set_target_properties(olib-example-${example} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endforeach()

    message(STATUS "Building examples")
endif()

#
# CLI Utility (Optional)
#

if(OLIB_BUILD_CLI)
    add_executable(olib-convert
        ${CMAKE_CURRENT_SOURCE_DIR}/cli/main.c
    )

    target_include_directories(olib-convert
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(olib-convert
        PRIVATE
            ${OLIB_TARGET}
    )

    # Set output directory for CLI utility
    set_target_properties(olib-convert PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Install the CLI utility
    install(TARGETS olib-convert
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    message(STATUS "Building CLI utility (olib-convert)")
endif()

#
# Installation
#

include(GNUInstallDirs)

install(TARGETS ${OLIB_TARGET}
    EXPORT olibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${OLIB_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT olibTargets
    FILE olibTargets.cmake
    NAMESPACE olib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/olib
)
