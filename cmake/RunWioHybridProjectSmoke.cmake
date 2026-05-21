if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()

set(project_name "SmokeHybrid")
set(project_root "${WIO_SCRATCH_DIR}/${project_name}")

file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")
file(MAKE_DIRECTORY "${WIO_SCRATCH_DIR}")

execute_process(
    COMMAND "${WIO_EXE}" project new "${project_name}" --output-dir "${WIO_SCRATCH_DIR}" --template hybrid-module
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE new_result
    OUTPUT_VARIABLE new_stdout
    ERROR_VARIABLE new_stderr
)
set(new_output "${new_stdout}${new_stderr}")
if(NOT new_result EQUAL 0)
    message(FATAL_ERROR
        "Hybrid project smoke failed during 'project new' with code ${new_result}.\n"
        "Tool output:\n${new_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project describe --project "${project_root}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE describe_result
    OUTPUT_VARIABLE describe_stdout
    ERROR_VARIABLE describe_stderr
)
set(describe_output "${describe_stdout}${describe_stderr}")
if(NOT describe_result EQUAL 0)
    message(FATAL_ERROR
        "Hybrid project smoke failed during 'project describe' with code ${describe_result}.\n"
        "Tool output:\n${describe_output}"
    )
endif()

string(FIND "${describe_output}" "\"enabled\": true" host_enabled_index)
if(host_enabled_index EQUAL -1)
    message(FATAL_ERROR
        "Hybrid project describe output did not report host.enabled = true.\n"
        "Tool output:\n${describe_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project build --project "${project_root}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
set(build_output "${build_stdout}${build_stderr}")
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Hybrid project smoke failed during 'project build' with code ${build_result}.\n"
        "Tool output:\n${build_output}"
    )
endif()

execute_process(
    COMMAND "${WIO_EXE}" project run --project "${project_root}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)
set(run_output "${run_stdout}${run_stderr}")
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "Hybrid project smoke failed during 'project run' with code ${run_result}.\n"
        "Tool output:\n${run_output}"
    )
endif()

string(FIND "${run_output}" "Hybrid project: before=11 afterAdd=16 afterTick=18" hybrid_output_index)
if(hybrid_output_index EQUAL -1)
    message(FATAL_ERROR
        "Hybrid project run output did not contain the expected host-side summary.\n"
        "Tool output:\n${run_output}"
    )
endif()

message(STATUS "Hybrid project smoke succeeded for ${project_root}")
