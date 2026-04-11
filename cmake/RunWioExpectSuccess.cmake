if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_EXPECT)
    message(FATAL_ERROR "WIO_EXPECT was not provided.")
endif()

if(NOT DEFINED WIO_ARGS OR WIO_ARGS STREQUAL "")
    set(WIO_ARGS --dry-run)
endif()

execute_process(
    COMMAND "${WIO_EXE}" "${WIO_SOURCE}" ${WIO_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")

if(NOT wio_result EQUAL 0)
    message(FATAL_ERROR
        "Expected compilation to succeed for '${WIO_SOURCE}', but it failed with code ${wio_result}.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

string(REGEX MATCH "${WIO_EXPECT}" wio_match "${wio_output}")

if(NOT wio_match)
    message(FATAL_ERROR
        "Expected output matching '${WIO_EXPECT}' for '${WIO_SOURCE}', but it was not found.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

message(STATUS "Observed expected Wio compiler output for: ${WIO_SOURCE}")
