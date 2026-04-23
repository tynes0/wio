if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SOURCE)
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

if(NOT DEFINED WIO_EXPECT)
    message(FATAL_ERROR "WIO_EXPECT was not provided.")
endif()

if(NOT DEFINED WIO_EMIT_DIR OR WIO_EMIT_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_EMIT_DIR was not provided.")
endif()

if(NOT DEFINED WIO_ARGS)
    set(WIO_ARGS "")
endif()

get_filename_component(wio_source_name "${WIO_SOURCE}" NAME)
file(MAKE_DIRECTORY "${WIO_EMIT_DIR}")

set(wio_temp_source "${WIO_EMIT_DIR}/${wio_source_name}")
configure_file("${WIO_SOURCE}" "${wio_temp_source}" COPYONLY)

execute_process(
    COMMAND "${WIO_EXE}" "${wio_temp_source}" --emit-cpp ${WIO_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")

if(NOT wio_result EQUAL 0)
    message(FATAL_ERROR
        "Expected emit-cpp to succeed for '${WIO_SOURCE}', but it failed with code ${wio_result}.\n"
        "Compiler output:\n${wio_output}"
    )
endif()

set(wio_cpp_path "${wio_temp_source}.cpp")

if(NOT EXISTS "${wio_cpp_path}")
    message(FATAL_ERROR
        "Expected generated C++ file was not created: ${wio_cpp_path}\n"
        "Compiler output:\n${wio_output}"
    )
endif()

file(READ "${wio_cpp_path}" wio_cpp_text)
string(REGEX MATCH "${WIO_EXPECT}" wio_match "${wio_cpp_text}")

if(NOT wio_match)
    message(FATAL_ERROR
        "Expected generated C++ file '${wio_cpp_path}' to match '${WIO_EXPECT}', but it did not.\n"
        "Compiler output:\n${wio_output}\n"
        "Generated C++ preview:\n${wio_cpp_text}"
    )
endif()

message(STATUS "Observed expected emit-cpp file output for: ${WIO_SOURCE}")
