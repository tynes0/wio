foreach(required WIO_EXECUTABLE WIO_ROOT WIO_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "WIR behavioral differential test requires ${required}.")
    endif()
endforeach()

if(NOT DEFINED WIO_EXECUTABLE_SUFFIX)
    set(WIO_EXECUTABLE_SUFFIX "")
endif()

set(behavioral_sources
    attribute_behavioral_pipeline_run
    attribute_typed_receiver_pre_run
    attribute_behavioral_ordering_run
    attribute_around_pipeline_run
    attribute_around_skip_run
    attribute_around_result_run
    attribute_receiver_guard_run
    attribute_behavioral_reflection_run
    attribute_behavioral_async_result_run
    invalid/attribute_around_proceed_twice
    invalid/attribute_around_proceed_escaped
    invalid/attribute_around_result_proceed_escaped
)

file(REMOVE_RECURSE "${WIO_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")

foreach(source_name IN LISTS behavioral_sources)
    string(REPLACE "/" "_" output_name "${source_name}")
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DWIO_EXECUTABLE=${WIO_EXECUTABLE}"
            "-DWIO_EXECUTABLE_SUFFIX=${WIO_EXECUTABLE_SUFFIX}"
            "-DWIO_ROOT=${WIO_ROOT}"
            "-DWIO_SOURCE=${WIO_ROOT}/tests/${source_name}.wio"
            "-DWIO_OUTPUT_DIR=${WIO_OUTPUT_DIR}/${output_name}"
            -P "${WIO_ROOT}/tests/cmake/wir_cpp_differential_test.cmake"
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE differential_result
        OUTPUT_VARIABLE differential_stdout
        ERROR_VARIABLE differential_stderr
    )

    if(NOT differential_result EQUAL 0)
        message(FATAL_ERROR
            "Behavioral differential failed for ${source_name}.\n"
            "${differential_stdout}\n${differential_stderr}")
    endif()

    message(STATUS "Behavioral legacy/WIR parity passed: ${source_name}")
endforeach()
