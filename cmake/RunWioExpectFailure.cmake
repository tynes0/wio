if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_EXPECT)
    message(FATAL_ERROR "WIO_EXPECT was not provided.")
endif()

execute_process(
    COMMAND "${WIO_EXE}" "${WIO_SOURCE}" --dry-run
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")

if(wio_result EQUAL 0)
    message(FATAL_ERROR
        "Expected compilation to fail for '${WIO_SOURCE}', but it succeeded.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

string(REGEX MATCH "${WIO_EXPECT}" wio_match "${wio_output}")

if(NOT wio_match)
    message(FATAL_ERROR
        "Expected failure output matching '${WIO_EXPECT}' for '${WIO_SOURCE}', but it was not found.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

message(STATUS "Observed expected Wio compiler failure for: ${WIO_SOURCE}")
