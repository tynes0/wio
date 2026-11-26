foreach(required WIO_EXECUTABLE WIO_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "POD template rejection test requires ${required}")
    endif()
endforeach()

foreach(shape IN ITEMS variadic_field pack_storage const_identity const_specialized_field)
    if(shape STREQUAL "const_identity")
        set(expected "does not declare")
    else()
        set(expected "not POD-native-compatible")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
        "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/invalid/native_pod_${shape}.wio" --dry-run
        --include-dir "${WIO_ROOT}/tests/native"
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if("${result}" STREQUAL "0" OR NOT "${output}${error}" MATCHES "${expected}")
        message(FATAL_ERROR "Unsupported native POD ${shape} was not rejected during analysis:\n${output}\n${error}")
    endif()
endforeach()
message(STATUS "Native non-POD variadic/specialized fields, pack storage and non-whitelisted extents were rejected during analysis")
