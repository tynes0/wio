include("${WIO_ROOT}/tests/cmake/vm_native_pod_layout_test.cmake")

execute_process(COMMAND "${WIO_OBJECT_PROXY_EXECUTABLE}"
    "${WIO_OUTPUT_DIR}/program.wiob" "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}" nullable-any
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Nullable Any host qualification failed: ${output}\n${error}")
endif()
message(STATUS "Nullable object/interface/opaque Any host qualification passed")

foreach(shape IN ITEMS reference callback)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
        "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/invalid/any_nullable_${shape}_storage.wio" --dry-run
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if("${result}" STREQUAL "0" OR NOT "${output}${error}" MATCHES "Type mismatch for 'erased'")
        message(FATAL_ERROR "Nullable ${shape} Any storage escaped its lifetime gate: ${output}\n${error}")
    endif()
endforeach()
