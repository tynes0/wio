if(NOT DEFINED WIO_EXE OR WIO_EXE STREQUAL "")
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()
if(NOT DEFINED WIO_SOURCE OR WIO_SOURCE STREQUAL "")
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

function(run_success label)
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

run_success("Self-hosted companion probe" --self-hosted-info)
if(NOT LAST_OUTPUT MATCHES "Argonaut-Wio bootstrap")
    message(FATAL_ERROR "Self-hosted marker was missing.\n${LAST_OUTPUT}")
endif()

run_success("Self-hosted root help" --help)
if(NOT LAST_OUTPUT MATCHES "Wio command line interface")
    message(FATAL_ERROR "Root help did not survive self-hosted routing.\n${LAST_OUTPUT}")
endif()

run_success("Self-hosted version" --version)
if(NOT LAST_OUTPUT MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "Version did not survive self-hosted routing.\n${LAST_OUTPUT}")
endif()

execute_process(
    COMMAND "${WIO_EXE}" project package --selfhost-invalid-option
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_stderr
)
set(invalid_output "${invalid_stdout}${invalid_stderr}")
if(invalid_result EQUAL 0 OR
   NOT invalid_output MATCHES "Argonaut: Undefined argument: --selfhost-invalid-option")
    message(FATAL_ERROR
        "Project package was not validated by Argonaut-Wio.\n${invalid_output}"
    )
endif()

run_success("Native CLI recursion bridge" --native-cli --version)
run_success("Raw stage-0 compiler path" "${WIO_SOURCE}" --dry-run)

message(STATUS "Self-hosted CLI bootstrap smoke succeeded.")
