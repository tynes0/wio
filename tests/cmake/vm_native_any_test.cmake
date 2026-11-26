include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")

execute_process(COMMAND "${WIO_ANY_CONTRACT_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Any producer lifetime qualification failed: ${output}\n${error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/vm_native_any_unsupported.wio" --cpp-backend wir
    --include-dir "${WIO_ROOT}/tests/native" --target shared
    --output "${WIO_OUTPUT_DIR}/unsupported${WIO_SHARED_SUFFIX}"
    --bytecode-sidecar "${WIO_OUTPUT_DIR}/unsupported.wiob"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Unsupported any fixture compilation failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${WIO_EXECUTABLE}" --vm-run-bytecode "${WIO_OUTPUT_DIR}/unsupported.wiob"
    --native-library "${WIO_OUTPUT_DIR}/unsupported${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if("${result}" STREQUAL "0" OR NOT "${output}${error}" MATCHES "WVM1224.*Native any payload is unsupported")
    message(FATAL_ERROR "Unknown any payload was not rejected deterministically: ${output}\n${error}")
endif()
