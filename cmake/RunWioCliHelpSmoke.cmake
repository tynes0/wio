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
    "migrate|--help"
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

set(bare_command_groups file project bind env perf migrate)
foreach(command_group IN LISTS bare_command_groups)
    execute_process(
        COMMAND "${WIO_EXE}" "${command_group}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE group_result
        OUTPUT_VARIABLE group_stdout
        ERROR_VARIABLE group_stderr
    )
    set(group_output "${group_stdout}${group_stderr}")
    if(NOT group_result EQUAL 0 OR NOT group_output MATCHES "Usage:")
        message(FATAL_ERROR
            "Bare command group '${command_group}' did not show successful help.\n"
            "Tool output:\n${group_output}"
        )
    endif()
endforeach()

set(command_help_specs
    "project|run|--help"
    "project|build|--help"
    "project|test|--help"
    "project|package|--help"
    "file|run|--help"
    "bind|new|--help"
    "env|status|--help"
    "perf|smoke|--help"
    "migrate|attributes|--help"
)

foreach(command_spec IN LISTS command_help_specs)
    string(REPLACE "|" ";" command_parts "${command_spec}")
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
            "CLI command help failed for '${command_spec}' with code ${wio_result}.\n"
            "Tool output:\n${wio_output}"
        )
    endif()
    string(FIND "${wio_output}" "Usage:" usage_index)
    if(usage_index EQUAL -1)
        message(FATAL_ERROR
            "CLI command help did not contain a usage line for '${command_spec}'.\n"
            "Tool output:\n${wio_output}"
        )
    endif()

    if(command_spec STREQUAL "file|run|--help")
        string(FIND "${wio_output}" "Expected a file subcommand" unexpected_error_index)
        if(NOT unexpected_error_index EQUAL -1)
            message(FATAL_ERROR
                "File command help contained an error diagnostic.\n"
                "Tool output:\n${wio_output}"
            )
        endif()
    endif()
endforeach()

set(routed_help_specs
    "help|project|run"
    "help|file|run"
    "help|bind|import"
    "help|env|doctor"
    "help|package"
    "help|perf|smoke"
)

foreach(command_spec IN LISTS routed_help_specs)
    string(REPLACE "|" ";" command_parts "${command_spec}")
    execute_process(
        COMMAND "${WIO_EXE}" ${command_parts}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE routed_help_result
        OUTPUT_VARIABLE routed_help_stdout
        ERROR_VARIABLE routed_help_stderr
    )
    set(routed_help_output "${routed_help_stdout}${routed_help_stderr}")
    if(NOT routed_help_result EQUAL 0 OR NOT routed_help_output MATCHES "Usage:")
        message(FATAL_ERROR
            "Routed help failed for '${command_spec}'.\n"
            "Tool output:\n${routed_help_output}"
        )
    endif()
endforeach()

execute_process(
    COMMAND "${WIO_EXE}" build --version
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_stdout
    ERROR_VARIABLE version_stderr
)
set(version_output "${version_stdout}${version_stderr}")
if(NOT version_result EQUAL 0 OR NOT version_output MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR
        "Subcommand version handling failed.\n"
        "Tool output:\n${version_output}"
    )
endif()

set(subcommand_typo_specs
    "project|buld|wio project build"
    "file|chek|wio file check"
    "bind|improt|wio bind import"
    "env|doctr|wio env doctor"
    "perf|smok|wio perf smoke"
)

foreach(command_spec IN LISTS subcommand_typo_specs)
    string(REPLACE "|" ";" typo_parts "${command_spec}")
    list(GET typo_parts 0 typo_group)
    list(GET typo_parts 1 typo_command)
    list(GET typo_parts 2 typo_expected)
    execute_process(
        COMMAND "${WIO_EXE}" "${typo_group}" "${typo_command}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE typo_result
        OUTPUT_VARIABLE typo_stdout
        ERROR_VARIABLE typo_stderr
    )
    set(typo_output "${typo_stdout}${typo_stderr}")
    string(FIND "${typo_output}" "${typo_expected}" typo_suggestion_index)
    if(typo_result EQUAL 0 OR typo_suggestion_index EQUAL -1)
        message(FATAL_ERROR
            "Subcommand suggestion failed for '${typo_group} ${typo_command}'.\n"
            "Tool output:\n${typo_output}"
        )
    endif()
endforeach()

set(version_groups file project bind env package perf)
foreach(version_group IN LISTS version_groups)
    execute_process(
        COMMAND "${WIO_EXE}" "${version_group}" --version
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE group_version_result
        OUTPUT_VARIABLE group_version_stdout
        ERROR_VARIABLE group_version_stderr
    )
    set(group_version_output "${group_version_stdout}${group_version_stderr}")
    if(NOT group_version_result EQUAL 0 OR
       NOT group_version_output MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
        message(FATAL_ERROR
            "Version handling failed for command group '${version_group}'.\n"
            "Tool output:\n${group_version_output}"
        )
    endif()
endforeach()

execute_process(
    COMMAND "${WIO_EXE}" bild
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE unknown_result
    OUTPUT_VARIABLE unknown_stdout
    ERROR_VARIABLE unknown_stderr
)
set(unknown_output "${unknown_stdout}${unknown_stderr}")
if(unknown_result EQUAL 0 OR NOT unknown_output MATCHES "Did you mean 'wio build'")
    message(FATAL_ERROR
        "Unknown-command suggestion handling failed.\n"
        "Tool output:\n${unknown_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" build -- unexpected
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE delimiter_result
    OUTPUT_VARIABLE delimiter_stdout
    ERROR_VARIABLE delimiter_stderr
)
set(delimiter_output "${delimiter_stdout}${delimiter_stderr}")
if(delimiter_result EQUAL 0 OR NOT delimiter_output MATCHES "Too many positional arguments")
    message(FATAL_ERROR
        "Parser delimiter bounds handling failed.\n"
        "Tool output:\n${delimiter_output}"
    )
endif()

message(STATUS "CLI help smoke succeeded for ${WIO_EXE}")
