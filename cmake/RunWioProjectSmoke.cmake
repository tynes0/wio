if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()

set(project_name "SmokeApp")
set(project_root "${WIO_SCRATCH_DIR}/${project_name}")

file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")
file(MAKE_DIRECTORY "${WIO_SCRATCH_DIR}")

execute_process(
    COMMAND "${WIO_EXE}" project new "${project_name}" --output-dir "${WIO_SCRATCH_DIR}" --template wio-app
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE new_result
    OUTPUT_VARIABLE new_stdout
    ERROR_VARIABLE new_stderr
)
set(new_output "${new_stdout}${new_stderr}")
if(NOT new_result EQUAL 0)
    message(FATAL_ERROR
        "Project smoke failed during 'project new' with code ${new_result}.\n"
        "Tool output:\n${new_output}"
    )
endif()

if(NOT EXISTS "${project_root}/wio.makewio")
    message(FATAL_ERROR "Expected generated project manifest at ${project_root}/wio.makewio")
endif()

file(READ "${project_root}/wio.makewio" project_manifest)
string(REPLACE "args = []" "args = [\"manifest-default\"]" project_manifest "${project_manifest}")
file(WRITE "${project_root}/wio.makewio" "${project_manifest}")

set(nested_working_directory "${project_root}/nested/work")
file(MAKE_DIRECTORY "${nested_working_directory}")

execute_process(
    COMMAND "${WIO_EXE}" project describe
    WORKING_DIRECTORY "${nested_working_directory}"
    RESULT_VARIABLE describe_result
    OUTPUT_VARIABLE describe_stdout
    ERROR_VARIABLE describe_stderr
)
set(describe_output "${describe_stdout}${describe_stderr}")
if(NOT describe_result EQUAL 0)
    message(FATAL_ERROR
        "Project smoke failed during 'project describe' with code ${describe_result}.\n"
        "Tool output:\n${describe_output}"
    )
endif()

string(FIND "${describe_output}" "SmokeApp" describe_name_index)
if(describe_name_index EQUAL -1)
    message(FATAL_ERROR
        "Project describe output did not mention the generated project name.\n"
        "Tool output:\n${describe_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project build
    WORKING_DIRECTORY "${nested_working_directory}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
set(build_output "${build_stdout}${build_stderr}")
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Project smoke failed during 'project build' with code ${build_result}.\n"
        "Tool output:\n${build_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project run -- "two words" "--literal"
    WORKING_DIRECTORY "${nested_working_directory}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)
set(run_output "${run_stdout}${run_stderr}")
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "Project smoke failed during 'project run' with code ${run_result}.\n"
        "Tool output:\n${run_output}"
    )
endif()

string(FIND "${run_output}" "Hello from a plain Wio application." hello_index)
if(hello_index EQUAL -1)
    message(FATAL_ERROR
        "Project run output did not contain the expected hello text.\n"
        "Tool output:\n${run_output}"
    )
endif()

string(FIND "${run_output}" "Application arguments: manifest-default|two words|--literal" args_index)
if(args_index EQUAL -1)
    message(FATAL_ERROR
        "Project run did not preserve manifest and console arguments.\n"
        "Tool output:\n${run_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" run --no-build --no-manifest-args --cwd nested --print-command --arg "option value" -- "console value" "--flag-like-value"
    WORKING_DIRECTORY "${nested_working_directory}"
    RESULT_VARIABLE shorthand_result
    OUTPUT_VARIABLE shorthand_stdout
    ERROR_VARIABLE shorthand_stderr
)
set(shorthand_output "${shorthand_stdout}${shorthand_stderr}")
if(NOT shorthand_result EQUAL 0)
    message(FATAL_ERROR
        "Project smoke failed during shorthand 'wio run' with code ${shorthand_result}.\n"
        "Tool output:\n${shorthand_output}"
    )
endif()

string(FIND "${shorthand_output}" "Working directory:" working_directory_index)
string(FIND "${shorthand_output}" "Command:" command_index)
if(working_directory_index EQUAL -1 OR command_index EQUAL -1)
    message(FATAL_ERROR
        "Shorthand project run did not print its resolved invocation.\n"
        "Tool output:\n${shorthand_output}"
    )
endif()

string(FIND "${shorthand_output}" "Application arguments: option value|console value|--flag-like-value" shorthand_args_index)
if(shorthand_args_index EQUAL -1)
    message(FATAL_ERROR
        "Shorthand project run did not preserve option and console arguments.\n"
        "Tool output:\n${shorthand_output}"
    )
endif()

file(READ "${project_root}/wio/module.wio" project_source)
string(REPLACE "return 0;" "return 23;" project_source "${project_source}")
file(WRITE "${project_root}/wio/module.wio" "${project_source}")

execute_process(
    COMMAND "${WIO_EXE}" project run --rebuild --no-manifest-args
    WORKING_DIRECTORY "${nested_working_directory}"
    RESULT_VARIABLE exit_code_result
    OUTPUT_VARIABLE exit_code_stdout
    ERROR_VARIABLE exit_code_stderr
)
set(exit_code_output "${exit_code_stdout}${exit_code_stderr}")
if(NOT exit_code_result EQUAL 23)
    message(FATAL_ERROR
        "Project run should preserve application exit code 23, but returned ${exit_code_result}.\n"
        "Tool output:\n${exit_code_output}"
    )
endif()

message(STATUS "Project smoke succeeded for ${project_root}")
