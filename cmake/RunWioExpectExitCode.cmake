if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_EXPECT_EXIT_CODE)
    message(FATAL_ERROR "WIO_EXPECT_EXIT_CODE was not provided.")
endif()

execute_process(
    COMMAND "${WIO_EXE}" file run "${WIO_SOURCE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")
if(NOT wio_result EQUAL ${WIO_EXPECT_EXIT_CODE})
    message(FATAL_ERROR
        "Expected file run exit code ${WIO_EXPECT_EXIT_CODE}, but received ${wio_result}.\n"
        "Tool output:\n${wio_output}"
    )
endif()

message(STATUS "Observed expected file run exit code ${WIO_EXPECT_EXIT_CODE}.")
