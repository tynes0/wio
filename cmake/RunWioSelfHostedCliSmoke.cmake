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

run_success("Self-hosted empty invocation")
if(NOT LAST_OUTPUT MATCHES "Wio command line interface")
    message(FATAL_ERROR "Empty invocation was not handled by Wio.\n${LAST_OUTPUT}")
endif()

run_success("Self-hosted nested help" help project run)
if(NOT LAST_OUTPUT MATCHES "Usage: wio project run")
    message(FATAL_ERROR "Nested help was not rewritten by Wio.\n${LAST_OUTPUT}")
endif()

run_success("Self-hosted version" --version)
if(NOT LAST_OUTPUT MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "Version did not survive self-hosted routing.\n${LAST_OUTPUT}")
endif()

execute_process(
    COMMAND "${WIO_EXE}" projec
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE typo_result
    OUTPUT_VARIABLE typo_stdout
    ERROR_VARIABLE typo_stderr
)
set(typo_output "${typo_stdout}${typo_stderr}")
if(typo_result EQUAL 0 OR
   NOT typo_output MATCHES "Did you mean 'wio project'")
    message(FATAL_ERROR
        "Top-level command suggestions were not handled by Wio.\n${typo_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project bild
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE project_typo_result
    OUTPUT_VARIABLE project_typo_stdout
    ERROR_VARIABLE project_typo_stderr
)
set(project_typo_output "${project_typo_stdout}${project_typo_stderr}")
if(project_typo_result EQUAL 0 OR
   NOT project_typo_output MATCHES "Did you mean 'wio project build'")
    message(FATAL_ERROR
        "Project command suggestions were not handled by Wio.\n${project_typo_output}"
    )
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

execute_process(
    COMMAND "${WIO_EXE}" build --selfhost-invalid-option
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE build_invalid_result
    OUTPUT_VARIABLE build_invalid_stdout
    ERROR_VARIABLE build_invalid_stderr
)
set(build_invalid_output "${build_invalid_stdout}${build_invalid_stderr}")
if(build_invalid_result EQUAL 0 OR
   NOT build_invalid_output MATCHES "Argonaut: Undefined argument: --selfhost-invalid-option")
    message(FATAL_ERROR
        "Repository build was not validated by Argonaut-Wio.\n${build_invalid_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" perf smoke --selfhost-invalid-option
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE perf_invalid_result
    OUTPUT_VARIABLE perf_invalid_stdout
    ERROR_VARIABLE perf_invalid_stderr
)
set(perf_invalid_output "${perf_invalid_stdout}${perf_invalid_stderr}")
if(perf_invalid_result EQUAL 0 OR
   NOT perf_invalid_output MATCHES "Argonaut: Undefined argument: --selfhost-invalid-option")
    message(FATAL_ERROR
        "Performance smoke was not validated by Argonaut-Wio.\n${perf_invalid_output}"
    )
endif()

run_success("Native CLI recursion bridge" --native-cli --version)
run_success("Raw stage-0 compiler path" "${WIO_SOURCE}" --dry-run)

message(STATUS "Self-hosted CLI bootstrap smoke succeeded.")
