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

function(add_wio_dev_target target_name source_file)
    add_custom_target(
        ${target_name}
        COMMAND $<TARGET_FILE:wio_app> "${source_file}" ${ARGN}
        DEPENDS wio_app
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
    )
endfunction()

function(make_wio_target_suffix input_name output_var)
    string(MAKE_C_IDENTIFIER "${input_name}" sanitized_name)
    string(TOLOWER "${sanitized_name}" sanitized_name)
    set(${output_var} "${sanitized_name}" PARENT_SCOPE)
endfunction()

set(wio_ctest_config_args)
if (CMAKE_CONFIGURATION_TYPES)
    set(wio_ctest_config_args -C $<CONFIG>)
endif()

add_custom_target(
    wio_tests
    COMMAND ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS wio_app
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

set(WIO_PLAYGROUND_DIR "${CMAKE_SOURCE_DIR}/playground")
set(WIO_PLAYGROUND_MAIN "${WIO_PLAYGROUND_DIR}/main.wio")

if (EXISTS "${WIO_PLAYGROUND_MAIN}")
    add_wio_dev_target(
        wio_playground_check
        "${WIO_PLAYGROUND_MAIN}"
        --dry-run
    )

    add_wio_dev_target(
        wio_playground_run
        "${WIO_PLAYGROUND_MAIN}"
        --run
    )

    add_wio_dev_target(
        wio_playground_tokens
        "${WIO_PLAYGROUND_MAIN}"
        --show-tokens
        --dry-run
    )

    add_wio_dev_target(
        wio_playground_ast
        "${WIO_PLAYGROUND_MAIN}"
        --show-ast
        --dry-run
    )
endif()

file(GLOB WIO_PLAYGROUND_FILES CONFIGURE_DEPENDS "${WIO_PLAYGROUND_DIR}/*.wio")

foreach (wio_playground_file IN LISTS WIO_PLAYGROUND_FILES)
    get_filename_component(wio_playground_name "${wio_playground_file}" NAME_WE)
    make_wio_target_suffix("${wio_playground_name}" wio_playground_suffix)

    add_wio_dev_target(
        "wio_file_${wio_playground_suffix}_check"
        "${wio_playground_file}"
        --dry-run
    )

    add_wio_dev_target(
        "wio_file_${wio_playground_suffix}_run"
        "${wio_playground_file}"
        --run
    )
endforeach()

if (BUILD_TESTING AND EXISTS "${CMAKE_SOURCE_DIR}/tests")
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

    add_wio_success_test(
        wio_test_user_module_merge_dry_run
        "${CMAKE_SOURCE_DIR}/tests/modules/single_file_root.wio"
    )

    add_wio_output_test(
        wio_test_user_module_alias_run
        "${CMAKE_SOURCE_DIR}/tests/modules/user_alias_root.wio"
        "Value: 42"
        --run
    )

    add_wio_output_test(
        wio_test_realm_basic_run
        "${CMAKE_SOURCE_DIR}/tests/realm_basic.wio"
        "Realm sum: 3"
        --run
    )

    add_wio_output_test(
        wio_test_realm_object_isolation_run
        "${CMAKE_SOURCE_DIR}/tests/realm_object_isolation.wio"
        "Realm counters: 3"
        --run
    )

    add_wio_output_test(
        wio_test_realm_nested_run
        "${CMAKE_SOURCE_DIR}/tests/realm_nested.wio"
        "Nested realm sum: 7"
        --run
    )

    add_wio_output_test(
        wio_show_tokens_alias_stream
        "${CMAKE_SOURCE_DIR}/tests/test_use_alias.wio"
        "kwAs 'as'"
        --show-tokens
        --dry-run
    )

    add_wio_output_test(
        wio_show_tokens_realm_stream
        "${CMAKE_SOURCE_DIR}/tests/realm_basic.wio"
        "kwRealm 'realm'"
        --show-tokens
        --dry-run
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
        wio_invalid_single_file_ignores_user_imports
        "${CMAKE_SOURCE_DIR}/tests/modules/single_file_root.wio"
        "Undefined symbol: 'GetValue'"
        --single-file
    )

    add_wio_failure_test(
        wio_invalid_no_builtin_ignores_stdlib_imports
        "${CMAKE_SOURCE_DIR}/tests/test_use_alias.wio"
        "Undefined symbol: 'console'"
        --no-builtin
    )
endif()
