if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()

set(project_name "EmitCppSmoke")
set(project_root "${WIO_SCRATCH_DIR}/${project_name}")
file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")
file(MAKE_DIRECTORY "${WIO_SCRATCH_DIR}")

execute_process(
    COMMAND "${WIO_EXE}" project new "${project_name}" --output-dir "${WIO_SCRATCH_DIR}" --template wio-app
    RESULT_VARIABLE new_result
    OUTPUT_VARIABLE new_stdout
    ERROR_VARIABLE new_stderr
)
if(NOT new_result EQUAL 0)
    message(FATAL_ERROR "Could not create emit-C++ smoke project.\n${new_stdout}${new_stderr}")
endif()

execute_process(
    COMMAND "${WIO_EXE}" project build --project "${project_root}" --emit-cpp
    RESULT_VARIABLE emit_result
    OUTPUT_VARIABLE emit_stdout
    ERROR_VARIABLE emit_stderr
)
set(emit_output "${emit_stdout}${emit_stderr}")
if(NOT emit_result EQUAL 0)
    message(FATAL_ERROR "Project --emit-cpp failed with code ${emit_result}.\n${emit_output}")
endif()

file(GLOB generated_cpp "${project_root}/.wio-build/interop/*.wio.cpp")
list(LENGTH generated_cpp generated_count)
if(NOT generated_count EQUAL 1)
    message(FATAL_ERROR
        "Project --emit-cpp should retain exactly one generated C++ file; found ${generated_count}.\n${emit_output}"
    )
endif()

file(GLOB backend_outputs
    "${project_root}/.wio-build/interop/*.exe"
    "${project_root}/.wio-build/interop/*.a"
    "${project_root}/.wio-build/interop/*.lib"
    "${project_root}/.wio-build/interop/*.so"
    "${project_root}/.wio-build/interop/*.dylib"
)
list(LENGTH backend_outputs backend_count)
if(NOT backend_count EQUAL 0)
    message(FATAL_ERROR "Project --emit-cpp must stop before backend compilation.")
endif()
