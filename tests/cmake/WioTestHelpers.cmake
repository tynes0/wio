include_guard(GLOBAL)

function(add_wio_success_test test_name source_file)
    add_test(
        NAME ${test_name}
        COMMAND $<TARGET_FILE:wio_app> "${source_file}" --dry-run ${ARGN}
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_output_test test_name source_file expected_output)
    set(wio_output_args ${ARGN})

    if (NOT wio_output_args)
        set(wio_output_args --dry-run)
    endif()

    list(JOIN wio_output_args ";" wio_output_args_serialized)

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_output}
            "-DWIO_ARGS=${wio_output_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectSuccess.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_emit_cpp_file_test test_name source_file expected_cpp_content)
    set(wio_emit_args ${ARGN})
    list(JOIN wio_emit_args ";" wio_emit_args_serialized)

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_cpp_content}
            -DWIO_EMIT_DIR=${CMAKE_BINARY_DIR}/emit-cpp/${test_name}
            "-DWIO_ARGS=${wio_emit_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioEmitCppExpectFile.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_failure_test test_name source_file expected_output)
    set(wio_failure_args --dry-run ${ARGN})
    list(JOIN wio_failure_args ";" wio_failure_args_serialized)

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_output}
            "-DWIO_ARGS=${wio_failure_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectFailure.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_runtime_failure_test test_name source_file expected_output)
    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_output}
            "-DWIO_ARGS=--run"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectFailure.cmake"
    )
    set_tests_properties(
        ${test_name}
        PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_failure_test_clean test_name source_file expected_output rejected_output)
    set(wio_failure_args --dry-run ${ARGN})
    list(JOIN wio_failure_args ";" wio_failure_args_serialized)

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_output}
            -DWIO_REJECT=${rejected_output}
            "-DWIO_ARGS=${wio_failure_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectFailure.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_non_dry_failure_test_clean test_name source_file expected_output rejected_output)
    set(wio_failure_args ${ARGN})
    list(JOIN wio_failure_args ";" wio_failure_args_serialized)

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_output}
            -DWIO_REJECT=${rejected_output}
            "-DWIO_ARGS=${wio_failure_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectFailure.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_backend_failure_test test_name source_file expected_output)
    set(wio_failure_args ${ARGN})
    list(JOIN wio_failure_args ";" wio_failure_args_serialized)

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_EXPECT=${expected_output}
            "-DWIO_ARGS=${wio_failure_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectFailure.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_host_interop_test test_name source_file host_source expected_output)
    set(wio_host_args ${ARGN})
    list(JOIN wio_host_args ";" wio_host_args_serialized)

    set(wio_host_output "${CMAKE_BINARY_DIR}/interop/${test_name}.a")

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_OUTPUT=${wio_host_output}
            -DWIO_HOST_SOURCE=${host_source}
            -DWIO_HOST_CXX=${WIO_BACKEND_CXX_COMPILER}
            -DWIO_RUNTIME_LIBRARY=${WIO_BACKEND_RUNTIME_LIBRARY}
            -DWIO_TARGET=static
            -DWIO_EXPECT=${expected_output}
            "-DWIO_ARGS=${wio_host_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioHostInterop.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_shared_host_interop_test test_name source_file host_source expected_output)
    set(wio_host_args ${ARGN})
    list(JOIN wio_host_args ";" wio_host_args_serialized)

    if (WIN32)
        set(wio_host_output "${CMAKE_BINARY_DIR}/interop/${test_name}.dll")
        set(wio_host_link_args_serialized "")
    elseif(APPLE)
        set(wio_host_output "${CMAKE_BINARY_DIR}/interop/${test_name}.dylib")
        set(wio_host_link_args_serialized "-ldl")
    else()
        set(wio_host_output "${CMAKE_BINARY_DIR}/interop/${test_name}.so")
        set(wio_host_link_args_serialized "-ldl")
    endif()

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_OUTPUT=${wio_host_output}
            -DWIO_HOST_SOURCE=${host_source}
            -DWIO_HOST_CXX=${WIO_BACKEND_CXX_COMPILER}
            -DWIO_TARGET=shared
            -DWIO_EXPECT=${expected_output}
            -DWIO_HOST_LINK_WITH_WIO_OUTPUT=OFF
            "-DWIO_HOST_RUNTIME_ARGS=${wio_host_output}"
            "-DWIO_HOST_LINK_ARGS=${wio_host_link_args_serialized}"
            "-DWIO_ARGS=${wio_host_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioHostInterop.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

function(add_wio_shared_reload_interop_test test_name source_file host_source expected_output)
    set(wio_host_args ${ARGN})
    list(JOIN wio_host_args ";" wio_host_args_serialized)

    if (WIN32)
        set(wio_host_output_a "${CMAKE_BINARY_DIR}/interop/${test_name}.a.dll")
        set(wio_host_output_b "${CMAKE_BINARY_DIR}/interop/${test_name}.b.dll")
    elseif(APPLE)
        set(wio_host_output_a "${CMAKE_BINARY_DIR}/interop/${test_name}.a.dylib")
        set(wio_host_output_b "${CMAKE_BINARY_DIR}/interop/${test_name}.b.dylib")
    else()
        set(wio_host_output_a "${CMAKE_BINARY_DIR}/interop/${test_name}.a.so")
        set(wio_host_output_b "${CMAKE_BINARY_DIR}/interop/${test_name}.b.so")
    endif()

    add_test(
        NAME ${test_name}
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${source_file}
            -DWIO_OUTPUT_A=${wio_host_output_a}
            -DWIO_OUTPUT_B=${wio_host_output_b}
            -DWIO_HOST_SOURCE=${host_source}
            -DWIO_HOST_CXX=${WIO_BACKEND_CXX_COMPILER}
            -DWIO_EXPECT=${expected_output}
            "-DWIO_ARGS=${wio_host_args_serialized}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioSharedReloadInterop.cmake"
    )

    set_tests_properties(
        ${test_name}
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()

# Record labels until all legacy properties have been applied. Appending here
# prevents a later LABELS property from accidentally erasing suite membership.
function(wio_test_labels test_name)
    set_property(GLOBAL APPEND PROPERTY "WIO_TEST_LABELS_${test_name}" ${ARGN})
endfunction()

function(wio_finalize_test_labels)
    get_property(registered DIRECTORY PROPERTY TESTS)
    foreach(test_name IN LISTS registered)
        get_property(pending GLOBAL PROPERTY "WIO_TEST_LABELS_${test_name}")
        if(pending)
            get_property(labels TEST "${test_name}" PROPERTY LABELS)
            list(APPEND labels ${pending})
            list(REMOVE_DUPLICATES labels)
            set_tests_properties("${test_name}" PROPERTIES LABELS "${labels}")
        endif()
    endforeach()
endfunction()
