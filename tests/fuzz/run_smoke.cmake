foreach(required FUZZ_EXECUTABLE SOURCE_DIR BINARY_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# libFuzzer writes new inputs to its corpus. Work in the build tree so source
# samples and regression seeds remain unchanged between runs.
set(corpus "${BINARY_DIR}/fuzz-smoke-corpus")
file(REMOVE_RECURSE "${corpus}")
file(MAKE_DIRECTORY "${corpus}")
file(COPY "${SOURCE_DIR}/samples/" "${SOURCE_DIR}/tests/fuzz/seeds/" DESTINATION "${corpus}")
execute_process(COMMAND "${FUZZ_EXECUTABLE}" -seed=1 -runs=1000 -max_len=4096
    -timeout=5 "-artifact_prefix=${BINARY_DIR}/" "${corpus}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Parser fuzz smoke failed: ${result}")
endif()
