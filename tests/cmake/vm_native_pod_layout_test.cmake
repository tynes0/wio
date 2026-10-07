foreach(required WIO_EXECUTABLE WIO_ROOT WIO_OUTPUT_DIR WIO_SHARED_SUFFIX)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "POD layout differential test requires ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")
if(NOT DEFINED WIO_SOURCE)
    set(WIO_SOURCE "${WIO_ROOT}/tests/vm_native_pod_layout.wio")
endif()
if(NOT DEFINED WIO_EXPECTED_OUTPUT)
    set(WIO_EXPECTED_OUTPUT "native-pod-layout-ok:")
endif()
set(source "${WIO_SOURCE}")
set(native "${WIO_OUTPUT_DIR}/native${WIO_EXECUTABLE_SUFFIX}")
set(bridge "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}")
set(bytecode "${WIO_OUTPUT_DIR}/program.wiob")
foreach(mode IN ITEMS native bridge)
    if(mode STREQUAL "native")
        set(options --target exe --output "${native}")
    else()
        set(options --target shared --output "${bridge}" --bytecode-sidecar "${bytecode}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
        "${WIO_EXECUTABLE}" "${source}" --cpp-backend wir
        --include-dir "${WIO_ROOT}/tests/native" ${options}
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "${mode} POD compilation failed (${result}):\n${output}\n${error}")
    endif()
endforeach()
execute_process(COMMAND "${native}" RESULT_VARIABLE native_result
    OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error TIMEOUT 30)
execute_process(COMMAND "${WIO_EXECUTABLE}" --vm-run-bytecode "${bytecode}" --native-library "${bridge}"
    RESULT_VARIABLE vm_result OUTPUT_VARIABLE vm_output ERROR_VARIABLE vm_error TIMEOUT 30)
if(NOT "${native_result}" STREQUAL "0" OR NOT "${vm_result}" STREQUAL "0" OR
   NOT native_output MATCHES "${WIO_EXPECTED_OUTPUT}" OR
   NOT native_output STREQUAL vm_output OR NOT native_error STREQUAL vm_error)
    message(FATAL_ERROR "POD layout differential failed: C++=${native_result}, VM=${vm_result}\nC++: ${native_output}\n${native_error}\nVM: ${vm_output}\n${vm_error}")
endif()
message(STATUS "POD layout differential passed: ${native_output}")
