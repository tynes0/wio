if(NOT DEFINED WIO_EXE OR WIO_EXE STREQUAL "")
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()
if(NOT DEFINED WIO_BUILD_DIR OR WIO_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_BUILD_DIR was not provided.")
endif()
if(NOT DEFINED WIO_CONFIG OR WIO_CONFIG STREQUAL "")
    set(WIO_CONFIG Debug)
endif()

function(run_dev_success label)
    execute_process(
        COMMAND "${WIO_EXE}" ${ARGN}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_stdout
        ERROR_VARIABLE command_stderr
    )
    set(command_output "${command_stdout}${command_stderr}")
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with code ${command_result}.\n${command_output}"
        )
    endif()
    set(LAST_OUTPUT "${command_output}" PARENT_SCOPE)
endfunction()

run_dev_success(
    "Self-hosted repository build"
    build --build-dir "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
)

run_dev_success(
    "Self-hosted repository test listing"
    test --build-dir "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
         --list --filter "^wio_test_std_environment_statistics_run$"
)
if(NOT LAST_OUTPUT MATCHES "wio_test_std_environment_statistics_run")
    message(FATAL_ERROR "Repository test listing did not contain the requested test.\n${LAST_OUTPUT}")
endif()

run_dev_success(
    "Self-hosted dev alias test listing"
    dev test --build-dir "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
             --list --filter "^wio_test_std_environment_statistics_run$"
)
if(NOT LAST_OUTPUT MATCHES "wio_test_std_environment_statistics_run")
    message(FATAL_ERROR "Dev alias did not preserve the test filter.\n${LAST_OUTPUT}")
endif()

message(STATUS "Self-hosted repository build/test/dev smoke succeeded.")
