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
        wio_test_for_in_dynamic_array_run
        "${CMAKE_SOURCE_DIR}/tests/for_in_dynamic_array.wio"
        "For in dynamic sum: 7"
        --run
    )

    add_wio_output_test(
        wio_test_for_in_static_array_run
        "${CMAKE_SOURCE_DIR}/tests/for_in_static_array.wio"
        "For in static sum: 9"
        --run
    )

    add_wio_output_test(
        wio_test_foreach_dynamic_array_run
        "${CMAKE_SOURCE_DIR}/tests/foreach_dynamic_array.wio"
        "Foreach sum: 12"
        --run
    )

    add_wio_output_test(
        wio_test_for_in_inclusive_range_run
        "${CMAKE_SOURCE_DIR}/tests/for_in_inclusive_range.wio"
        "Inclusive range sum: 10"
        --run
    )

    add_wio_output_test(
        wio_test_for_in_exclusive_range_run
        "${CMAKE_SOURCE_DIR}/tests/for_in_exclusive_range.wio"
        "Exclusive range sum: 6"
        --run
    )

    add_wio_output_test(
        wio_test_for_in_range_step_positive_run
        "${CMAKE_SOURCE_DIR}/tests/for_in_range_step_positive.wio"
        "Positive step sum: 16"
        --run
    )

    add_wio_output_test(
        wio_test_for_in_range_step_negative_run
        "${CMAKE_SOURCE_DIR}/tests/for_in_range_step_negative.wio"
        "Negative step sum: 16"
        --run
    )

    add_wio_output_test(
        wio_test_for_parenthesized_array_step_run
        "${CMAKE_SOURCE_DIR}/tests/for_parenthesized_array_step.wio"
        "Parenthesized array step score: 28"
        --run
    )

    add_wio_output_test(
        wio_test_for_array_index_value_run
        "${CMAKE_SOURCE_DIR}/tests/for_array_index_value.wio"
        "Array index/value score: 51"
        --run
    )

    add_wio_output_test(
        wio_test_for_ref_view_array_run
        "${CMAKE_SOURCE_DIR}/tests/for_ref_view_array.wio"
        "Ref/view array: 11, 12, 13 => 3"
        --run
    )

    add_wio_output_test(
        wio_test_for_dict_key_value_run
        "${CMAKE_SOURCE_DIR}/tests/for_dict_key_value.wio"
        "Dict loop score: 100"
        --run
    )

    add_wio_output_test(
        wio_test_for_dict_view_ref_binding_run
        "${CMAKE_SOURCE_DIR}/tests/for_dict_view_ref_binding.wio"
        "Dict ref/value score: 50"
        --run
    )

    add_wio_output_test(
        wio_test_for_component_binding_run
        "${CMAKE_SOURCE_DIR}/tests/for_component_binding.wio"
        "Component binding total: 16"
        --run
    )

    add_wio_output_test(
        wio_test_for_component_ref_binding_arithmetic_run
        "${CMAKE_SOURCE_DIR}/tests/for_component_ref_binding_arithmetic.wio"
        "Component ref binding total: 16, seen: 16"
        --run
    )

    add_wio_output_test(
        wio_test_for_component_index_binding_run
        "${CMAKE_SOURCE_DIR}/tests/for_component_index_binding.wio"
        "Component index binding total: 11"
        --run
    )

    add_wio_output_test(
        wio_test_for_component_index_binding_step_run
        "${CMAKE_SOURCE_DIR}/tests/for_component_index_binding_step.wio"
        "Component step binding total: 16"
        --run
    )

    add_wio_output_test(
        wio_test_c_style_for_loop_run
        "${CMAKE_SOURCE_DIR}/tests/c_style_for_loop.wio"
        "C for sum: 10"
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

    add_wio_output_test(
        wio_test_native_generic_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_bridge.wio"
        "Generic native double: 42, 42"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
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
        wio_test_generic_export_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/generic_exported_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/generic_export_host.cpp"
        "Generic export instantiate: table=1 exports=2 i32=41 bool=true"
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
        wio_test_generic_function_identity_run
        "${CMAKE_SOURCE_DIR}/tests/generic_function_identity.wio"
        "Generic values: 42, wio"
        --run
    )

    add_wio_output_test(
        wio_test_generic_function_ref_assign_run
        "${CMAKE_SOURCE_DIR}/tests/generic_function_ref_assign.wio"
        "Generic assign: 25"
        --run
    )

    add_wio_output_test(
        wio_test_generic_function_multi_explicit_run
        "${CMAKE_SOURCE_DIR}/tests/generic_function_multi_explicit.wio"
        "Generic multi: 7|deduced, 8|explicit, wio"
        --run
    )

    add_wio_output_test(
        wio_test_generic_overload_prefers_concrete_run
        "${CMAKE_SOURCE_DIR}/tests/generic_overload_prefers_concrete.wio"
        "Generic overloads: int, generic"
        --run
    )

    add_wio_output_test(
        wio_test_generic_overload_prefers_specialized_generic_run
        "${CMAKE_SOURCE_DIR}/tests/generic_overload_prefers_specialized_generic.wio"
        "Generic specialized overloads: same, mixed"
        --run
    )

    add_wio_output_test(
        wio_test_generic_explicit_call_prefers_generic_run
        "${CMAKE_SOURCE_DIR}/tests/generic_explicit_call_prefers_generic.wio"
        "Generic explicit overload: generic"
        --run
    )

    add_wio_output_test(
        wio_test_generic_realm_explicit_call_run
        "${CMAKE_SOURCE_DIR}/tests/generic_realm_explicit_call.wio"
        "Realm generic: 5/hp"
        --run
    )

    add_wio_output_test(
        wio_test_generic_type_alias_identity_run
        "${CMAKE_SOURCE_DIR}/tests/generic_type_alias_identity.wio"
        "Generic alias: 42, alias, 10"
        --run
    )

    add_wio_output_test(
        wio_test_generic_type_alias_nested_run
        "${CMAKE_SOURCE_DIR}/tests/generic_type_alias_nested.wio"
        "Generic alias nested: 12, 36"
        --run
    )

    add_wio_output_test(
        wio_test_generic_component_pair_run
        "${CMAKE_SOURCE_DIR}/tests/generic_component_pair.wio"
        "Generic component: 7/wio"
        --run
    )

    add_wio_output_test(
        wio_test_generic_component_constructor_deduction_run
        "${CMAKE_SOURCE_DIR}/tests/generic_component_constructor_deduction.wio"
        "Generic component ctor deduction: 7/9, wio/true"
        --run
    )

    add_wio_output_test(
        wio_test_generic_object_box_run
        "${CMAKE_SOURCE_DIR}/tests/generic_object_box.wio"
        "Generic object: 7, 7"
        --run
    )

    add_wio_output_test(
        wio_test_generic_object_inheritance_run
        "${CMAKE_SOURCE_DIR}/tests/generic_object_inheritance.wio"
        "Generic object inheritance: 11 / 11"
        --run
    )

    add_wio_output_test(
        wio_test_generic_object_constructor_deduction_run
        "${CMAKE_SOURCE_DIR}/tests/generic_object_constructor_deduction.wio"
        "Generic ctor deduction: 7, wio"
        --run
    )

    add_wio_output_test(
        wio_test_generic_object_method_basic_run
        "${CMAKE_SOURCE_DIR}/tests/generic_object_method_basic.wio"
        "Generic method inner: true\nGeneric methods: wio, 42, 7"
        --run
    )

    add_wio_output_test(
        wio_test_cpp_reserved_identifier_codegen_run
        "${CMAKE_SOURCE_DIR}/tests/cpp_reserved_identifier_codegen.wio"
        "Reserved identifiers: 7/11"
        --run
    )

    add_wio_output_test(
        wio_test_generic_interface_inheritance_run
        "${CMAKE_SOURCE_DIR}/tests/generic_interface_inheritance.wio"
        "Generic interface inheritance: 9 / 9"
        --run
    )

    add_wio_success_test(
        wio_test_generic_interface_declaration_dry_run
        "${CMAKE_SOURCE_DIR}/tests/generic_interface_declaration.wio"
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
        wio_invalid_for_in_requires_array
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_in_requires_array.wio"
        "For-in loops currently require an array or dictionary type"
    )

    add_wio_failure_test(
        wio_invalid_for_dict_requires_two_bindings
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_dict_requires_two_bindings.wio"
        "Dictionary iteration requires exactly 2 bindings"
    )

    add_wio_failure_test(
        wio_invalid_for_ref_dict_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_ref_dict_not_supported.wio"
        "Dictionary keys are immutable"
    )

    add_wio_failure_test(
        wio_invalid_for_component_binding_count_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_component_binding_count_mismatch.wio"
        "Component binding expects 2 names, or 3 with an index binding, but 4 were provided"
    )

    add_wio_failure_test(
        wio_invalid_for_array_index_ref_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_array_index_ref_not_supported.wio"
        "Array index bindings do not support 'ref' or 'view'"
    )

    add_wio_failure_test(
        wio_invalid_for_dict_step_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_dict_step_not_supported.wio"
        "Step clauses are currently supported only for range iteration"
    )

    add_wio_failure_test(
        wio_invalid_for_array_step_requires_integer
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_array_step_requires_integer.wio"
        "Array step expressions must be integer values"
    )

    add_wio_failure_test(
        wio_invalid_for_range_ref_binding_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_range_ref_binding_not_supported.wio"
        "Range iteration does not support 'ref' or 'view' bindings"
    )

    add_wio_failure_test(
        wio_invalid_for_range_step_zero
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_range_step_zero.wio"
        "Range step cannot be zero"
    )

    add_wio_failure_test(
        wio_invalid_for_range_requires_integer_bounds
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_range_requires_integer_bounds.wio"
        "Range iteration currently requires integer bounds"
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

    add_wio_failure_test(
        wio_invalid_generic_function_argument_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_function_argument_mismatch.wio"
        "No matching function/constructor overload found"
    )

    add_wio_failure_test(
        wio_invalid_generic_interface_method_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_interface_method_not_supported.wio"
        "Generic methods are currently supported only on object methods"
    )

    add_wio_failure_test(
        wio_invalid_generic_explicit_argument_count_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_explicit_argument_count_mismatch.wio"
        "No matching function/constructor overload found"
    )

    add_wio_failure_test(
        wio_invalid_generic_overload_ambiguous
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_overload_ambiguous.wio"
        "Ambiguous function call"
    )

    add_wio_failure_test(
        wio_invalid_generic_native_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_native_not_supported.wio"
        "Generic @Native functions require at least one @Instantiate\\(\\.\\.\\.\\) declaration\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_export_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_not_supported.wio"
        "Generic @Export functions require at least one @Instantiate\\(\\.\\.\\.\\) declaration\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_instantiate_argument_count_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_argument_count_mismatch.wio"
        "@Instantiate expects exactly 2 type arguments for 'PickLeft'\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_instantiate_non_concrete
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_non_concrete.wio"
        "@Instantiate must use fully concrete type arguments\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_instantiate_requires_native_or_export
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_requires_native_or_export.wio"
        "@Instantiate is currently supported only together with @Native or @Export\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_export_instantiate_non_abi_safe
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_instantiate_non_abi_safe.wio"
        "@Export instantiated with 'string' produces a non-C-ABI-safe parameter 0 of type 'string'\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_type_alias_missing_arguments
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_type_alias_missing_arguments.wio"
        "Type alias 'Buffer' expects 1 generic arguments, but got 0"
    )

    add_wio_failure_test(
        wio_invalid_generic_type_alias_argument_count_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_type_alias_argument_count_mismatch.wio"
        "Type alias 'Ordered' expects 2 generic arguments, but got 1"
    )

    add_wio_failure_test(
        wio_invalid_type_alias_generic_args_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/type_alias_generic_args_not_supported.wio"
        "Type alias 'Score' does not accept generic arguments"
    )

    add_wio_failure_test(
        wio_invalid_type_alias_attributes_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/type_alias_attributes_not_supported.wio"
        "Attributes are not currently supported on type aliases"
    )

    add_wio_failure_test(
        wio_invalid_generic_object_constructor_missing_arguments
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_object_constructor_missing_arguments.wio"
        "Cannot infer generic arguments for constructor 'Box'. Use explicit type arguments."
    )

    add_wio_failure_test(
        wio_invalid_generic_component_constructor_missing_arguments
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_component_constructor_missing_arguments.wio"
        "Cannot infer generic arguments for constructor 'Pair'. Use explicit type arguments."
    )

    add_wio_failure_test(
        wio_invalid_generic_from_missing_arguments
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_from_missing_arguments.wio"
        "Generic type 'Reader' expects 1 generic arguments, but got 0"
    )
endif()
