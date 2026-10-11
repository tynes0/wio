foreach(required WIO_EXECUTABLE WIO_ROOT WIO_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Bridge project test requires ${required}")
    endif()
endforeach()
set(project "${WIO_ROOT}/tests/projects/vm_native_pruning")
foreach(mode IN ITEMS build run)
    set(options)
    if(mode STREQUAL "build")
        list(APPEND options --rebuild)
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
        "${WIO_EXECUTABLE}" project "${mode}" --backend vm --build-dir "${WIO_OUTPUT_DIR}" ${options}
        WORKING_DIRECTORY "${project}" RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 240)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "VM project ${mode} failed (${result}): ${output}\n${error}")
    endif()
    if(mode STREQUAL "run" AND
       (NOT output MATCHES "up to date" OR NOT output MATCHES "native-bridge-project-ok"))
        message(FATAL_ERROR "Incremental VM project run failed: ${output}\n${error}")
    endif()
endforeach()
message(STATUS "Pruned VM project build and incremental run passed")
