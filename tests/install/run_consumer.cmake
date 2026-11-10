# Install the already-built library and exercise it from an unrelated project.
foreach(required SOURCE_DIR BINARY_DIR GENERATOR INSTALL_LIBDIR INSTALL_BINDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(prefix "${BINARY_DIR}/install-consumer-prefix")
set(consumer_build "${BINARY_DIR}/install-consumer-build")
file(REMOVE_RECURSE "${prefix}" "${consumer_build}")

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed with status ${result}: ${ARGV}")
    endif()
endfunction()

set(config_args)
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND config_args --config "${CONFIG}")
endif()
run_checked("${CMAKE_COMMAND}" --install "${BINARY_DIR}" --prefix "${prefix}" --component olib ${config_args})

set(generator_args -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
    list(APPEND generator_args -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
    list(APPEND generator_args -T "${GENERATOR_TOOLSET}")
endif()
run_checked("${CMAKE_COMMAND}" -S "${SOURCE_DIR}/tests/install/consumer" -B "${consumer_build}"
    ${generator_args} "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DOLIB_SHARED=${OLIB_SHARED}"
    "-DOLIB_TARGETS_FILE=${prefix}/${INSTALL_LIBDIR}/cmake/olib/olibTargets.cmake")
run_checked("${CMAKE_COMMAND}" --build "${consumer_build}" ${config_args})

# On Windows the installed DLL lives in bin, separate from the executable.
if(WIN32)
    set(ENV{PATH} "${prefix}/${INSTALL_BINDIR};$ENV{PATH}")
endif()
set(ctest_args --test-dir "${consumer_build}" --output-on-failure)
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND ctest_args -C "${CONFIG}")
endif()
run_checked("${CMAKE_CTEST_COMMAND}" ${ctest_args})
