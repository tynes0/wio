foreach(required WIO_EXECUTABLE WIO_ROOT WIO_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "WIR project differential test requires ${required}.")
    endif()
endforeach()

file(REMOVE_RECURSE "${WIO_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")

function(run_checked label)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" ${ARGN}
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_stdout
        ERROR_VARIABLE command_stderr
    )
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with ${command_result}.\n${command_stdout}\n${command_stderr}")
    endif()
endfunction()

function(build_project project_root backend build_dir elapsed_output)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E time
            "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
            "${WIO_EXECUTABLE}" project build
            --project "${project_root}"
            --build-dir "${build_dir}"
            --cpp-backend "${backend}"
            --rebuild
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_stdout
        ERROR_VARIABLE build_stderr
    )
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR
            "${backend} project build failed with ${build_result}.\n${build_stdout}\n${build_stderr}")
    endif()
    string(REGEX MATCH "Elapsed time: [0-9.]+ s" elapsed "${build_stdout}\n${build_stderr}")
    if(elapsed STREQUAL "")
        set(elapsed "Elapsed time: unavailable")
    endif()
    set(${elapsed_output} "${elapsed}" PARENT_SCOPE)
endfunction()

function(run_project project_root backend build_dir result_output stdout_output stderr_output)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
            "${WIO_EXECUTABLE}" project run
            --project "${project_root}"
            --build-dir "${build_dir}"
            --cpp-backend "${backend}"
            --no-build
            -- "parity" "two words"
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_stdout
        ERROR_VARIABLE run_stderr
    )
    set(${result_output} "${run_result}" PARENT_SCOPE)
    set(${stdout_output} "${run_stdout}" PARENT_SCOPE)
    set(${stderr_output} "${run_stderr}" PARENT_SCOPE)
endfunction()

function(check_project template_name project_name)
    run_checked(
        "create ${template_name} project"
        "${WIO_EXECUTABLE}" project new "${project_name}"
        --output-dir "${WIO_OUTPUT_DIR}"
        --template "${template_name}"
    )
    set(project_root "${WIO_OUTPUT_DIR}/${project_name}")
    set(legacy_build "${WIO_OUTPUT_DIR}/${project_name}-legacy")
    set(wir_build "${WIO_OUTPUT_DIR}/${project_name}-wir")

    build_project("${project_root}" legacy "${legacy_build}" legacy_elapsed)
    build_project("${project_root}" wir "${wir_build}" wir_elapsed)
    run_project("${project_root}" legacy "${legacy_build}" legacy_result legacy_stdout legacy_stderr)
    run_project("${project_root}" wir "${wir_build}" wir_result wir_stdout wir_stderr)

    if(NOT legacy_result STREQUAL wir_result)
        message(FATAL_ERROR
            "${template_name} exit mismatch: legacy=${legacy_result}, wir=${wir_result}.")
    endif()
    if(NOT legacy_stdout STREQUAL wir_stdout)
        message(FATAL_ERROR
            "${template_name} stdout mismatch.\nlegacy:\n${legacy_stdout}\nwir:\n${wir_stdout}")
    endif()
    if(NOT legacy_stderr STREQUAL wir_stderr)
        message(FATAL_ERROR
            "${template_name} stderr mismatch.\nlegacy:\n${legacy_stderr}\nwir:\n${wir_stderr}")
    endif()
    message(STATUS
        "${template_name} parity: exit=${wir_result}; legacy ${legacy_elapsed}; wir ${wir_elapsed}")
endfunction()

check_project(wio-app WirProjectParity)
check_project(wio-native-app WirNativeProjectParity)
