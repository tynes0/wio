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

add_custom_target(
    wio_tests_list
    COMMAND ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -N
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
        wio_test_realm_merge_across_modules_run
        "${CMAKE_SOURCE_DIR}/tests/modules/realm_merge_root.wio"
        "Merged realm sum: 3"
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
        wio_test_realm_qualified_types_run
        "${CMAKE_SOURCE_DIR}/tests/realm_qualified_types.wio"
        "Qualified cast hp: 7"
        --run
    )

    add_wio_output_test(
        wio_test_realm_qualified_from_run
        "${CMAKE_SOURCE_DIR}/tests/realm_qualified_from.wio"
        "Qualified from hp: 7"
        --run
    )

    add_wio_output_test(
        wio_test_interface_ref_upcast_run
        "${CMAKE_SOURCE_DIR}/tests/interface_ref_upcast.wio"
        "Interface ref hp: 7"
        --run
    )

    add_wio_output_test(
        wio_test_component_embedded_member_ctor_run
        "${CMAKE_SOURCE_DIR}/tests/component_embedded_member_ctor.wio"
        "Embedded component: 7/3"
        --run
    )

    add_wio_output_test(
        wio_test_inferred_object_and_fit_locals_run
        "${CMAKE_SOURCE_DIR}/tests/inferred_object_and_fit_locals.wio"
        "Inferred locals hp: 7"
        --run
    )

    add_wio_output_test(
        wio_test_native_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_bridge.wio"
        "Native multiply: 42"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_math.cpp"
    )

    add_wio_host_interop_test(
        wio_test_static_export_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/static_host.cpp"
        "Static export sum: 42"
    )

    add_wio_shared_host_interop_test(
        wio_test_shared_export_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/shared_host.cpp"
        "Shared export sum: 42"
    )

    add_wio_shared_host_interop_test(
        wio_test_module_lifecycle_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle_host.cpp"
        "Module lifecycle: table=1 caps=15 schema=0 exports=4 commands=2 events=2 v1 load=0 counter=48 add=51"
    )

    add_wio_shared_reload_interop_test(
        wio_test_module_reload_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_reload.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/module_reload_host.cpp"
        "Module reload: table=1 caps=63 schema=1 exports=4 commands=2 events=2 v1 load=0 restore=0 before=42 after=42 tick=48 add=51"
    )

    add_wio_shared_reload_interop_test(
        wio_test_hybrid_arena_demo
        "${CMAKE_SOURCE_DIR}/examples/hybrid_arena/arena_module.wio"
        "${CMAKE_SOURCE_DIR}/examples/hybrid_arena/hybrid_arena_host.cpp"
        "Hybrid arena summary: start=240/35/12/1 afterTickA=240/40/18/1 pulseA=1 afterHitA=207 restore=0 afterTickB=207/5/28/1 pulseB=2 final=78/0/38/2 threat=2380 alive=1 v1 load=0"
        --include-dir
        "${CMAKE_SOURCE_DIR}/examples/hybrid_arena"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/examples/hybrid_arena/hybrid_arena_native.cpp"
    )

    add_wio_output_test(
        wio_test_fit_numeric_clamp_run
        "${CMAKE_SOURCE_DIR}/tests/fit_numeric_clamp.wio"
        "Fit clamp: 0, 127"
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

    add_wio_failure_test(
        wio_invalid_native_function_with_body
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_function_with_body.wio"
        "@Native functions cannot define a Wio body"
    )

    add_wio_failure_test(
        wio_invalid_native_method_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_method_not_supported.wio"
        "@Native is currently supported only for top-level functions"
    )

    add_wio_failure_test(
        wio_invalid_export_string_return_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/export_string_return_not_supported.wio"
        "@Export currently supports only primitive or void return types"
    )

    add_wio_failure_test(
        wio_invalid_module_load_wrong_return
        "${CMAKE_SOURCE_DIR}/tests/invalid/module_load_wrong_return.wio"
        "@ModuleLoad must return i32"
    )

    add_wio_failure_test(
        wio_invalid_duplicate_module_update
        "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_module_update.wio"
        "Only one @ModuleUpdate function may be declared per compilation unit"
    )

    add_wio_failure_test(
        wio_invalid_module_restore_wrong_param
        "${CMAKE_SOURCE_DIR}/tests/invalid/module_restore_wrong_param.wio"
        "@ModuleRestoreState must declare exactly one i32 parameter"
    )

    add_wio_failure_test(
        wio_invalid_command_requires_export
        "${CMAKE_SOURCE_DIR}/tests/invalid/command_requires_export.wio"
        "@Command requires @Export on the same function"
    )

    add_wio_failure_test(
        wio_invalid_event_requires_void_return
        "${CMAKE_SOURCE_DIR}/tests/invalid/event_requires_void_return.wio"
        "@Event hooks must return void"
    )
endif()
