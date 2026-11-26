include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")

execute_process(COMMAND "${WIO_GENERIC_ANY_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Generic Any selection/lifetime qualification failed: ${output}\n${error}")
endif()
message(STATUS "Generic native POD Any/container selection and lifetime qualification passed")
