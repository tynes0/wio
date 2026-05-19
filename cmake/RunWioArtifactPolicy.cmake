if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_MODE OR WIO_MODE STREQUAL "")
    message(FATAL_ERROR "WIO_MODE was not provided.")
endif()

if(NOT DEFINED WIO_WORK_DIR OR WIO_WORK_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_WORK_DIR was not provided.")
endif()

if(NOT DEFINED WIO_ARGS)
    set(WIO_ARGS "")
endif()

if(NOT DEFINED WIO_EXPECT)
    set(WIO_EXPECT "")
endif()

if(NOT DEFINED WIO_EXPECT_OUTPUT)
    set(WIO_EXPECT_OUTPUT "")
endif()

file(MAKE_DIRECTORY "${WIO_WORK_DIR}")

get_filename_component(wio_source_name "${WIO_SOURCE}" NAME)
set(wio_temp_source "${WIO_WORK_DIR}/${wio_source_name}")
configure_file("${WIO_SOURCE}" "${wio_temp_source}" COPYONLY)

set(wio_generated_cpp "${wio_temp_source}.cpp")
get_filename_component(wio_source_stem "${wio_temp_source}" NAME_WE)

if(WIN32)
    set(wio_adjacent_output "${WIO_WORK_DIR}/${wio_source_stem}.exe")
else()
    set(wio_adjacent_output "${WIO_WORK_DIR}/${wio_source_stem}")
endif()

if(NOT WIO_EXPECT_OUTPUT STREQUAL "")
    file(REMOVE "${WIO_EXPECT_OUTPUT}")
endif()

file(REMOVE "${wio_generated_cpp}" "${wio_adjacent_output}")

if(WIO_ARGS STREQUAL "")
    set(wio_args_list)
else()
    set(wio_args_list ${WIO_ARGS})
endif()

if(WIO_MODE STREQUAL "compiler")
    execute_process(
        COMMAND "${WIO_EXE}" "${wio_temp_source}" ${wio_args_list}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE wio_result
        OUTPUT_VARIABLE wio_stdout
        ERROR_VARIABLE wio_stderr
    )
elseif(WIO_MODE STREQUAL "file-run")
    execute_process(
        COMMAND "${WIO_EXE}" file run "${wio_temp_source}" ${wio_args_list}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE wio_result
        OUTPUT_VARIABLE wio_stdout
        ERROR_VARIABLE wio_stderr
    )
else()
    message(FATAL_ERROR "Unsupported WIO_MODE '${WIO_MODE}'.")
endif()

set(wio_output "${wio_stdout}${wio_stderr}")

if(NOT wio_result EQUAL 0)
    message(FATAL_ERROR
        "Artifact policy smoke failed for '${WIO_SOURCE}' in mode '${WIO_MODE}' with code ${wio_result}.\n"
        "Tool output:\n${wio_output}"
    )
endif()

if(EXISTS "${wio_generated_cpp}")
    message(FATAL_ERROR
        "Generated C++ intermediate was expected to be cleaned up, but it remained on disk:\n"
        "  ${wio_generated_cpp}\n"
        "Tool output:\n${wio_output}"
    )
endif()

if(EXISTS "${wio_adjacent_output}")
    message(FATAL_ERROR
        "Source-adjacent backend output was expected to stay out of the source directory, but it remained on disk:\n"
        "  ${wio_adjacent_output}\n"
        "Tool output:\n${wio_output}"
    )
endif()

if(NOT WIO_EXPECT_OUTPUT STREQUAL "" AND NOT EXISTS "${WIO_EXPECT_OUTPUT}")
    message(FATAL_ERROR
        "Expected backend output '${WIO_EXPECT_OUTPUT}' was not created.\n"
        "Tool output:\n${wio_output}"
    )
endif()

if(NOT WIO_EXPECT STREQUAL "")
    string(REGEX MATCH "${WIO_EXPECT}" wio_match "${wio_output}")
    if(NOT wio_match)
        message(FATAL_ERROR
            "Expected tool output to match '${WIO_EXPECT}', but it did not.\n"
            "Tool output:\n${wio_output}"
        )
    endif()
endif()

message(STATUS "Artifact policy smoke succeeded for: ${WIO_SOURCE} (${WIO_MODE})")
