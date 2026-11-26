include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")

execute_process(COMMAND "${WIO_NESTED_ANY_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Nested Any host qualification failed: ${output}\n${error}")
endif()
message(STATUS "Nested Any identity, depth, failure and producer lifetime qualification passed")
