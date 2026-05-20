if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

execute_process(
    COMMAND "${WIO_EXE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")

if(NOT wio_result EQUAL 0)
    message(FATAL_ERROR
        "CLI help smoke failed for bare 'wio' invocation with code ${wio_result}.\n"
        "Tool output:\n${wio_output}"
    )
endif()

string(FIND "${wio_output}" "--help" help_index)
string(FIND "${wio_output}" "Usage:" usage_index)
if(help_index EQUAL -1 AND usage_index EQUAL -1)
    message(FATAL_ERROR
        "CLI help smoke for bare 'wio' invocation did not print help text.\n"
        "Tool output:\n${wio_output}"
    )
endif()

set(commands
    "build|--help"
    "test|--help"
    "file|--help"
    "project|--help"
    "bind|--help"
    "env|--help"
    "package|--help"
    "perf|--help"
)

foreach(command_spec IN LISTS commands)
    string(REPLACE "|" ";" command_parts "${command_spec}")
    string(REPLACE "|" " " command_label "${command_spec}")

    execute_process(
        COMMAND "${WIO_EXE}" ${command_parts}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE wio_result
        OUTPUT_VARIABLE wio_stdout
        ERROR_VARIABLE wio_stderr
    )

    set(wio_output "${wio_stdout}${wio_stderr}")

    if(NOT wio_result EQUAL 0)
        message(FATAL_ERROR
            "CLI help smoke failed for '${command_label}' with code ${wio_result}.\n"
            "Tool output:\n${wio_output}"
        )
    endif()

    string(FIND "${wio_output}" "--help" help_index)
    string(FIND "${wio_output}" "Usage:" usage_index)
    if(help_index EQUAL -1 AND usage_index EQUAL -1)
        message(FATAL_ERROR
            "CLI help smoke for '${command_label}' did not print help text.\n"
            "Tool output:\n${wio_output}"
        )
    endif()
endforeach()

message(STATUS "CLI help smoke succeeded for ${WIO_EXE}")
