foreach(required WIO_EXECUTABLE WIO_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "POD pointer rejection test requires ${required}")
    endif()
endforeach()

foreach(shape IN ITEMS scalar object generic)
    if(shape STREQUAL "scalar")
        set(expected "cannot be nullable")
    else()
        set(expected "not POD-native-compatible")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
        "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/invalid/native_pod_nullable_${shape}.wio" --dry-run
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if("${result}" STREQUAL "0" OR NOT "${output}${error}" MATCHES "${expected}")
        message(FATAL_ERROR "Unsupported nullable ${shape} field was not rejected during analysis:\n${output}\n${error}")
    endif()
endforeach()
message(STATUS "Unsupported nullable scalar/object/concrete-generic POD fields were rejected during analysis")
