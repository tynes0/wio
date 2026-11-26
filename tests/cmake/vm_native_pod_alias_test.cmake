if(NOT DEFINED WIO_ALIAS_CONTRACT_EXECUTABLE)
    message(FATAL_ERROR "POD alias qualification requires the contract executable")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/vm_native_pod_layout_test.cmake")

execute_process(
    COMMAND "${WIO_ALIAS_CONTRACT_EXECUTABLE}" "${bytecode}" "${bridge}"
    RESULT_VARIABLE legacy_result
    OUTPUT_VARIABLE legacy_output
    ERROR_VARIABLE legacy_error
    TIMEOUT 30
)
if(NOT "${legacy_result}" STREQUAL "0")
    message(FATAL_ERROR "POD alias legacy-producer rejection failed (${legacy_result}):\n${legacy_output}\n${legacy_error}")
endif()
message(STATUS "POD alias legacy producer rejected without executing overlapping thunks")
