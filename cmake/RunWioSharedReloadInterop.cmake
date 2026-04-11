if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_OUTPUT_A)
    message(FATAL_ERROR "WIO_OUTPUT_A was not provided.")
endif()

if(NOT DEFINED WIO_OUTPUT_B)
    message(FATAL_ERROR "WIO_OUTPUT_B was not provided.")
endif()

if(NOT DEFINED WIO_HOST_SOURCE)
    message(FATAL_ERROR "WIO_HOST_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_HOST_CXX)
    message(FATAL_ERROR "WIO_HOST_CXX was not provided.")
endif()

if(NOT DEFINED WIO_EXPECT)
    message(FATAL_ERROR "WIO_EXPECT was not provided.")
endif()

if(NOT DEFINED WIO_ARGS OR WIO_ARGS STREQUAL "")
    set(WIO_ARGS)
endif()

get_filename_component(wio_output_a_dir "${WIO_OUTPUT_A}" DIRECTORY)
get_filename_component(wio_output_b_dir "${WIO_OUTPUT_B}" DIRECTORY)
file(MAKE_DIRECTORY "${wio_output_a_dir}")
file(MAKE_DIRECTORY "${wio_output_b_dir}")

execute_process(
    COMMAND "${WIO_EXE}" "${WIO_SOURCE}" --target shared --output "${WIO_OUTPUT_A}" ${WIO_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result_a
    OUTPUT_VARIABLE wio_stdout_a
    ERROR_VARIABLE wio_stderr_a
)

set(wio_output_a "${wio_stdout_a}${wio_stderr_a}")

if(NOT wio_result_a EQUAL 0)
    message(FATAL_ERROR
        "Expected first shared module build to succeed for '${WIO_SOURCE}', but it failed with code ${wio_result_a}.\n"
        "Compiler output:\n${wio_output_a}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" "${WIO_SOURCE}" --target shared --output "${WIO_OUTPUT_B}" ${WIO_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result_b
    OUTPUT_VARIABLE wio_stdout_b
    ERROR_VARIABLE wio_stderr_b
)

set(wio_output_b "${wio_stdout_b}${wio_stderr_b}")

if(NOT wio_result_b EQUAL 0)
    message(FATAL_ERROR
        "Expected second shared module build to succeed for '${WIO_SOURCE}', but it failed with code ${wio_result_b}.\n"
        "Compiler output:\n${wio_output_b}"
    )
endif()

if(WIN32)
    set(host_exe "${WIO_OUTPUT_A}.reload.host.exe")
else()
    set(host_exe "${WIO_OUTPUT_A}.reload.host")
endif()

set(host_build_command
    "${WIO_HOST_CXX}"
    -std=c++20
    "${WIO_HOST_SOURCE}"
    -o "${host_exe}"
)

if(APPLE OR UNIX)
    list(APPEND host_build_command -ldl)
endif()

execute_process(
    COMMAND ${host_build_command}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE host_build_result
    OUTPUT_VARIABLE host_build_stdout
    ERROR_VARIABLE host_build_stderr
)

set(host_build_output "${host_build_stdout}${host_build_stderr}")

if(NOT host_build_result EQUAL 0)
    message(FATAL_ERROR
        "Expected host reload build to succeed for '${WIO_HOST_SOURCE}', but it failed with code ${host_build_result}.\n"
        "Host compiler output:\n${host_build_output}"
    )
endif()

execute_process(
    COMMAND "${host_exe}" "${WIO_OUTPUT_A}" "${WIO_OUTPUT_B}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE host_run_result
    OUTPUT_VARIABLE host_run_stdout
    ERROR_VARIABLE host_run_stderr
)

set(host_run_output "${host_run_stdout}${host_run_stderr}")

if(NOT host_run_result EQUAL 0)
    message(FATAL_ERROR
        "Expected host reload executable '${host_exe}' to run successfully, but it failed with code ${host_run_result}.\n"
        "Host run output:\n${host_run_output}"
    )
endif()

string(REGEX MATCH "${WIO_EXPECT}" host_match "${host_run_output}")

if(NOT host_match)
    message(FATAL_ERROR
        "Expected host reload output matching '${WIO_EXPECT}', but it was not found.\n"
        "Host run output:\n${host_run_output}"
    )
endif()

message(STATUS "Observed expected shared reload host output for: ${WIO_SOURCE}")
