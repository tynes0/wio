if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED EXAMPLE_ROOT OR EXAMPLE_ROOT STREQUAL "")
    message(FATAL_ERROR "EXAMPLE_ROOT was not provided.")
endif()

if(NOT DEFINED WIO_EXPECT OR WIO_EXPECT STREQUAL "")
    message(FATAL_ERROR "WIO_EXPECT was not provided.")
endif()

execute_process(
    COMMAND "${WIO_EXE}" project describe --project "${EXAMPLE_ROOT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE describe_result
    OUTPUT_VARIABLE describe_stdout
    ERROR_VARIABLE describe_stderr
)
set(describe_output "${describe_stdout}${describe_stderr}")
if(NOT describe_result EQUAL 0)
    message(FATAL_ERROR
        "Example project describe failed for '${EXAMPLE_ROOT}' with code ${describe_result}.\n"
        "Tool output:\n${describe_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project build --project "${EXAMPLE_ROOT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
set(build_output "${build_stdout}${build_stderr}")
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Example project build failed for '${EXAMPLE_ROOT}' with code ${build_result}.\n"
        "Tool output:\n${build_output}"
    )
endif()

set(run_command "${WIO_EXE}" project run --project "${EXAMPLE_ROOT}")
if(DEFINED WIO_RUN_ARGS AND NOT WIO_RUN_ARGS STREQUAL "")
    list(APPEND run_command -- ${WIO_RUN_ARGS})
endif()

execute_process(
    COMMAND ${run_command}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)
set(run_output "${run_stdout}${run_stderr}")
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "Example project run failed for '${EXAMPLE_ROOT}' with code ${run_result}.\n"
        "Tool output:\n${run_output}"
    )
endif()

string(FIND "${run_output}" "${WIO_EXPECT}" expected_index)
if(expected_index EQUAL -1)
    message(FATAL_ERROR
        "Example project output did not contain the expected text '${WIO_EXPECT}'.\n"
        "Tool output:\n${run_output}"
    )
endif()

message(STATUS "Example project smoke succeeded for ${EXAMPLE_ROOT}")
