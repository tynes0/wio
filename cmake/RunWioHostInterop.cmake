if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_OUTPUT)
    message(FATAL_ERROR "WIO_OUTPUT was not provided.")
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

if(NOT DEFINED WIO_TARGET OR WIO_TARGET STREQUAL "")
    set(WIO_TARGET static)
endif()

if(NOT DEFINED WIO_ARGS OR WIO_ARGS STREQUAL "")
    set(WIO_ARGS)
endif()

if(NOT DEFINED WIO_HOST_LINK_WITH_WIO_OUTPUT OR WIO_HOST_LINK_WITH_WIO_OUTPUT STREQUAL "")
    set(WIO_HOST_LINK_WITH_WIO_OUTPUT ON)
endif()

if(NOT DEFINED WIO_HOST_RUNTIME_ARGS OR WIO_HOST_RUNTIME_ARGS STREQUAL "")
    set(WIO_HOST_RUNTIME_ARGS)
endif()

if(NOT DEFINED WIO_HOST_LINK_ARGS OR WIO_HOST_LINK_ARGS STREQUAL "")
    set(WIO_HOST_LINK_ARGS)
endif()

get_filename_component(wio_output_dir "${WIO_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${wio_output_dir}")

execute_process(
    COMMAND "${WIO_EXE}" "${WIO_SOURCE}" --target "${WIO_TARGET}" --output "${WIO_OUTPUT}" ${WIO_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")

if(NOT wio_result EQUAL 0)
    message(FATAL_ERROR
        "Expected Wio host-interop compilation to succeed for '${WIO_SOURCE}', but it failed with code ${wio_result}.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

if(NOT EXISTS "${WIO_OUTPUT}")
    message(FATAL_ERROR
        "Expected host-interop library output to exist at '${WIO_OUTPUT}', but it was not created.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

if(WIN32)
    set(host_exe "${WIO_OUTPUT}.host.exe")
else()
    set(host_exe "${WIO_OUTPUT}.host")
endif()

set(host_build_command
    "${WIO_HOST_CXX}"
    -std=c++20
    -I
    "${CMAKE_SOURCE_DIR}/compiler/include/runtime"
    "${WIO_HOST_SOURCE}"
)

if(WIO_HOST_LINK_WITH_WIO_OUTPUT)
    list(APPEND host_build_command "${WIO_OUTPUT}")
endif()

if(WIO_HOST_LINK_ARGS)
    list(APPEND host_build_command ${WIO_HOST_LINK_ARGS})
endif()

list(APPEND host_build_command -o "${host_exe}")

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
        "Expected host C++ build to succeed for '${WIO_HOST_SOURCE}', but it failed with code ${host_build_result}.\n"
        "Host compiler output:\n${host_build_output}"
    )
endif()

execute_process(
    COMMAND "${host_exe}" ${WIO_HOST_RUNTIME_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE host_run_result
    OUTPUT_VARIABLE host_run_stdout
    ERROR_VARIABLE host_run_stderr
)

set(host_run_output "${host_run_stdout}${host_run_stderr}")

if(NOT host_run_result EQUAL 0)
    message(FATAL_ERROR
        "Expected host executable '${host_exe}' to run successfully, but it failed with code ${host_run_result}.\n"
        "Host run output:\n${host_run_output}"
    )
endif()

string(REGEX MATCH "${WIO_EXPECT}" host_match "${host_run_output}")

if(NOT host_match)
    message(FATAL_ERROR
        "Expected host output matching '${WIO_EXPECT}', but it was not found.\n"
        "Host run output:\n${host_run_output}"
    )
endif()

message(STATUS "Observed expected host interop output for: ${WIO_SOURCE}")
