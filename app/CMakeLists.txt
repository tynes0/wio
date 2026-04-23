file(GLOB_RECURSE WIO_TEST_FILES "${CMAKE_SOURCE_DIR}/tests/*.wio")
file(GLOB_RECURSE WIO_STD_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/std/*.wio")

source_group("tests" FILES ${WIO_TEST_FILES})
source_group("std" FILES ${WIO_STD_FILES})

add_executable(wio_app
    main.cpp
    ${WIO_STD_FILES}
    ${WIO_TEST_FILES}
)

set_target_properties(wio_app PROPERTIES
    OUTPUT_NAME "wio"
)

target_link_libraries(wio_app
    PRIVATE
        wio_compiler
)

target_include_directories(wio_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/include
)

install(TARGETS wio_app
    RUNTIME DESTINATION bin
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
        wio_test_import_path_module_dir_run
        "${CMAKE_SOURCE_DIR}/tests/import_path_root.wio"
        "Import path threat: 45"
        --run
        --module-dir
        "${CMAKE_SOURCE_DIR}/tests/imports"
    )

    add_wio_output_test(
        wio_test_realm_merge_across_modules_run
        "${CMAKE_SOURCE_DIR}/tests/modules/realm_merge_root.wio"
        "Merged realm sum: 3"
        --run
    )

    add_wio_output_test(
        wio_test_relative_module_imports_run
        "${CMAKE_SOURCE_DIR}/tests/modules/relative/root.wio"
        "Relative import sum: 50"
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
        wio_test_object_component_access_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/object_component_access_semantics.wio"
        "Object/component access semantics: 17"
        --run
    )

    add_wio_output_test(
        wio_test_trusted_access_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/trusted_access_semantics.wio"
        "Trusted access sum: 49"
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
        wio_test_match_value_basic_run
        "${CMAKE_SOURCE_DIR}/tests/match_value_basic.wio"
        "Match value: zero / small / mid / large"
        --run
    )

    add_wio_output_test(
        wio_test_match_statement_basic_run
        "${CMAKE_SOURCE_DIR}/tests/match_statement_basic.wio"
        "Match statement: 1 / 12 / 100"
        --run
    )

    add_wio_output_test(
        wio_test_when_guard_fallback_value_run
        "${CMAKE_SOURCE_DIR}/tests/when_guard_fallback_value.wio"
        "When guard: 7 / 0"
        --run
    )

    add_wio_output_test(
        wio_test_ref_view_fit_is_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/ref_view_fit_is_semantics.wio"
        "Ref/view/fit/is: 6/6/6"
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

    add_wio_output_test(
        wio_test_generic_native_forward_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/generic_native_forward_bridge.wio"
        "Generic native forward: 42, 42"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_native_generic_pair_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_pair_bridge.wio"
        "Generic native pair: 42"
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

    add_wio_host_interop_test(
        wio_test_sdk_static_api_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_static_api_host.cpp"
        "SDK static: 42"
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
        "Generic export instantiate: table=1 exports=3 i32=41 f32=.* bool=false"
    )

    add_wio_shared_host_interop_test(
        wio_test_module_lifecycle_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle_host.cpp"
        "Module lifecycle: table=1 caps=15 schema=0 exports=4 commands=2 events=2 v1 load=0 counter=48 add=51"
    )

    add_wio_shared_host_interop_test(
        wio_test_sdk_module_easy_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_module_easy_host.cpp"
        "SDK easy: before=42 afterAdd=45 afterTick=51 exports=4 commands=2 events=2"
    )

    add_wio_shared_host_interop_test(
        wio_test_sdk_exported_types_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_types_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_types_host.cpp"
        "SDK exported types: exports=14 types=2 objectFields=3 objectMethods=2 start=7 afterAdd=25 mixed=41 componentFields=2 hp=100 hpAfter=135 maxHp=150"
    )

    add_wio_shared_host_interop_test(
        wio_test_sdk_exported_ctor_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_ctor_types_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_ctor_host.cpp"
        "SDK ctor exported types: exports=11 types=2 enemy=15 afterDamage=11 point=4,9 ctorCount=1/1"
    )

    add_wio_shared_host_interop_test(
        wio_test_sdk_exported_inherited_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_inherited_types_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_inherited_host.cpp"
        "SDK inherited exported types: exports=13 types=1 fields=3 methods=5 hp=40 id=2 phase=3 score0=42 score1=47 enrage=5 heal=49 score2=59"
    )

    add_wio_shared_host_interop_test(
        wio_test_sdk_exported_complex_fields_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_complex_fields_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_complex_fields_host.cpp"
        "SDK complex fields: fields=8 title=boss tags=2 hp=30 callback=21 profile=77 position=11 fixedSum=24"
    )

    add_wio_shared_reload_interop_test(
        wio_test_module_reload_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_reload.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/module_reload_host.cpp"
        "Module reload: table=1 caps=63 schema=1 exports=4 commands=2 events=2 v1 load=0 restore=0 before=42 after=42 tick=48 add=51"
    )

    add_wio_shared_reload_interop_test(
        wio_test_sdk_module_hot_reload_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_reload.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_module_hot_reload_host.cpp"
        "SDK reload: before=40 afterAdd=43 afterReload=43 afterTick=49 generation=2"
    )

    add_wio_shared_reload_interop_test(
        wio_test_hybrid_arena_demo
        "${CMAKE_SOURCE_DIR}/examples/hybrid_arena/arena_module.wio"
        "${CMAKE_SOURCE_DIR}/examples/hybrid_arena/hybrid_arena_host.cpp"
        "Hybrid arena summary:[ \t\r\n]*start=240/35/12/1[ \t\r\n]*afterTickA=240/40/18/1[ \t\r\n]*pulseA=1[ \t\r\n]*afterHitA=207[ \t\r\n]*restore=0[ \t\r\n]*afterTickB=207/5/28/1[ \t\r\n]*pulseB=2[ \t\r\n]*final=78/0/38/2[ \t\r\n]*threat=2380[ \t\r\n]*alive=1[ \t\r\n]*v1[ \t\r\n]*load=0"
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
        wio_test_numeric_literal_suffixes_run
        "${CMAKE_SOURCE_DIR}/tests/numeric_literal_suffixes_run.wio"
        "Numeric literal suffixes: 16/3/true/true"
        --run
    )

    add_wio_output_test(
        wio_test_numeric_literal_context_run
        "${CMAKE_SOURCE_DIR}/tests/numeric_literal_context_run.wio"
        "Numeric literal context: 0/2/3/5/20"
        --run
    )

    add_wio_output_test(
        wio_test_std_console_direct_run
        "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
        "Std direct: hello\nStd direct value: 7"
        --run
    )

    add_wio_output_test(
        wio_test_cpp_header_use_statement_run
        "${CMAKE_SOURCE_DIR}/tests/cpp_header_use_statement.wio"
        "CppHeader use: 42"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_std_math_run
        "${CMAKE_SOURCE_DIR}/tests/std_math_run.wio"
        "Std math: 3/10/4/2.5"
        --run
    )

    add_wio_output_test(
        wio_test_std_math_extended_run
        "${CMAKE_SOURCE_DIR}/tests/std_math_extended_run.wio"
        "Std math extended: 49/-1/3/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_collections_alias_run
        "${CMAKE_SOURCE_DIR}/tests/std_collections_alias.wio"
        "Std collections: 22"
        --run
    )

    add_wio_output_test(
        wio_test_array_member_methods_run
        "${CMAKE_SOURCE_DIR}/tests/array_member_methods.wio"
        "Array members: 4/1/3/5/2/-1/2/2/2/9/8/2/9/2/true/wi-o-arena/4/true/true/5/5/true"
        --run
    )

    add_wio_output_test(
        wio_test_dictionary_member_methods_run
        "${CMAKE_SOURCE_DIR}/tests/dictionary_member_methods.wio"
        "Dict members: true/true/7/-1/true/7/11/4/true/true/true/true/5/true/4/1/10/5/50/3/5/true"
        --run
    )

    add_wio_output_test(
        wio_test_string_member_methods_run
        "${CMAKE_SOURCE_DIR}/tests/string_member_methods.wio"
        "String members: 9/true/true/true/true/4/8/W/a/\\?/Arena/Arena/Wio/Arena/Wio/Arena/wio arena/WIO ARENA/Wio World/Wio wio/xoxoxo/2/Arena/3/c/007/700/cba/!DLROWOIW/hi/true"
        --run
    )

    add_wio_output_test(
        wio_test_intrinsic_member_overloads_run
        "${CMAKE_SOURCE_DIR}/tests/intrinsic_member_overloads.wio"
        "Intrinsic overloads: true/true/4/4/8/Arena/Arena/  7/7  /2/3/3"
        --run
    )

    add_wio_output_test(
        wio_test_std_algorithms_callbacks_run
        "${CMAKE_SOURCE_DIR}/tests/std_algorithms_callbacks_run.wio"
        "Callback std: true/true/true/7"
        --run
    )

    add_wio_output_test(
        wio_test_std_collections_helpers_run
        "${CMAKE_SOURCE_DIR}/tests/std_collections_helpers_run.wio"
        "Std collections helpers: true/false/3/-1/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_collections_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_collections_surface_run.wio"
        "Std collections surface: true/3/9/7/5/1/4/a:b:c/true/3/true/2/3/3"
        --run
    )

    add_wio_output_test(
        wio_test_std_strings_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_strings_surface_run.wio"
        "Std strings surface: 9/true/4/Arena/Arena/cba/007/!DLROWOIW/hi/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_path_fs_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_path_fs_surface_run.wio"
        "Std path/fs surface: true/true/true/3/gamma/true/true/logs/final\\.log/final\\.log/2/logs/true/alpha\\|beta\\|gamma/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_console_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_console_surface_run.wio"
        "Std console surface: 1,2,3 xxx a/b"
        --run
    )

    add_wio_output_test(
        wio_test_std_algorithms_run
        "${CMAKE_SOURCE_DIR}/tests/std_algorithms_run.wio"
        "Std algorithms: 2/true/false/false/true/false"
        --run
    )

    add_wio_output_test(
        wio_test_std_algorithms_extended_run
        "${CMAKE_SOURCE_DIR}/tests/std_algorithms_extended_run.wio"
        "Std algorithms extended: true/2/-1/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_algorithms_callback_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_algorithms_callback_wave_run.wio"
        "Callback wave: 3/2/6/2/12/3/21/2/2/2"
        --run
    )

    add_wio_output_test(
        wio_test_std_assert_run
        "${CMAKE_SOURCE_DIR}/tests/std_assert_run.wio"
        "Std assert: ok"
        --run
    )

    add_wio_output_test(
        wio_test_lambda_contextual_typing_run
        "${CMAKE_SOURCE_DIR}/tests/lambda_contextual_typing_run.wio"
        "Lambda context: true/0/0/0"
        --run
    )

    add_wio_output_test(
        wio_test_inline_callback_inference_run
        "${CMAKE_SOURCE_DIR}/tests/inline_callback_inference_run.wio"
        "Inline callback inference: 3/12/21"
        --run
    )

    add_wio_output_test(
        wio_test_function_value_nested_call_run
        "${CMAKE_SOURCE_DIR}/tests/function_value_nested_call_run.wio"
        "Function value chain: 7"
        --run
    )

    add_wio_output_test(
        wio_test_factory_chain_call_run
        "${CMAKE_SOURCE_DIR}/tests/factory_chain_call_run.wio"
        "Factory chain: 0/0/0"
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
        wio_test_generic_overload_prefers_generic_over_numeric_conversion_run
        "${CMAKE_SOURCE_DIR}/tests/generic_overload_prefers_generic_over_numeric_conversion.wio"
        "Generic numeric preference: generic"
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
        wio_test_generic_component_constructor_expected_type_run
        "${CMAKE_SOURCE_DIR}/tests/generic_component_constructor_expected_type.wio"
        "Generic component expected type: 4/5, 6/7, 8/9"
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
        wio_test_generic_object_constructor_expected_type_run
        "${CMAKE_SOURCE_DIR}/tests/generic_object_constructor_expected_type.wio"
        "Generic ctor expected type: 7, 9, 11"
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

    add_wio_success_test(
        wio_test_generic_explicit_forward_export_dry_run
        "${CMAKE_SOURCE_DIR}/tests/generic_explicit_forward_export.wio"
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

    add_wio_failure_test_clean(
        wio_invalid_non_void_when_without_else
        "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_when_without_else.wio"
        "Functions with a return value must provide an 'else' fallback for 'when' guards"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_match_value_requires_assumed
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_value_requires_assumed.wio"
        "Value-producing match expressions must include an 'assumed' fallback case"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_match_assumed_must_be_last
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_assumed_must_be_last.wio"
        "The 'assumed' match case must be the last case"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_match_duplicate_assumed
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_duplicate_assumed.wio"
        "Match expressions can only contain one 'assumed' case"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_match_case_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_case_type_mismatch.wio"
        "Match case value type 'string' is not compatible with matched value type 'i32'"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_match_range_subject_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_range_subject_type_mismatch.wio"
        "Range match cases require the matched value to be numeric, but got 'string'"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_match_return_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_return_type_mismatch.wio"
        "All value-producing match cases must return compatible types\\. Expected 'i32', but got 'string'"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_when_fallback_return_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/when_fallback_return_type_mismatch.wio"
        "When guard fallback type mismatch! Expected 'i32', but got 'string'"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_when_condition_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/when_condition_type_mismatch.wio"
        "When guard condition must be a boolean, numeric, or reference type\\. Got: string"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_ref_requires_addressable_operand
        "${CMAKE_SOURCE_DIR}/tests/invalid/ref_requires_addressable_operand.wio"
        "The 'ref' operator requires an addressable variable, member, or indexed element"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_ref_immutable_to_mutable_param
        "${CMAKE_SOURCE_DIR}/tests/invalid/ref_immutable_to_mutable_param.wio"
        "expects a mutable reference"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_is_left_requires_object_or_interface
        "${CMAKE_SOURCE_DIR}/tests/invalid/is_left_requires_object_or_interface.wio"
        "The left side of the 'is' operator must be an object or interface value/reference\\. Got 'i32'"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_is_right_requires_object_or_interface
        "${CMAKE_SOURCE_DIR}/tests/invalid/is_right_requires_object_or_interface.wio"
        "The right side of the 'is' operator must be an object or interface type"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_fit_requires_numeric_or_object_interface
        "${CMAKE_SOURCE_DIR}/tests/invalid/fit_requires_numeric_or_object_interface.wio"
        "The 'fit' operator can only be used with numeric types or object/interface casts"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_if_fit_requires_is_condition
        "${CMAKE_SOURCE_DIR}/tests/invalid/if_fit_requires_is_condition.wio"
        "Pattern matching 'fit' can only be used with the 'is' operator"
        "Compiling failed:"
    )

    add_wio_failure_test(
        wio_invalid_component_inheritance
        "${CMAKE_SOURCE_DIR}/tests/invalid/component_inheritance.wio"
        "Components must be POD \\(Plain Old Data\\) and cannot inherit from objects or interfaces"
    )

    add_wio_failure_test(
        wio_invalid_component_method_not_allowed
        "${CMAKE_SOURCE_DIR}/tests/invalid/component_method_not_allowed.wio"
        "Components cannot define ordinary methods"
    )

    add_wio_failure_test(
        wio_invalid_object_inherits_component
        "${CMAKE_SOURCE_DIR}/tests/invalid/object_inherits_component.wio"
        "Object 'BadActor' cannot inherit from component 'Position'"
    )

    add_wio_failure_test(
        wio_invalid_final_object_inheritance
        "${CMAKE_SOURCE_DIR}/tests/invalid/final_object_inheritance.wio"
        "Object 'DerivedFromFinal' cannot inherit from final object 'FinalBase'"
    )

    add_wio_failure_test(
        wio_invalid_default_attribute_invalid_access
        "${CMAKE_SOURCE_DIR}/tests/invalid/default_attribute_invalid_access.wio"
        "@Default expects exactly one access modifier: public, private, or protected"
    )

    add_wio_failure_test(
        wio_invalid_private_base_member_access
        "${CMAKE_SOURCE_DIR}/tests/invalid/private_base_member_access.wio"
        "Cannot access private member 'secret' from outside the object"
    )

    add_wio_failure_test(
        wio_invalid_trust_requires_struct_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/trust_requires_struct_type.wio"
        "@Trust expects object/component/interface type names"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_entry_invalid_parameter_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/entry_invalid_parameter_type.wio"
        "Error \\[.*entry_invalid_parameter_type\\.wio:1:1\\]: The `Entry` function's parameter type must be `string\\[\\]`"
        "Compiling failed:"
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
        wio_invalid_use_path_missing_module_name
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_path_missing_module_name.wio"
        "Unfinished use statement\\. Use statements should finish with a module name\\."
    )

    add_wio_failure_test(
        wio_invalid_use_missing_user_module
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_missing_user_module.wio"
        "Module file was not found: missing_gameplay\\.wio"
    )

    add_wio_failure_test(
        wio_invalid_use_missing_std_module
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_missing_std_module.wio"
        "Module file was not found: missing_std_module\\.wio"
    )

    add_wio_failure_test(
        wio_invalid_use_alias_conflicts_with_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_alias_conflicts_with_symbol.wio"
        "Symbol 'console' already exists and cannot be used as an import alias\\."
    )

    add_wio_failure_test(
        wio_invalid_realm_conflicts_with_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/realm_conflicts_with_symbol.wio"
        "Symbol 'gameplay' already exists and is not a realm\\."
    )

    add_wio_failure_test(
        wio_invalid_function_call_missing_argument
        "${CMAKE_SOURCE_DIR}/tests/invalid/function_call_missing_argument.wio"
        "Expected function call argument after ','."
    )

    add_wio_failure_test(
        wio_invalid_array_literal_missing_element
        "${CMAKE_SOURCE_DIR}/tests/invalid/array_literal_missing_element.wio"
        "Expected array element after ','."
    )

    add_wio_failure_test(
        wio_invalid_dictionary_literal_missing_entry
        "${CMAKE_SOURCE_DIR}/tests/invalid/dictionary_literal_missing_entry.wio"
        "Expected dictionary entry after ','."
    )

    add_wio_failure_test(
        wio_invalid_static_array_type_missing_size
        "${CMAKE_SOURCE_DIR}/tests/invalid/static_array_type_missing_size.wio"
        "Static array types must declare a size after ';'."
    )

    add_wio_failure_test(
        wio_invalid_function_type_missing_parameter
        "${CMAKE_SOURCE_DIR}/tests/invalid/function_type_missing_parameter.wio"
        "Expected function type parameter after ','."
    )

    add_wio_failure_test(
        wio_invalid_function_parameter_missing_after_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/function_parameter_missing_after_comma.wio"
        "Expected function parameter after ','."
    )

    add_wio_failure_test(
        wio_invalid_lambda_parameter_missing_after_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/lambda_parameter_missing_after_comma.wio"
        "Expected lambda parameter after ','."
    )

    add_wio_failure_test(
        wio_invalid_generic_parameter_missing_after_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_parameter_missing_after_comma.wio"
        "Expected generic parameter after ','."
    )

    add_wio_failure_test(
        wio_invalid_attribute_argument_missing_after_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_argument_missing_after_comma.wio"
        "Expected attribute argument after ','."
    )

    add_wio_failure_test_clean(
        wio_invalid_lexer_invalid_escape_sequence
        "${CMAKE_SOURCE_DIR}/tests/invalid/lexer_invalid_escape_sequence.wio"
        "Error \\[.*lexer_invalid_escape_sequence\\.wio:2:[0-9]+\\]: Invalid escape sequence: \\\\q"
        "Compiling failed:"
    )

    add_wio_failure_test(
        wio_invalid_explicit_type_argument_missing_after_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/explicit_type_argument_missing_after_comma.wio"
        "Expected explicit type argument after ','."
    )

    add_wio_failure_test(
        wio_invalid_generic_type_argument_missing_after_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_type_argument_missing_after_comma.wio"
        "Expected generic type argument after ','."
    )

    add_wio_failure_test(
        wio_invalid_native_function_with_body
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_function_with_body.wio"
        "@Native functions cannot define a Wio body"
    )

    add_wio_failure_test_clean(
        wio_invalid_native_function_with_body_clean
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_function_with_body.wio"
        "@Native functions cannot define a Wio body"
        "Compiling failed:"
    )

    add_wio_failure_test(
        wio_invalid_native_method_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_method_not_supported.wio"
        "@Native is currently supported only for top-level functions"
    )

    add_wio_failure_test(
        wio_invalid_missing_cpp_header_use
        "${CMAKE_SOURCE_DIR}/tests/invalid/missing_cpp_header_use.wio"
        "Native C\\+\\+ header 'missing_cpp_header_use_test.h' referenced by use @CppHeader in '.*missing_cpp_header_use\\.wio' was not found"
    )

    add_wio_failure_test(
        wio_invalid_missing_cpp_header_native_attr
        "${CMAKE_SOURCE_DIR}/tests/invalid/missing_cpp_header_native_attr.wio"
        "Native C\\+\\+ header 'missing_cpp_header_attr_test.h' referenced by @CppHeader on native function 'Invoke' in '.*missing_cpp_header_native_attr\\.wio' was not found"
    )

    add_wio_failure_test(
        wio_invalid_missing_cpp_header_std_import
        "${CMAKE_SOURCE_DIR}/tests/invalid/missing_cpp_header_std_import.wio"
        "Native C\\+\\+ header 'missing_std_header_for_test.h' referenced by use @CppHeader in '.*std[\\\\/]assert[\\\\/]missing_cpp_header\\.wio' was not found"
    )

    add_wio_failure_test(
        wio_invalid_numeric_literal_context_u8_out_of_range
        "${CMAKE_SOURCE_DIR}/tests/invalid/numeric_literal_context_u8_out_of_range.wio"
        "Integer literal '300' does not fit into expected type 'u8'"
    )

    add_wio_failure_test(
        wio_invalid_numeric_literal_context_f32_out_of_range
        "${CMAKE_SOURCE_DIR}/tests/invalid/numeric_literal_context_f32_out_of_range.wio"
        "Float literal '1\\.0e100' does not fit into expected type 'f32'"
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_cpp_line_mapping_native_name
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_cpp_line_mapping_native_name.wio"
        "Error \\[.*backend_cpp_line_mapping_native_name\\.wio:6:12\\]:"
        --run
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_linker_missing_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_linker_missing_symbol.wio"
        "Error \\[backend:linker\\]: Native linker: (undefined reference to|unresolved external symbol|symbol\\(s\\) not found|cannot find)"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_compile_error_from_native_source
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_compile_error_from_native_source.wio"
        "Error \\[.*backend_compile_error_source\\.cpp:[0-9]+:[0-9]+\\]: Native C\\+\\+ compiler:"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/backend_compile_error_source.cpp"
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_generated_code_failure
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_generated_code_failure.wio"
        "Error \\[backend:generated-cpp:[0-9]+:[0-9]+\\]: Generated C\\+\\+ wrapper:"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_include_chain_failure
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_include_chain_failure.wio"
        "Note \\[.*backend_include_chain_root\\.h:[0-9]+\\]: Native C\\+\\+ compiler: Included from here"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --run
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_include_chain_generated_note
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_include_chain_failure.wio"
        "Note \\[backend:generated-cpp:[0-9]+\\]: Generated C\\+\\+ wrapper: Included from here"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --run
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_template_context_note
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_compile_error_from_native_source.wio"
        "Note \\[.*backend_template_context_source\\.cpp:[0-9]+:[0-9]+\\]: Native C\\+\\+ compiler: required from here"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/backend_template_context_source.cpp"
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_template_instantiation_note
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_compile_error_from_native_source.wio"
        "Note \\[.*backend_template_context_source\\.cpp\\]: Native C\\+\\+ compiler: In instantiation of 'void backendTemplateContextLeaf\\(\\) \\[with T = int\\]':"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/backend_template_context_source.cpp"
    )

    add_wio_failure_test(
        wio_invalid_cpp_name_native_invalid_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/cpp_name_native_invalid_symbol.wio"
        "@CppName for @Native must be a valid C\\+\\+ identifier path like foo::bar\\."
    )

    add_wio_failure_test(
        wio_invalid_cpp_name_export_invalid_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/cpp_name_export_invalid_symbol.wio"
        "@CppName for @Export must be a plain C/C\\+\\+ symbol name like FooBar\\."
    )

    add_wio_failure_test(
        wio_invalid_missing_module_dir_arg
        "${CMAKE_SOURCE_DIR}/tests/import_path_root.wio"
        "Module search directory '.*missing_module_dir.*' was not found or is not a directory"
        --module-dir
        "${CMAKE_SOURCE_DIR}/tests/missing_module_dir"
    )

    add_wio_failure_test(
        wio_invalid_missing_include_dir_arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_bridge.wio"
        "Include directory '.*missing_include_dir.*' was not found or is not a directory"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/missing_include_dir"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_math.cpp"
    )

    add_wio_failure_test(
        wio_invalid_missing_link_dir_arg
        "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
        "Link directory '.*missing_link_dir.*' was not found or is not a directory"
        --link-dir
        "${CMAKE_SOURCE_DIR}/tests/missing_link_dir"
    )

    add_wio_failure_test(
        wio_invalid_missing_backend_source_arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_bridge.wio"
        "Backend source file '.*missing_backend_source\\.cpp' was not found"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/missing_backend_source.cpp"
    )

    add_wio_failure_test_clean(
        wio_invalid_cli_unknown_build_target
        "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
        "Error \\[cli\\]: Unknown build target: nonsense\\. Expected one of: exe, static, shared\\."
        "Unexpected Error:"
        --target
        nonsense
    )

    add_wio_failure_test(
        wio_invalid_missing_link_library_file_arg
        "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
        "Linked library file '.*missing_link_library\\.lib' was not found"
        --link-lib
        "${CMAKE_SOURCE_DIR}/tests/native/missing_link_library.lib"
    )

    add_wio_failure_test(
        wio_invalid_export_string_return_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/export_string_return_not_supported.wio"
        "@Export currently supports only primitive or void return types"
    )

    add_wio_failure_test(
        wio_invalid_duplicate_export_cpp_name
        "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_export_cpp_name.wio"
        "Exported C\\+\\+ symbol 'WioEcho' is already declared by another @Export or module lifecycle function\\."
    )

    add_wio_failure_test(
        wio_invalid_duplicate_export_module_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_export_module_symbol.wio"
        "Exported C\\+\\+ symbol 'WioModuleLoad' is already declared by another @Export or module lifecycle function\\."
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
        "Step clauses are currently supported only for range or array iteration"
    )

    add_wio_failure_test(
        wio_invalid_for_array_step_requires_integer
        "${CMAKE_SOURCE_DIR}/tests/invalid/for_array_step_requires_integer.wio"
        "Array step expressions must be integer values"
    )

    add_wio_failure_test(
        wio_invalid_array_member_requires_mutable_receiver
        "${CMAKE_SOURCE_DIR}/tests/invalid/array_member_requires_mutable_receiver.wio"
        "Container member 'Push' requires a mutable receiver"
    )

    add_wio_failure_test(
        wio_invalid_static_array_mutating_member_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/static_array_mutating_member_not_supported.wio"
        "Static arrays do not support member 'Push'. Use a dynamic array instead"
    )

    add_wio_failure_test(
        wio_invalid_dictionary_member_requires_mutable_receiver
        "${CMAKE_SOURCE_DIR}/tests/invalid/dictionary_member_requires_mutable_receiver.wio"
        "Container member 'Remove' requires a mutable receiver"
    )

    add_wio_failure_test(
        wio_invalid_string_member_requires_mutable_receiver
        "${CMAKE_SOURCE_DIR}/tests/invalid/string_member_requires_mutable_receiver.wio"
        "Container member 'Append' requires a mutable receiver"
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
        wio_invalid_break_outside_loop
        "${CMAKE_SOURCE_DIR}/tests/invalid/break_outside_loop.wio"
        "'break' statement can only be used inside a loop"
    )

    add_wio_failure_test(
        wio_invalid_continue_outside_loop
        "${CMAKE_SOURCE_DIR}/tests/invalid/continue_outside_loop.wio"
        "'continue' statement can only be used inside a loop"
    )

    add_wio_failure_test(
        wio_invalid_c_style_for_invalid_condition_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/c_style_for_invalid_condition_type.wio"
        "For-loop condition must be a boolean, numeric, or reference type. Got: string"
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
        "No matching overload for 'PickFirst' with arguments \\(i32, string\\)\\. Candidates: PickFirst<T>\\(T, T\\) -> T\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_interface_method_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_interface_method_not_supported.wio"
        "Generic methods are currently supported only on object methods"
    )

    add_wio_failure_test(
        wio_invalid_generic_component_method_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_component_method_not_supported.wio"
        "Generic methods are currently supported only on object methods"
    )

    add_wio_failure_test(
        wio_invalid_generic_explicit_argument_count_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_explicit_argument_count_mismatch.wio"
        "Error \\[.*generic_explicit_argument_count_mismatch\\.wio:6:16\\]: Generic call to 'MakeLabel' expects 2 explicit type arguments, but got 1\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_overload_ambiguous
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_overload_ambiguous.wio"
        "Ambiguous function call"
    )

    add_wio_failure_test(
        wio_invalid_overload_no_matching_signature
        "${CMAKE_SOURCE_DIR}/tests/invalid/overload_no_matching_signature.wio"
        "No matching overload for 'Mix' with arguments \\(bool\\)\\. Candidates: Mix\\(i32\\) -> i32; Mix\\(string\\) -> string\\."
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
        wio_invalid_generic_native_call_not_instantiated
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_native_call_not_instantiated.wio"
        "Generic interop function 'DoubleValue' does not declare @Instantiate<string>\\."
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_failure_test(
        wio_invalid_generic_export_call_not_instantiated
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_call_not_instantiated.wio"
        "Generic interop function 'Echo' does not declare @Instantiate<bool>\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_object_constructor_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_object_constructor_not_supported.wio"
        "Generic constructors and destructors are not supported yet"
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

    add_wio_failure_test(
        wio_invalid_lambda_contextual_return_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/lambda_contextual_return_mismatch.wio"
        "Return type mismatch"
    )

    add_wio_failure_test(
        wio_invalid_exported_object_non_abi_field
        "${CMAKE_SOURCE_DIR}/tests/invalid/exported_object_non_abi_field.wio"
        "@Export object 'BadProfile' exposes public field 'worker' with type 'IWorker' that is not yet SDK-exportable\\."
    )

    add_wio_failure_test(
        wio_invalid_exported_component_requires_host_ctor
        "${CMAKE_SOURCE_DIR}/tests/invalid/exported_component_requires_host_ctor.wio"
        "@Export component 'HiddenStats' must expose at least one public host-callable constructor with only C-ABI-safe parameters\\."
    )

    add_wio_failure_test(
        wio_invalid_exported_object_private_ctor
        "${CMAKE_SOURCE_DIR}/tests/invalid/exported_object_private_ctor.wio"
        "@Export object 'HiddenFactory' constructor must be public to be host-callable\\."
    )
endif()
