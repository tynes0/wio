file(GLOB_RECURSE WIO_TEST_FILES "${CMAKE_SOURCE_DIR}/tests/*.wio")

source_group("tests" FILES ${WIO_TEST_FILES})

add_executable(wio_app
    main.cpp
    ${WIO_TEST_FILES}
)

target_link_libraries(wio_app
    PRIVATE
        wio_compiler
)

target_include_directories(wio_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/include
)

if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
    add_custom_command(TARGET wio_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tests"
            "$<TARGET_FILE_DIR:wio_app>/tests"
        COMMENT "Test files are being copied...."
    )
else()
    message(WARNING "WARNING: The 'tests' folder was not found; the copy operation was skipped.")
endif()

if (BUILD_TESTING AND EXISTS "${CMAKE_SOURCE_DIR}/tests")
    function(add_wio_success_test test_name source_file)
        add_test(
            NAME ${test_name}
            COMMAND $<TARGET_FILE:wio_app> "${source_file}" --dry-run
        )

        set_tests_properties(
            ${test_name}
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endfunction()

    function(add_wio_failure_test test_name source_file expected_output)
        add_test(
            NAME ${test_name}
            COMMAND
                ${CMAKE_COMMAND}
                -DWIO_EXE=$<TARGET_FILE:wio_app>
                -DWIO_SOURCE=${source_file}
                -DWIO_EXPECT=${expected_output}
                -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectFailure.cmake"
        )

        set_tests_properties(
            ${test_name}
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endfunction()

    add_wio_success_test(
        wio_test1_dry_run
        "${CMAKE_SOURCE_DIR}/tests/test1.wio"
    )

    add_wio_success_test(
        wio_test2_dry_run
        "${CMAKE_SOURCE_DIR}/tests/test2.wio"
    )

    add_wio_success_test(
        wio_test_use_alias_dry_run
        "${CMAKE_SOURCE_DIR}/tests/test_use_alias.wio"
    )

    add_wio_failure_test(
        wio_invalid_interface_method_body
        "${CMAKE_SOURCE_DIR}/tests/invalid/interface_method_body.wio"
        "Interface methods cannot have a body"
    )

    add_wio_failure_test(
        wio_invalid_non_void_when_without_else
        "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_when_without_else.wio"
        "Functions with a return value must provide an 'else' fallback for 'when' guards"
    )

    add_wio_failure_test(
        wio_invalid_component_inheritance
        "${CMAKE_SOURCE_DIR}/tests/invalid/component_inheritance.wio"
        "Components must be POD \\(Plain Old Data\\) and cannot inherit from objects or interfaces"
    )

    add_wio_failure_test(
        wio_invalid_multiple_object_inheritance
        "${CMAKE_SOURCE_DIR}/tests/invalid/multiple_object_inheritance.wio"
        "cannot inherit from multiple objects/components"
    )

    add_wio_failure_test(
        wio_invalid_readonly_external_mutation
        "${CMAKE_SOURCE_DIR}/tests/invalid/readonly_external_mutation.wio"
        "Cannot modify @Readonly member 'hp' from outside its object"
    )

    add_wio_failure_test(
        wio_invalid_user_module_alias_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/user_module_alias_not_supported.wio"
        "Import aliasing with 'as' is currently supported only for standard library modules"
    )
endif()
