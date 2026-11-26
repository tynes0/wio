include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")

execute_process(COMMAND "${WIO_DICTIONARY_CONTRACT_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Dictionary producer lifetime qualification failed: ${output}\n${error}")
endif()

# Structural changes and overlapping values now have dedicated focused gates.
foreach(case IN ITEMS failure)
    set(source "${WIO_ROOT}/tests/vm_native_dictionary_${case}.wio")
    set(bridge "${WIO_OUTPUT_DIR}/${case}${WIO_SHARED_SUFFIX}")
    set(bytecode "${WIO_OUTPUT_DIR}/${case}.wiob")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
        "${WIO_EXECUTABLE}" "${source}" --cpp-backend wir --include-dir "${WIO_ROOT}/tests/native"
        --target shared --output "${bridge}" --bytecode-sidecar "${bytecode}"
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Dictionary ${case} compilation failed: ${output}\n${error}")
    endif()
    execute_process(COMMAND "${WIO_EXECUTABLE}" --vm-run-bytecode "${bytecode}" --native-library "${bridge}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    set(expected "WVM1222.*dictionary failure oracle")
    if("${result}" STREQUAL "0" OR NOT "${output}\n${error}" MATCHES "${expected}")
        message(FATAL_ERROR "Dictionary ${case} negative gate failed: ${result}\n${output}\n${error}")
    endif()
endforeach()
