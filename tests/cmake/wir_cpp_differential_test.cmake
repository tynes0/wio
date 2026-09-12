foreach(required WIO_EXECUTABLE WIO_ROOT WIO_SOURCE WIO_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "WIR differential test requires ${required}.")
    endif()
endforeach()

if(NOT DEFINED WIO_EXECUTABLE_SUFFIX)
    set(WIO_EXECUTABLE_SUFFIX "")
endif()

file(REMOVE_RECURSE "${WIO_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")

set(common_arguments "${WIO_SOURCE}")
if(WIO_NO_BUILTIN)
    list(APPEND common_arguments --no-builtin)
endif()

function(build_backend backend output_path elapsed_output)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E time
            "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
            "${WIO_EXECUTABLE}" ${common_arguments}
            --cpp-backend "${backend}"
            --output "${output_path}"
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_stdout
        ERROR_VARIABLE build_stderr
    )
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR
            "${backend} backend failed with ${build_result}.\n${build_stdout}\n${build_stderr}")
    endif()
    if(NOT EXISTS "${output_path}")
        message(FATAL_ERROR "${backend} backend did not create ${output_path}.")
    endif()

    string(REGEX MATCH "Elapsed time: [0-9.]+ s" elapsed "${build_stdout}\n${build_stderr}")
    if(elapsed STREQUAL "")
        set(elapsed "Elapsed time: unavailable")
    endif()
    set(${elapsed_output} "${elapsed}" PARENT_SCOPE)
endfunction()

function(run_backend backend output_path result_output stdout_output stderr_output)
    execute_process(
        COMMAND "${output_path}"
        WORKING_DIRECTORY "${WIO_OUTPUT_DIR}"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_stdout
        ERROR_VARIABLE run_stderr
    )
    set(${result_output} "${run_result}" PARENT_SCOPE)
    set(${stdout_output} "${run_stdout}" PARENT_SCOPE)
    set(${stderr_output} "${run_stderr}" PARENT_SCOPE)
endfunction()

set(legacy_output "${WIO_OUTPUT_DIR}/legacy${WIO_EXECUTABLE_SUFFIX}")
set(wir_output "${WIO_OUTPUT_DIR}/wir${WIO_EXECUTABLE_SUFFIX}")
build_backend(legacy "${legacy_output}" legacy_elapsed)
build_backend(wir "${wir_output}" wir_elapsed)

run_backend(legacy "${legacy_output}" legacy_result legacy_stdout legacy_stderr)
run_backend(wir "${wir_output}" wir_result wir_stdout wir_stderr)

if(NOT legacy_result STREQUAL wir_result)
    message(FATAL_ERROR "Backend exit mismatch: legacy=${legacy_result}, wir=${wir_result}.")
endif()
if(NOT legacy_stdout STREQUAL wir_stdout)
    message(FATAL_ERROR "Backend stdout mismatch.\nlegacy:\n${legacy_stdout}\nwir:\n${wir_stdout}")
endif()
if(NOT legacy_stderr STREQUAL wir_stderr)
    message(FATAL_ERROR "Backend stderr mismatch.\nlegacy:\n${legacy_stderr}\nwir:\n${wir_stderr}")
endif()

message(STATUS
    "WIR differential parity succeeded: exit=${wir_result}; legacy ${legacy_elapsed}; wir ${wir_elapsed}")
