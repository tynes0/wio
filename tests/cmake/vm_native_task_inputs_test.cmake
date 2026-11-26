include("${WIO_ROOT}/tests/cmake/vm_native_bridge_pruning_test.cmake")
foreach(mode IN ITEMS full pruned)
    execute_process(COMMAND "${WIO_TASK_INPUT_HOST_EXECUTABLE}"
        "${WIO_OUTPUT_DIR}/${mode}/program.wiob" "${WIO_OUTPUT_DIR}/${mode}/bridge${WIO_SHARED_SUFFIX}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "native-task-input-host-ok")
        message(FATAL_ERROR "${mode} native task ownership/shutdown failed (${result}): ${output}\n${error}")
    endif()
endforeach()
message(STATUS "Native task inputs preserve identity, cancellation, Machine move and generation isolation")
