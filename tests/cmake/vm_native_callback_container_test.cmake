include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")
execute_process(COMMAND "${WIO_CALLBACK_HOST_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Native container callback qualification failed: ${output}\n${error}")
endif()
message(STATUS "Array/dictionary/Any callbacks, nested object ownership and escaped code leases passed")
