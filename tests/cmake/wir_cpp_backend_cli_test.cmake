if(NOT DEFINED WIO_EXECUTABLE OR NOT DEFINED WIO_SOURCE OR NOT DEFINED WIO_OUTPUT_DIR)
    message(FATAL_ERROR "WIR C++ CLI test requires WIO_EXECUTABLE, WIO_SOURCE, and WIO_OUTPUT_DIR")
endif()

file(REMOVE_RECURSE "${WIO_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")
set(output "${WIO_OUTPUT_DIR}/wir-cpp-smoke${CMAKE_EXECUTABLE_SUFFIX}")

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${WIO_SOURCE}"
        --no-builtin
        --cpp-backend wir
        --output "${output}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr
)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR
        "WIR C++ backend compile failed (${compile_result})\n${compile_stdout}\n${compile_stderr}")
endif()
if(NOT EXISTS "${output}")
    message(FATAL_ERROR "WIR C++ backend did not create ${output}")
endif()

execute_process(COMMAND "${output}" RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 3)
    message(FATAL_ERROR "WIR C++ backend program returned ${run_result}; expected 3")
endif()
