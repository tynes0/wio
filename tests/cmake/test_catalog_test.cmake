foreach(required WIO_CTEST WIO_BUILD_DIR WIO_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()

set(config_args)
if(DEFINED WIO_CONFIG AND NOT WIO_CONFIG STREQUAL "")
    list(APPEND config_args -C "${WIO_CONFIG}")
endif()

foreach(label traceability freeze coherence)
    execute_process(
        COMMAND "${WIO_CTEST}" --test-dir "${WIO_BUILD_DIR}" ${config_args}
            --show-only=json-v1 -L "^${label}$"
        RESULT_VARIABLE result OUTPUT_VARIABLE registry ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not inspect ${label} suite: ${error}")
    endif()
    string(JSON count LENGTH "${registry}" tests)
    if(count EQUAL 0)
        message(FATAL_ERROR "Release suite ${label} is empty")
    endif()
endforeach()

foreach(mode ON OFF)
    set(fixture_build "${WIO_BUILD_DIR}/test-catalog-fixture-${mode}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${WIO_ROOT}/tests/cmake/catalog_fixture" -B "${fixture_build}"
            "-DWIO_ROOT=${WIO_ROOT}" "-DBUILD_TESTING=${mode}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Test-label fixture configure failed: ${output}\n${error}")
    endif()
    execute_process(
        COMMAND "${WIO_CTEST}" --test-dir "${fixture_build}" -C Debug --show-only=json-v1
        RESULT_VARIABLE result OUTPUT_VARIABLE registry ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Test-label fixture inspection failed: ${error}")
    endif()
    string(JSON count LENGTH "${registry}" tests)
    if((mode STREQUAL "ON" AND NOT count EQUAL 2) OR
       (mode STREQUAL "OFF" AND NOT count EQUAL 0))
        message(FATAL_ERROR "BUILD_TESTING=${mode} registered ${count} fixture tests")
    endif()
endforeach()
