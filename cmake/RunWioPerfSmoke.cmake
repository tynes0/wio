if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()

file(REMOVE_RECURSE "${WIO_SCRATCH_DIR}")

execute_process(
    COMMAND "${WIO_EXE}" perf smoke --iterations 1 --scratch-dir "${WIO_SCRATCH_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE perf_result
    OUTPUT_VARIABLE perf_stdout
    ERROR_VARIABLE perf_stderr
)

set(perf_output "${perf_stdout}${perf_stderr}")

if(NOT perf_result EQUAL 0)
    message(FATAL_ERROR
        "Perf smoke failed with code ${perf_result}.\n"
        "Tool output:\n${perf_output}"
    )
endif()

foreach(expected_fragment
        "Wio performance smoke"
        "Scenario: file-check"
        "Scenario: file-run"
        "Scenario: project-build-cold"
        "Scenario: project-build-warm"
        "Scenario: project-run-warm")
    string(FIND "${perf_output}" "${expected_fragment}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR
            "Perf smoke output did not contain '${expected_fragment}'.\n"
            "Tool output:\n${perf_output}"
        )
    endif()
endforeach()

if(EXISTS "${WIO_SCRATCH_DIR}")
    message(FATAL_ERROR
        "Perf smoke should clean its scratch directory on success.\n"
        "Scratch directory still exists: ${WIO_SCRATCH_DIR}\n"
        "Tool output:\n${perf_output}"
    )
endif()

message(STATUS "Perf smoke succeeded for ${WIO_EXE}")
