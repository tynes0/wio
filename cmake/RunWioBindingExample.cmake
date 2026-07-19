if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED EXAMPLE_ROOT OR EXAMPLE_ROOT STREQUAL "")
    message(FATAL_ERROR "EXAMPLE_ROOT was not provided.")
endif()

if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()

file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")
file(MAKE_DIRECTORY "${WIO_SCRATCH_DIR}")

set(header_input "${WIO_SCRATCH_DIR}/binding_import_example.h")
set(manifest_input "${WIO_SCRATCH_DIR}/binding_manifest.json")
set(header_output "${WIO_SCRATCH_DIR}/binding_import_example.wio")
set(manifest_output "${WIO_SCRATCH_DIR}/binding_manifest_example.wio")

configure_file("${EXAMPLE_ROOT}/binding_import_example.h" "${header_input}" COPYONLY)
configure_file("${EXAMPLE_ROOT}/binding_manifest.json" "${manifest_input}" COPYONLY)

file(REMOVE "${header_output}" "${manifest_output}")

execute_process(
    COMMAND "${WIO_EXE}" bind import --header "${header_input}" --realm binding_import_example --output "${header_output}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_stdout
    ERROR_VARIABLE import_stderr
)
set(import_output "${import_stdout}${import_stderr}")
if(NOT import_result EQUAL 0)
    message(FATAL_ERROR
        "Binding example import failed with code ${import_result}.\n"
        "Tool output:\n${import_output}"
    )
endif()

if(NOT EXISTS "${header_output}")
    message(FATAL_ERROR "Expected generated binding file at ${header_output}")
endif()

execute_process(
    COMMAND "${WIO_EXE}" bind new --manifest "${manifest_input}" --output "${manifest_output}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE manifest_result
    OUTPUT_VARIABLE manifest_stdout
    ERROR_VARIABLE manifest_stderr
)
set(manifest_output_text "${manifest_stdout}${manifest_stderr}")
if(NOT manifest_result EQUAL 0)
    message(FATAL_ERROR
        "Binding example manifest generation failed with code ${manifest_result}.\n"
        "Tool output:\n${manifest_output_text}"
    )
endif()

if(NOT EXISTS "${manifest_output}")
    message(FATAL_ERROR "Expected manifest-generated binding file at ${manifest_output}")
endif()

execute_process(
    COMMAND "${WIO_EXE}" file check "${header_output}" --include-dir "${WIO_SCRATCH_DIR}" --target static
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_stdout
    ERROR_VARIABLE check_stderr
)
set(check_output "${check_stdout}${check_stderr}")
if(NOT check_result EQUAL 0)
    message(FATAL_ERROR
        "Generated binding validation failed with code ${check_result}.\n"
        "Tool output:\n${check_output}"
    )
endif()

message(STATUS "Binding example smoke succeeded in ${WIO_SCRATCH_DIR}")
