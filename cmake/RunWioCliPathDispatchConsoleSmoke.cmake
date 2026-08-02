if(NOT DEFINED WIO_EXE OR WIO_EXE STREQUAL "")
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()
if(NOT DEFINED WIO_SOURCE OR WIO_SOURCE STREQUAL "")
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()
if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()

get_filename_component(wio_bin_dir "${WIO_EXE}" DIRECTORY)
get_filename_component(wio_file_name "${WIO_EXE}" NAME)
file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")
file(MAKE_DIRECTORY "${WIO_SCRATCH_DIR}/cwd" "${WIO_SCRATCH_DIR}/isolated-bin")

if(WIN32)
    execute_process(
        COMMAND cmd /d /c "set PATH=${wio_bin_dir};%PATH%&&wio"
        WORKING_DIRECTORY "${WIO_SCRATCH_DIR}/cwd"
        RESULT_VARIABLE bare_result
        OUTPUT_VARIABLE bare_stdout
        ERROR_VARIABLE bare_stderr
    )
else()
    execute_process(
        COMMAND sh -c "PATH=\"${wio_bin_dir}:$PATH\"; cd \"${WIO_SCRATCH_DIR}/cwd\"; exec wio"
        RESULT_VARIABLE bare_result
        OUTPUT_VARIABLE bare_stdout
        ERROR_VARIABLE bare_stderr
    )
endif()
set(bare_output "${bare_stdout}${bare_stderr}")
if(NOT bare_result EQUAL 0 OR NOT bare_output MATCHES "Wio command line interface")
    message(FATAL_ERROR "PATH-dispatched bare wio did not print CLI help.\n${bare_output}")
endif()
if(bare_output MATCHES "WIO LOG")
    message(FATAL_ERROR "Bare CLI help leaked compiler logger output.\n${bare_output}")
endif()

file(COPY "${WIO_EXE}" DESTINATION "${WIO_SCRATCH_DIR}/isolated-bin")
set(isolated_wio "${WIO_SCRATCH_DIR}/isolated-bin/${wio_file_name}")
execute_process(
    COMMAND "${isolated_wio}"
    WORKING_DIRECTORY "${WIO_SCRATCH_DIR}/cwd"
    RESULT_VARIABLE isolated_result
    OUTPUT_VARIABLE isolated_stdout
    ERROR_VARIABLE isolated_stderr
)
set(isolated_output "${isolated_stdout}${isolated_stderr}")
if(isolated_result EQUAL 0 OR NOT isolated_output MATCHES "CLI companion was not found")
    message(FATAL_ERROR "Missing companion did not produce the focused CLI diagnostic.\n${isolated_output}")
endif()
if(isolated_output MATCHES "WIO LOG|Required value FILE")
    message(FATAL_ERROR "Missing companion fell through to compiler argument parsing.\n${isolated_output}")
endif()

get_filename_component(source_file_name "${WIO_SOURCE}" NAME)
file(COPY "${WIO_SOURCE}" DESTINATION "${WIO_SCRATCH_DIR}/cwd")
set(probe_source "${WIO_SCRATCH_DIR}/cwd/${source_file_name}")
set(generated_cpp "${probe_source}.cpp")
execute_process(
    COMMAND "${WIO_EXE}" "${probe_source}" --emit-cpp
    WORKING_DIRECTORY "${WIO_SCRATCH_DIR}/cwd"
    RESULT_VARIABLE compiler_result
    OUTPUT_VARIABLE compiler_stdout
    ERROR_VARIABLE compiler_stderr
)
set(compiler_output "${compiler_stdout}${compiler_stderr}")
if(NOT compiler_result EQUAL 0 OR NOT EXISTS "${generated_cpp}")
    message(FATAL_ERROR "Compiler console-output probe failed.\n${compiler_output}")
endif()
if(NOT compiler_output MATCHES "Generated C\\+\\+ output:" OR compiler_output MATCHES "WIO LOG")
    message(FATAL_ERROR "Compiler output did not use the clean console presentation.\n${compiler_output}")
endif()

file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")
message(STATUS "PATH dispatch and clean console output smoke succeeded.")
