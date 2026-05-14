if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_POWERSHELL)
    message(FATAL_ERROR "WIO_POWERSHELL was not provided.")
endif()

if(NOT DEFINED WIO_SCRIPT)
    message(FATAL_ERROR "WIO_SCRIPT was not provided.")
endif()

if(NOT DEFINED WIO_OUTPUT)
    message(FATAL_ERROR "WIO_OUTPUT was not provided.")
endif()

if(NOT DEFINED WIO_SCRIPT_ARGS)
    set(WIO_SCRIPT_ARGS)
endif()

if(NOT DEFINED WIO_COMPILER_ARGS)
    set(WIO_COMPILER_ARGS --dry-run)
endif()

file(REMOVE "${WIO_OUTPUT}")

execute_process(
    COMMAND "${WIO_POWERSHELL}" -ExecutionPolicy Bypass -File "${WIO_SCRIPT}" ${WIO_SCRIPT_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_script_result
    OUTPUT_VARIABLE wio_script_stdout
    ERROR_VARIABLE wio_script_stderr
)

set(wio_script_output "${wio_script_stdout}${wio_script_stderr}")

if(NOT wio_script_result EQUAL 0)
    message(FATAL_ERROR
        "Binding generation failed for '${WIO_SCRIPT}' with code ${wio_script_result}.\n"
        "Script output:\n${wio_script_output}"
    )
endif()

if(NOT EXISTS "${WIO_OUTPUT}")
    message(FATAL_ERROR "Binding generator did not create expected output '${WIO_OUTPUT}'.")
endif()

file(READ "${WIO_OUTPUT}" wio_generated_text)

foreach(expected_name IN ITEMS WIO_EXPECT_1 WIO_EXPECT_2 WIO_EXPECT_3 WIO_EXPECT_4)
    if(DEFINED ${expected_name} AND NOT "${${expected_name}}" STREQUAL "")
        string(FIND "${wio_generated_text}" "${${expected_name}}" found_index)
        if(found_index EQUAL -1)
            message(FATAL_ERROR
                "Expected generated binding '${WIO_OUTPUT}' to contain '${${expected_name}}', but it did not.\n"
                "Generated content:\n${wio_generated_text}"
            )
        endif()
    endif()
endforeach()

execute_process(
    COMMAND "${WIO_EXE}" "${WIO_OUTPUT}" ${WIO_COMPILER_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_compile_result
    OUTPUT_VARIABLE wio_compile_stdout
    ERROR_VARIABLE wio_compile_stderr
)

set(wio_compile_output "${wio_compile_stdout}${wio_compile_stderr}")

if(NOT wio_compile_result EQUAL 0)
    message(FATAL_ERROR
        "Generated binding '${WIO_OUTPUT}' failed to compile with code ${wio_compile_result}.\n"
        "Compiler output:\n${wio_compile_output}\n"
        "Generated content:\n${wio_generated_text}"
    )
endif()

message(STATUS "Binding smoke generation and compile succeeded for: ${WIO_SCRIPT}")
