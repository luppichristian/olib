cmake_minimum_required(VERSION 3.19)
project(olib_install_consumer LANGUAGES C CXX)

if(NOT EXISTS "${OLIB_TARGETS_FILE}")
    message(FATAL_ERROR "Installed target export not found: ${OLIB_TARGETS_FILE}")
endif()
include("${OLIB_TARGETS_FILE}")

if(OLIB_SHARED)
    set(OLIB_IMPORTED_TARGET olib::olib)
else()
    set(OLIB_IMPORTED_TARGET olib::olib-static)
endif()

add_executable(consumer_c main.c)
set_target_properties(consumer_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(consumer_c PRIVATE ${OLIB_IMPORTED_TARGET})

add_executable(consumer_cpp main.cc)
set_target_properties(consumer_cpp PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_link_libraries(consumer_cpp PRIVATE ${OLIB_IMPORTED_TARGET})

enable_testing()
add_test(NAME installed_c COMMAND consumer_c)
add_test(NAME installed_cpp COMMAND consumer_cpp)
