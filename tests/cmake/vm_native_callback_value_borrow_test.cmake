include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")
execute_process(COMMAND "${WIO_CALLBACK_HOST_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 90)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Callback value borrow qualification failed: ${output}\n${error}")
endif()
message(STATUS "Callback value borrow ownership, identity and staged copy-back passed")
