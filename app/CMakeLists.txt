file(GLOB_RECURSE WIO_TEST_FILES "${CMAKE_SOURCE_DIR}/tests/*.wio")
file(GLOB_RECURSE WIO_STD_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/std/*.wio")
set(WIO_SELFHOST_CLI_SOURCE "${CMAKE_SOURCE_DIR}/cli/main.wio")
set(WIO_SELFHOST_PROJECT_NEW_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_new.wio")
set(WIO_SELFHOST_PROJECT_MANIFEST_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_manifest.wio")
set(WIO_SELFHOST_PROJECT_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_commands.wio")
set(WIO_SELFHOST_COMMON_SOURCE "${CMAKE_SOURCE_DIR}/cli/common.wio")
set(WIO_SELFHOST_GLOBAL_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/global_commands.wio")
set(WIO_SELFHOST_ENV_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/env_commands.wio")
set(WIO_SELFHOST_BIND_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/bind_commands.wio")
set(WIO_SELFHOST_PACKAGE_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/package_commands.wio")
set(WIO_SELFHOST_FILE_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/file_commands.wio")
set(WIO_SELFHOST_DEV_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/dev_commands.wio")
set(WIO_SELFHOST_PERF_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/perf_commands.wio")
set(WIO_SELFHOST_ARGONAUT_SOURCE "${CMAKE_SOURCE_DIR}/cli/vendor/argonaut.wio")

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
        wio_runtime_host
)

target_include_directories(wio_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/include
)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" WIO_APP_DEFAULT_ROOT_DIR)
target_compile_definitions(wio_app
    PRIVATE
        WIO_DEFAULT_ROOT_DIR="${WIO_APP_DEFAULT_ROOT_DIR}"
        WIO_VERSION="${PROJECT_VERSION}"
)

install(TARGETS wio_app
    RUNTIME DESTINATION bin
)

if(EXISTS "${WIO_SELFHOST_CLI_SOURCE}" AND EXISTS "${WIO_SELFHOST_ARGONAUT_SOURCE}")
    set(WIO_SELFHOST_CLI_DEPENDENCIES
        "${WIO_SELFHOST_CLI_SOURCE}"
        "${WIO_SELFHOST_PROJECT_NEW_SOURCE}"
        "${WIO_SELFHOST_PROJECT_MANIFEST_SOURCE}"
        "${WIO_SELFHOST_PROJECT_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_COMMON_SOURCE}"
        "${WIO_SELFHOST_GLOBAL_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_ENV_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_BIND_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_PACKAGE_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_FILE_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_DEV_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_PERF_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_ARGONAUT_SOURCE}"
    )
    if(CMAKE_CONFIGURATION_TYPES)
        set(WIO_SELFHOST_CLI_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/wio-selfhost${CMAKE_EXECUTABLE_SUFFIX}"
        )
    else()
        set(WIO_SELFHOST_CLI_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/wio-selfhost${CMAKE_EXECUTABLE_SUFFIX}"
        )
    endif()

    add_custom_command(
        OUTPUT "${WIO_SELFHOST_CLI_OUTPUT}"
        COMMAND
            ${CMAKE_COMMAND} -E env
            "WIO_ROOT=${CMAKE_SOURCE_DIR}"
            $<TARGET_FILE:wio_app>
            "${WIO_SELFHOST_CLI_SOURCE}"
            --target exe
            --output "${WIO_SELFHOST_CLI_OUTPUT}"
            --module-dir "${CMAKE_SOURCE_DIR}/cli"
            --module-dir "${CMAKE_SOURCE_DIR}/cli/vendor"
        DEPENDS
            wio_app
            ${WIO_SELFHOST_CLI_DEPENDENCIES}
        COMMENT "Building the Argonaut-Wio self-hosted CLI companion..."
        VERBATIM
    )

    add_custom_target(wio_selfhost_cli ALL
        DEPENDS "${WIO_SELFHOST_CLI_OUTPUT}"
    )
    set_target_properties(wio_selfhost_cli PROPERTIES FOLDER "app")

    install(
        PROGRAMS "${WIO_SELFHOST_CLI_OUTPUT}"
        TYPE BIN
    )
endif()

function(add_wio_dev_target target_name source_file)
    add_custom_target(
        ${target_name}
        COMMAND ${CMAKE_COMMAND} -E env
            "WIO_ROOT=${CMAKE_SOURCE_DIR}"
            $<TARGET_FILE:wio_app> "${source_file}" ${ARGN}
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

find_package(Python3 COMPONENTS Interpreter QUIET)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    set(WIO_FUZZ_WORK_DIR "${CMAKE_BINARY_DIR}/fuzz-smoke")
    add_test(
        NAME wio_fuzz_smoke
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/fuzz/wio_fuzz.py"
            --wio "$<TARGET_FILE:wio_app>"
            --backend "${WIO_BACKEND_CXX_COMPILER}"
            --root "${CMAKE_SOURCE_DIR}"
            --corpus "${CMAKE_SOURCE_DIR}/tests/fuzz/corpus"
            --work "${WIO_FUZZ_WORK_DIR}"
            --iterations 24
            --seed 5712207
            --timeout 5
            --max-diagnostics 64
    )
    set_tests_properties(wio_fuzz_smoke PROPERTIES
        TIMEOUT 180
        LABELS "fuzz;p0"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_custom_target(
        wio_fuzz
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/fuzz/wio_fuzz.py"
            --wio "$<TARGET_FILE:wio_app>"
            --backend "${WIO_BACKEND_CXX_COMPILER}"
            --root "${CMAKE_SOURCE_DIR}"
            --corpus "${CMAKE_SOURCE_DIR}/tests/fuzz/corpus"
            --work "${WIO_FUZZ_WORK_DIR}"
            --iterations 250
            --seed 5712207
            --timeout 5
            --max-diagnostics 64
        DEPENDS wio_app
        USES_TERMINAL
        VERBATIM
    )
endif()

set(
    WIO_TRACEABILITY_TEST_REGEX
    "(wio_test_(use_alias_dry_run|realm_basic_run|realm_nested_run|realm_qualified_from_run|object_component_access_semantics_run|interface_ref_upcast_run|component_embedded_member_ctor_run|for_in_dynamic_array_run|for_dict_key_value_run|for_component_binding_run|c_style_for_loop_run|match_value_basic_run|match_statement_basic_run|when_guard_fallback_value_run|fit_numeric_clamp_run|const_scalar_expressions_run|const_string_text_run|numeric_literal_suffixes_run|numeric_literal_signed_mins_run|default_parameters_run|default_parameter_native_bridge_run|function_parameter_pack_run|function_parameter_pack_explicit_type_arguments_run|function_parameter_pack_constraints_run|function_parameter_pack_defaults_run|pack_storage_write_semantics_run|pack_meta_and_generic_declarations_run|std_meta_second_wave_run|std_meta_wave3_run|generic_apply_function_constraints_run|generic_apply_type_and_object_run|generic_native_predicate_bridge_run|generic_object_explicit_specialization_run|generic_component_constructor_deduction_run|generic_object_constructor_deduction_run|generic_fit_target_constraints_dry_run|generic_parameterized_type_parameter_dry_run|ref_mut_container_members_dry_run|object_method_outer_const_dry_run|object_self_return_run|array_member_methods_run|dictionary_member_methods_run|string_member_methods_run|std_convert_surface_run|std_text_surface_run|std_algorithms_utility_wave_run|direct_indexing_run|fixed_array_auto_extent_run|control_flow_return_analysis_run|mutable_data_ergonomics_run|std_contract_v1_run|std_heap_box_run|std_heap_box_semantics_run|std_event_any_run|any_generic_box_fit_run|any_opaque_boundary_run|any_value_reference_semantics_run|native_opaque_bridge_run|native_box_bridge_run|direct_module_import_run|std_enum_flagset_surface_run|std_module_reimport_alias_run|enum_member_scope_run|binding_import_smoke|binding_manifest_smoke|sdk_invalid_api_contract|sdk_module_easy_host_interop|sdk_exported_types_host_interop|sdk_exported_inherited_host_interop|sdk_exported_complex_fields_host_interop|sdk_module_hot_reload_interop|sdk_hot_reload_stale_wrappers_interop|sdk_module_stale_bindings_interop|pack_export_host_interop)|wio_runtime_(direct_index_out_of_range|string_to_i32_invalid)|wio_invalid_(entry_invalid_parameter_type|entry_invalid_return_type|entry_too_many_parameters|entry_missing_body|entry_method_not_top_level|use_missing_user_module|use_missing_std_module|use_alias_conflicts_with_symbol|use_alias_hides_unqualified_root_symbol|use_without_direct_import_keeps_namespaced_access|realm_conflicts_with_symbol|ref_requires_addressable_operand|ref_immutable_to_mutable_param|is_left_requires_object_or_interface|is_right_requires_object_or_interface|fit_requires_numeric_or_object_interface|if_fit_requires_is_condition|component_method_not_allowed|object_inherits_component|final_object_inheritance|default_attribute_invalid_access|private_base_member_access|protected_member_access_outside_hierarchy|trust_requires_struct_type|let_without_initializer|mut_without_initializer|let_without_type_or_initializer|const_without_initializer|const_interpolated_text_not_folded|const_from_non_const_identifier|const_function_call_not_supported|const_array_not_supported|default_parameter_non_trailing|default_parameter_type_mismatch|default_parameter_declaration_only_not_supported|default_parameter_entry_not_allowed|default_parameter_overload_conflict|generic_fit_target_without_supported_constraint|generic_parameter_pack_non_trailing|parameter_pack_non_trailing|parameter_pack_missing_matching_generic_pack|parameter_pack_bare_use_not_allowed|parameter_pack_default_not_supported|parameter_pack_method_not_supported|parameter_pack_apply_reject_non_integer|parameter_pack_export_requires_instantiate|parameter_pack_instantiate_predicate_not_allowed|parameter_pack_view_assignment_not_allowed|generic_apply_reject_function_call|generic_apply_reject_type_alias|generic_apply_reject_constructor|generic_apply_non_generic_function|generic_native_predicate_reject_non_integer|generic_specialization_requires_primary|generic_specialization_argument_count_mismatch|generic_specialization_duplicate|generic_specialization_native_component|pack_field_requires_generic_pack|pack_index_requires_compile_time_constant|pack_index_requires_const_declaration|pack_index_from_size_requires_subtraction|type_pack_to_static_array_not_supported|match_value_requires_assumed|match_assumed_must_be_last|match_duplicate_assumed|match_case_type_mismatch|match_return_type_mismatch|non_void_when_without_else|non_void_missing_return|non_void_if_missing_return|array_index_requires_integer|static_array_index_out_of_range|fixed_array_auto_extent_without_initializer|fixed_array_auto_extent_parameter|fixed_array_auto_extent_dynamic_source|fixed_array_auto_extent_ragged_nested|opaque_arithmetic_not_supported|opaque_is_not_supported|any_non_null_equality_not_supported|when_condition_type_mismatch|when_fallback_return_type_mismatch|for_in_requires_array|for_dict_requires_two_bindings|for_component_binding_count_mismatch|for_range_step_zero|break_outside_loop|continue_outside_loop|c_style_for_invalid_condition_type|array_member_requires_mutable_receiver|dictionary_member_requires_mutable_receiver|string_member_requires_mutable_receiver))"
)

set(
    WIO_FREEZE_TEST_REGEX
    "(attribute|pipeline|match|application|async|lambda_capture)"
)

set(
    WIO_LANGUAGE_COHERENCE_TEST_REGEX
    "^(wio_test_(attribute_compact_declaration_run|generic_partial_specialization_ordering_run|extension_default_parameters_run|extension_generic_methods_run|match_enum_exhaustive_guards_run|unicode_text_literal_run|unicode_text_export_host_interop|const_string_text_run|fixed_array_auto_extent_run)|wio_invalid_(attribute_named_unknown|attribute_named_duplicate|attribute_named_missing_required|attribute_compact_conflict|generic_partial_specialization_structural_ambiguous|extension_generic_constraint_reject|match_enum_non_exhaustive|match_enum_duplicate_unguarded|match_literal_guard_type_mismatch|match_assumed_guard|text_requires_explicit_utf8_validation|text_operator|text_string_mixed_operation|text_index_assignment|const_interpolated_text_not_folded|fixed_array_auto_extent_without_initializer|fixed_array_auto_extent_parameter|fixed_array_auto_extent_dynamic_source|fixed_array_auto_extent_ragged_nested))$"
)

add_custom_target(
    wio_tests_traceability
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -R "${WIO_TRACEABILITY_TEST_REGEX}" --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS wio_app
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    wio_tests_freeze
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -R "${WIO_TRACEABILITY_TEST_REGEX}" --output-on-failure
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -R "${WIO_FREEZE_TEST_REGEX}" --output-on-failure
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -R "${WIO_LANGUAGE_COHERENCE_TEST_REGEX}" --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS wio_app wio_async_runtime_stress
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    wio_tests_language_coherence
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -R "${WIO_LANGUAGE_COHERENCE_TEST_REGEX}" --output-on-failure
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

    add_executable(wio_sdk_invalid_api_host
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_invalid_api_host.cpp"
    )
    target_link_libraries(wio_sdk_invalid_api_host
        PRIVATE
            wio_sdk
    )
    add_test(
        NAME wio_test_sdk_invalid_api_contract
        COMMAND $<TARGET_FILE:wio_sdk_invalid_api_host>
    )
    set_tests_properties(
        wio_test_sdk_invalid_api_contract
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

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
        wio_test_direct_module_import_run
        "${CMAKE_SOURCE_DIR}/tests/modules/direct_import_root.wio"
        "Direct import math: 5, 3"
        --run
    )

    add_wio_output_test(
        wio_test_direct_module_import_alias_run
        "${CMAKE_SOURCE_DIR}/tests/modules/direct_import_alias_root.wio"
        "Direct import alias math: 5, 3, 4"
        --run
    )

    add_wio_output_test(
        wio_test_mutable_data_ergonomics_run
        "${CMAKE_SOURCE_DIR}/tests/mutable_data_ergonomics_run.wio"
        "Mutable ergonomics: ok"
        --run
    )

    add_test(
        NAME wio_test_file_cli_check
        COMMAND $<TARGET_FILE:wio_app> file check "${CMAKE_SOURCE_DIR}/tests/test1.wio"
    )
    set_tests_properties(
        wio_test_file_cli_check
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_file_cli_tokens
        COMMAND $<TARGET_FILE:wio_app> file tokens "${CMAKE_SOURCE_DIR}/tests/test1.wio"
    )
    set_tests_properties(
        wio_test_file_cli_tokens
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_file_cli_run_args_forwarding
        COMMAND $<TARGET_FILE:wio_app> file run "${CMAKE_SOURCE_DIR}/tests/file_run_args_forwarding_run.wio" -- alpha "two words" "--literal" "amp&ersand" "--"
    )
    set_tests_properties(
        wio_test_file_cli_run_args_forwarding
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_file_cli_run_exit_code
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/file_run_exit_code.wio
            -DWIO_EXPECT_EXIT_CODE=37
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectExitCode.cmake"
    )
    set_tests_properties(
        wio_test_file_cli_run_exit_code
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_generated_cpp_cleanup
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/test1.wio
            -DWIO_MODE=compiler
            -DWIO_WORK_DIR=${CMAKE_BINARY_DIR}/artifact-policy/generated-cpp-cleanup
            -DWIO_EXPECT_OUTPUT=${CMAKE_BINARY_DIR}/artifact-policy/generated-cpp-cleanup/test1-built${CMAKE_EXECUTABLE_SUFFIX}
            "-DWIO_ARGS=--output;${CMAKE_BINARY_DIR}/artifact-policy/generated-cpp-cleanup/test1-built${CMAKE_EXECUTABLE_SUFFIX}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioArtifactPolicy.cmake"
    )
    set_tests_properties(
        wio_test_generated_cpp_cleanup
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_file_cli_run_uses_hidden_output
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/file_run_args_forwarding_run.wio
            -DWIO_MODE=file-run
            -DWIO_WORK_DIR=${CMAKE_BINARY_DIR}/artifact-policy/file-cli-run
            -DWIO_EXPECT=file-run-args-ok
            "-DWIO_ARGS=--;alpha;two words;--literal;amp&ersand;--"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioArtifactPolicy.cmake"
    )
    set_tests_properties(
        wio_test_file_cli_run_uses_hidden_output
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_env_cli_print
        COMMAND $<TARGET_FILE:wio_app> env print --wio-root "${CMAKE_SOURCE_DIR}" --shell powershell --add-path
    )
    set_tests_properties(
        wio_test_env_cli_print
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_env_cli_setup_preview
        COMMAND $<TARGET_FILE:wio_app> env setup --wio-root "${CMAKE_SOURCE_DIR}" --no-prompt --add-path
    )
    set_tests_properties(
        wio_test_env_cli_setup_preview
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_env_cli_status
        COMMAND $<TARGET_FILE:wio_app> env status --wio-root "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_env_cli_status
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_env_cli_remove_preview
        COMMAND $<TARGET_FILE:wio_app> env remove --wio-root "${CMAKE_SOURCE_DIR}" --no-prompt --remove-path
    )
    set_tests_properties(
        wio_test_env_cli_remove_preview
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_env_cli_doctor
        COMMAND $<TARGET_FILE:wio_app> env doctor --wio-root "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_env_cli_doctor
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_env_cli_backend_smoke
        COMMAND $<TARGET_FILE:wio_app> env doctor --wio-root "${CMAKE_SOURCE_DIR}" --backend-smoke
    )
    set_tests_properties(
        wio_test_env_cli_backend_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            TIMEOUT 120
    )

    add_test(
        NAME wio_test_cli_help_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioCliHelpSmoke.cmake"
    )
    set_tests_properties(
        wio_test_cli_help_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_self_hosted_cli_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/entry_default_void_run.wio
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioSelfHostedCliSmoke.cmake"
    )

    add_test(
        NAME wio_test_self_hosted_dev_cli_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
            -DWIO_CONFIG=$<CONFIG>
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioSelfHostedDevCliSmoke.cmake"
    )
    set_tests_properties(
        wio_test_self_hosted_dev_cli_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            TIMEOUT 300
    )
    set_tests_properties(
        wio_test_self_hosted_cli_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_package_cli_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
            -DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/package-smoke
            -DWIO_CONFIG=$<CONFIG>
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioPackageSmoke.cmake"
    )
    set_tests_properties(
        wio_test_package_cli_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESOURCE_LOCK wio_distribution_package
    )

    add_test(
        NAME wio_test_perf_cli_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/perf-smoke-test
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioPerfSmoke.cmake"
    )

    set_tests_properties(
        wio_test_perf_cli_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESOURCE_LOCK wio_distribution_package
    )

    add_test(
        NAME wio_test_project_cli_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/project-smoke
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioProjectSmoke.cmake"
    )
    set_tests_properties(
        wio_test_project_cli_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_project_emit_cpp_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/project-emit-cpp-smoke
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioProjectEmitCppSmoke.cmake"
    )
    set_tests_properties(
        wio_test_project_emit_cpp_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_project_test_package_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/project-test-package-smoke
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioProjectTestPackageSmoke.cmake"
    )
    set_tests_properties(
        wio_test_project_test_package_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_hybrid_project_cli_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/hybrid-project-smoke
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioHybridProjectSmoke.cmake"
    )
    set_tests_properties(
        wio_test_hybrid_project_cli_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_example_plain_app_run
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/plain_app
            -DWIO_EXPECT=Hello\ from\ the\ plain\ Wio\ app\ example.
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExampleProject.cmake"
    )
    set_tests_properties(
        wio_test_example_plain_app_run
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_example_native_app_run
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/native_app
            -DWIO_EXPECT=Native\ multiply:
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExampleProject.cmake"
    )
    set_tests_properties(
        wio_test_example_native_app_run
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_example_observatory_run
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/wio_observatory
            -DWIO_EXPECT=OBSERVATORY\ AUDIT\ COMPLETE
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExampleProject.cmake"
    )
    set_tests_properties(
        wio_test_example_observatory_run
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_example_hybrid_module_run
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/hybrid_module
            -DWIO_EXPECT=Hybrid\ project:\ before=11\ afterAdd=16\ afterTick=18
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExampleProject.cmake"
    )
    set_tests_properties(
        wio_test_example_hybrid_module_run
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_example_binding_import
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/binding_import
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/binding-example-smoke
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioBindingExample.cmake"
    )
    set_tests_properties(
        wio_test_example_binding_import
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_packaged_file_run_uses_user_cache
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
            -DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/package-file-run-smoke
            -DWIO_CONFIG=$<CONFIG>
            -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/file_run_args_forwarding_run.wio
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioPackagedFileRunSmoke.cmake"
    )

    set(WIO_QUALIFICATION_BACKEND_ROOT "")
    get_filename_component(wio_qualification_compiler_name "${CMAKE_CXX_COMPILER}" NAME)
    if(WIN32 AND wio_qualification_compiler_name MATCHES "^g\\+\\+(\\.exe)?$")
        get_filename_component(wio_qualification_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(WIO_QUALIFICATION_BACKEND_ROOT "${wio_qualification_compiler_bin}" DIRECTORY)
    endif()

    add_test(
        NAME wio_test_installed_package_qualification
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
            -DWIO_SCRATCH_DIR=${CMAKE_SOURCE_DIR}/.wio-qualification
            -DWIO_CONFIG=$<CONFIG>
            -DWIO_PORTABLE_BACKEND_ROOT=${WIO_QUALIFICATION_BACKEND_ROOT}
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioInstalledPackageQualification.cmake"
    )
    set_tests_properties(
        wio_test_installed_package_qualification
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            TIMEOUT 900
            RESOURCE_LOCK wio_distribution_package
    )
    set_tests_properties(
        wio_test_packaged_file_run_uses_user_cache
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESOURCE_LOCK wio_distribution_package
    )

    add_wio_success_test(
        wio_test_wio_tool_run_host_interop_dry_run
        "${CMAKE_SOURCE_DIR}/scripts/wio/run_host_interop.wio"
    )

    add_wio_success_test(
        wio_test_wio_tool_run_hybrid_arena_demo_dry_run
        "${CMAKE_SOURCE_DIR}/scripts/wio/run_hybrid_arena_demo.wio"
    )

    add_wio_output_test(
        wio_test_std_process_run_smoke_run
        "${CMAKE_SOURCE_DIR}/tests/std_process_run_smoke.wio"
        "std-process-run-ok"
        --run
    )

    add_wio_output_test(
        wio_test_import_all_alias_run
        "${CMAKE_SOURCE_DIR}/tests/modules/import_all_alias_root.wio"
        "import-all-alias-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_process_executable_path_run
        "${CMAKE_SOURCE_DIR}/tests/std_process_executable_path_run.wio"
        "Std process executable path: true/true"
        --run
    )

    add_test(
        NAME wio_test_binding_import_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_OUTPUT=${CMAKE_BINARY_DIR}/generated/binding_import_smoke.wio
            "-DWIO_TOOL_ARGS=bind;import;--header;${CMAKE_SOURCE_DIR}/tests/native/binding_import_smoke.h;--realm;binding_import_smoke;--output;${CMAKE_BINARY_DIR}/generated/binding_import_smoke.wio"
            "-DWIO_COMPILER_ARGS=--dry-run;--target;static;--include-dir;${CMAKE_SOURCE_DIR}/tests/native"
            "-DWIO_EXPECT_1=const DefaultPort: i32 = 8080;"
            "-DWIO_EXPECT_2=component color3 {"
            "-DWIO_EXPECT_3=rgb: [u8; 3];"
            "-DWIO_EXPECT_4=flagset feature_mask {"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunBindingGenerationSmoke.cmake"
    )
    set_tests_properties(
        wio_test_binding_import_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME wio_test_binding_manifest_smoke
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_OUTPUT=${CMAKE_BINARY_DIR}/generated/binding_manifest_smoke.wio
            "-DWIO_TOOL_ARGS=bind;new;--manifest;${CMAKE_SOURCE_DIR}/tests/native/binding_manifest_smoke.json;--output;${CMAKE_BINARY_DIR}/generated/binding_manifest_smoke.wio"
            "-DWIO_COMPILER_ARGS=--dry-run;--target;static;--include-dir;${CMAKE_SOURCE_DIR}/tests/native"
            "-DWIO_EXPECT_1=const DefaultPort: i32 = 8080;"
            "-DWIO_EXPECT_2=component color3 {"
            "-DWIO_EXPECT_3=flagset feature_mask {"
            "-DWIO_EXPECT_4=fn Scale(value: ref color3, factor: f32);"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunBindingGenerationSmoke.cmake"
    )
    set_tests_properties(
        wio_test_binding_manifest_smoke
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_import_path_module_dir_run
        "${CMAKE_SOURCE_DIR}/tests/import_path_root.wio"
        "Import path threat: 45"
        --run
        --module-dir
        "${CMAKE_SOURCE_DIR}/tests/imports"
    )

    add_wio_failure_test_clean(
        wio_invalid_imported_parser_error_no_cascade
        "${CMAKE_SOURCE_DIR}/tests/invalid/imported_parser_error_no_cascade.wio"
        "Unexpected token: expected identifier"
        "Undefined symbol|No matching overload|Generated C\\+\\+ output:"
        --module-dir
        "${CMAKE_SOURCE_DIR}/tests/modules"
    )

    add_wio_failure_test_clean(
        wio_invalid_semantic_poison_no_cascade
        "${CMAKE_SOURCE_DIR}/tests/invalid/semantic_poison_no_cascade.wio"
        "Unknown type: 'MissingType'"
        "Type mismatch in binary operation|Argument mismatch at index|No matching overload|Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_overload_poison_no_cascade
        "${CMAKE_SOURCE_DIR}/tests/invalid/overload_poison_no_cascade.wio"
        "Undefined symbol: 'missingArgument'"
        "No matching overload|Ambiguous function call|Called expression is undefined|Called expression is not a function|Generated C\\+\\+ output:"
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
        wio_test_realm_object_method_calls_run
        "${CMAKE_SOURCE_DIR}/tests/realm_object_method_calls_run.wio"
        "Realm object method sum: 7"
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
        wio_test_object_self_return_run
        "${CMAKE_SOURCE_DIR}/tests/object_self_return_run.wio"
        "Object self return: ok"
        --run
    )

    add_wio_output_test(
        wio_test_component_extension_run
        "${CMAKE_SOURCE_DIR}/tests/component_extension_run.wio"
        "Component extension: ok"
        --run
    )

    add_wio_output_test(
        wio_test_ref_view_lifetime_matrix_run
        "${CMAKE_SOURCE_DIR}/tests/ref_view_lifetime_matrix_run.wio"
        "ref-view-lifetime-matrix-ok"
        --run
    )

    add_wio_output_test(
        wio_test_numeric_promotion_run
        "${CMAKE_SOURCE_DIR}/tests/numeric_promotion_run.wio"
        "numeric-promotion-ok"
        --run
    )

    add_wio_output_test(
        wio_test_integer_literal_abi_run
        "${CMAKE_SOURCE_DIR}/tests/integer_literal_abi_run.wio"
        "integer-literal-abi-ok"
        --run
    )

    add_wio_output_test(
        wio_test_nullable_targets_run
        "${CMAKE_SOURCE_DIR}/tests/nullable_targets_run.wio"
        "nullable-targets-ok"
        --run
    )

    add_wio_output_test(
        wio_test_parser_precedence_run
        "${CMAKE_SOURCE_DIR}/tests/parser_precedence_run.wio"
        "parser-precedence-ok"
        --run
    )

    add_wio_output_test(
        wio_test_contextual_keyword_identifiers_run
        "${CMAKE_SOURCE_DIR}/tests/contextual_keyword_identifiers_run.wio"
        "Contextual keyword identifiers: ok"
        --run
    )

    add_wio_output_test(
        wio_test_default_initialized_variables_run
        "${CMAKE_SOURCE_DIR}/tests/default_initialized_variables_run.wio"
        "Default initialized variables: ok"
        --run
    )

    add_wio_output_test(
        wio_test_conditional_operator_run
        "${CMAKE_SOURCE_DIR}/tests/conditional_operator_run.wio"
        "Conditional operator: ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_foundation_extensions_run
        "${CMAKE_SOURCE_DIR}/tests/std_foundation_extensions_run.wio"
        "STD foundation extensions: ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_utility_stream_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_utility_stream_wave_run.wio"
        "STD utility stream wave: ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_json_packed_semver_run
        "${CMAKE_SOURCE_DIR}/tests/std_json_packed_semver_run.wio"
        "JSON \\+ packed semver: ok"
        --run
    )

    add_wio_output_test(
        wio_test_interpolated_string_nested_run
        "${CMAKE_SOURCE_DIR}/tests/interpolated_string_nested_run.wio"
        "Interpolated string nested: ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_option_result_consistency_run
        "${CMAKE_SOURCE_DIR}/tests/std_option_result_consistency_run.wio"
        "STD option/result consistency: ok"
        --run
    )

    add_wio_output_test(
        wio_test_lifecycle_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/lifecycle_semantics_run.wio"
        "lifecycle-semantics-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_resource_owned_run
        "${CMAKE_SOURCE_DIR}/tests/std_resource_owned_run.wio"
        "std-resource-owned-ok"
        --run
    )

    add_wio_output_test(
        wio_test_native_resource_owned_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_resource_owned_run.wio"
        "native-resource-owned-ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_dict_option_intrinsic_run
        "${CMAKE_SOURCE_DIR}/tests/dict_option_intrinsic_run.wio"
        "dict-option-intrinsic-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_option_adoption_run
        "${CMAKE_SOURCE_DIR}/tests/std_option_adoption_run.wio"
        "std-option-adoption-ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_object_explicit_specialization_run
        "${CMAKE_SOURCE_DIR}/tests/generic_object_explicit_specialization_run.wio"
        "Generic object specialization: ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_specialization_visibility_modules_run
        "${CMAKE_SOURCE_DIR}/tests/modules/generic_specialization_visibility_root.wio"
        "Generic specialization modules: ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_default_type_arguments_run
        "${CMAKE_SOURCE_DIR}/tests/generic_default_type_arguments_run.wio"
        "Generic defaults: ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_partial_specialization_run
        "${CMAKE_SOURCE_DIR}/tests/generic_partial_specialization_run.wio"
        "Generic partial specialization: ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_partial_specialization_ordering_run
        "${CMAKE_SOURCE_DIR}/tests/generic_partial_specialization_ordering_run.wio"
        "generic-partial-ordering-ok"
        --run
    )
    set_tests_properties(
        wio_test_generic_partial_specialization_ordering_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_generic_partial_specialization_structural_ambiguous
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_partial_specialization_structural_ambiguous.wio"
        "Ambiguous partial specialization"
    )
    set_tests_properties(
        wio_invalid_generic_partial_specialization_structural_ambiguous
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_extension_default_parameters_run
        "${CMAKE_SOURCE_DIR}/tests/extension_default_parameters_run.wio"
        "extension-default-parameters-ok"
        --run
    )
    set_tests_properties(
        wio_test_extension_default_parameters_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_match_enum_exhaustive_guards_run
        "${CMAKE_SOURCE_DIR}/tests/match_enum_exhaustive_guards_run.wio"
        "match-enum-exhaustive-guards-ok"
        --run
    )
    set_tests_properties(
        wio_test_match_enum_exhaustive_guards_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_extension_generic_methods_run
        "${CMAKE_SOURCE_DIR}/tests/extension_generic_methods_run.wio"
        "extension-generic-methods-ok"
        --run
    )
    set_tests_properties(
        wio_test_extension_generic_methods_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_extension_generic_constraint_reject
        "${CMAKE_SOURCE_DIR}/tests/invalid/extension_generic_constraint_reject.wio"
        "rejects type arguments"
    )
    set_tests_properties(
        wio_invalid_extension_generic_constraint_reject
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_match_enum_non_exhaustive
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_enum_non_exhaustive.wio"
        "must cover every member or include an 'assumed' fallback"
    )
    set_tests_properties(
        wio_invalid_match_enum_non_exhaustive
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_match_enum_duplicate_unguarded
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_enum_duplicate_unguarded.wio"
        "Unreachable enum match case"
    )
    set_tests_properties(
        wio_invalid_match_enum_duplicate_unguarded
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_match_literal_guard_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_literal_guard_type_mismatch.wio"
        "Match guards must have type 'bool'"
    )
    set_tests_properties(
        wio_invalid_match_literal_guard_type_mismatch
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_match_assumed_guard
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_assumed_guard.wio"
        "The final 'assumed' match case cannot have a guard"
    )
    set_tests_properties(
        wio_invalid_match_assumed_guard
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_generic_const_parameters_run
        "${CMAKE_SOURCE_DIR}/tests/generic_const_parameters_run.wio"
        "Const generics: ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_where_constraints_run
        "${CMAKE_SOURCE_DIR}/tests/generic_where_constraints_run.wio"
        "Generic where constraints: ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_native_default_instantiation_run
        "${CMAKE_SOURCE_DIR}/tests/native/generic_native_default_instantiation_run.wio"
        "Generic native defaults: ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_std_meta_pack_algorithms_run
        "${CMAKE_SOURCE_DIR}/tests/std_meta_pack_algorithms_run.wio"
        "Std meta pack algorithms: ok"
        --run
    )

    add_wio_failure_test(
        wio_invalid_generic_where_constraint_reject
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_where_constraint_reject.wio"
        "rejects type arguments <string> because of @Apply constraints"
    )

    add_wio_failure_test(
        wio_invalid_generic_where_unknown_parameter
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_where_unknown_parameter.wio"
        "Where clause references an unknown generic parameter 'U'"
    )

    add_wio_failure_test(
        wio_invalid_generic_where_conjunction_reject
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_where_conjunction_reject.wio"
        "rejects type arguments <u32> because of @Apply constraints"
    )

    add_wio_failure_test(
        wio_invalid_generic_invariance_reject
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_invariance_reject.wio"
        "Argument mismatch at index 0: Expected 'Holder<Animal>', but got 'Holder<Dog>'"
    )

    add_wio_failure_test(
        wio_invalid_generic_partial_specialization_ambiguous
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_partial_specialization_ambiguous.wio"
        "Ambiguous partial specialization for 'Choice\\(i32, string\\)'"
    )

    add_wio_failure_test(
        wio_invalid_generic_default_non_trailing
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_default_non_trailing.wio"
        "Generic parameters without defaults cannot follow a defaulted parameter"
    )

    add_wio_failure_test(
        wio_invalid_generic_default_forward_reference
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_default_forward_reference.wio"
        "defaults may only use earlier parameters"
    )

    add_wio_failure_test(
        wio_invalid_generic_const_argument_kind_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_argument_kind_mismatch.wio"
        "expects a compile-time integer value"
    )

    add_wio_failure_test(
        wio_invalid_generic_const_function_argument_kind_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_function_argument_kind_mismatch.wio"
        "expects a compile-time integer value"
    )

    add_wio_failure_test(
        wio_invalid_generic_const_parameter_requires_integer
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_parameter_requires_integer.wio"
        "must use an integer type"
    )

    add_wio_failure_test(
        wio_invalid_generic_const_default_forward_reference
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_default_forward_reference.wio"
        "defaults may only use earlier parameters"
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
        wio_test_native_cstring_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_cstring_bridge.wio"
        "Native cstring bridge: 3, 3, true"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_native_opaque_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_opaque_bridge.wio"
        "Opaque bridge: true, true, true, false, 6, 6, true"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_native_ref_params_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_ref_params_bridge.wio"
        "Native refs: 12, 13, Hi!, true, 3, 10"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_native_any_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_any_bridge.wio"
        "native-any-ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_native_box_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_box_bridge.wio"
        "native-box-ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
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
        wio_test_native_object_component_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_object_component_bridge.wio"
        "Native struct interop: 15, 3, 4 / 1, 1, 7"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_struct_methods.cpp"
    )

    add_wio_output_test(
        wio_test_native_pod_component_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_pod_component_bridge.wio"
        "Native POD component: 25, 7, 4, 6, 7, 8"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_pod_components.cpp"
    )

    add_wio_output_test(
        wio_test_native_component_extension_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_component_extension_run.wio"
        "Native component extension: ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_pod_components.cpp"
    )

    add_wio_output_test(
        wio_test_native_generic_pod_component_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_pod_component_bridge.wio"
        "Generic native POD component: 7, 3, 5, 5, 4, 5"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_pod_components.cpp"
    )

    add_wio_output_test(
        wio_test_native_generic_pod_array_component_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_pod_array_component_bridge.wio"
        "Native generic POD array: 6, 2, 3, 1, 4, 5, 6"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --backend-arg
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_pod_array_components.cpp"
    )

    add_wio_output_test(
        wio_test_native_generic_specialized_component_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_generic_specialized_component_run.wio"
        "Native generic component specialization: ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_native_enum_flagset_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/native_enum_flagset_bridge.wio"
        "native-enum-flagset-ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_generic_native_predicate_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/generic_native_predicate_bridge.wio"
        "Generic native predicate: 14, 18"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_default_parameter_native_bridge_run
        "${CMAKE_SOURCE_DIR}/tests/native/default_parameter_native_bridge.wio"
        "Native default parameters: 12, 16"
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
        wio_test_unicode_text_export_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/unicode_text_export.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/unicode_text_export_host.cpp"
        "Text export: valid=true count=9"
    )
    set_tests_properties(
        wio_test_unicode_text_export_host_interop
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
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
        wio_test_sdk_module_stale_bindings_interop
        "${CMAKE_SOURCE_DIR}/tests/native/module_stale_bindings.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_module_stale_bindings_host.cpp"
        "SDK module stale: before=5 afterAdd=12 stale=5 restarted=5"
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
        "SDK complex fields: fields=10 title=boss tags=2 hp=30 callback=21 mode=0 profile=77 position=11 fixedSum=24"
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
        wio_test_sdk_hot_reload_stale_wrappers_interop
        "${CMAKE_SOURCE_DIR}/tests/native/exported_types_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/sdk_hot_reload_stale_wrappers_host.cpp"
        "SDK reload stale wrappers: start=7 afterAdd=12 hp=100 stale=6 fresh=17 freshHp=100 generation=2"
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
        wio_test_const_scalar_expressions_run
        "${CMAKE_SOURCE_DIR}/tests/const_scalar_expressions.wio"
        "Const scalar: 11, 12, true, x"
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
        wio_test_numeric_literal_signed_mins_run
        "${CMAKE_SOURCE_DIR}/tests/numeric_literal_signed_mins_run.wio"
        "Signed literal mins: ok"
        --run
    )

    add_wio_output_test(
        wio_test_default_parameters_run
        "${CMAKE_SOURCE_DIR}/tests/default_parameters_run.wio"
        "Default parameters: ok"
        --run
    )

    add_wio_output_test(
        wio_test_function_parameter_pack_run
        "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_run.wio"
        "Function packs: ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_function_parameter_pack_explicit_type_arguments_run
        "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_explicit_type_arguments_run.wio"
        "Function packs explicit args: ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_function_parameter_pack_constraints_run
        "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_constraints_run.wio"
        "Function pack constraints: ok"
        --run
    )

    add_wio_output_test(
        wio_test_function_parameter_pack_defaults_run
        "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_defaults_run.wio"
        "Function pack defaults: ok"
        --run
    )

    add_wio_output_test(
        wio_test_pack_storage_write_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/pack_storage_write_semantics_run.wio"
        "Pack storage write semantics: ok"
        --run
    )

    add_wio_output_test(
        wio_test_pack_meta_and_generic_declarations_run
        "${CMAKE_SOURCE_DIR}/tests/pack_meta_and_generic_declarations_run.wio"
        "Pack meta \\+ generic declarations: ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_meta_second_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_meta_second_wave_run.wio"
        "Std meta second wave: ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_meta_wave3_run
        "${CMAKE_SOURCE_DIR}/tests/std_meta_wave3_run.wio"
        "Std meta wave 3: ok"
        --run
    )

    add_wio_shared_host_interop_test(
        wio_test_pack_export_host_interop
        "${CMAKE_SOURCE_DIR}/tests/native/pack_exported_library.wio"
        "${CMAKE_SOURCE_DIR}/tests/native/pack_export_host.cpp"
        "Pack export instantiate: exports=2 pair=2 triple=3"
    )

    add_wio_output_test(
        wio_test_std_console_direct_run
        "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
        "Std direct: hello\nStd direct value: 7"
        --run
    )

    add_wio_output_test(
        wio_test_entry_default_void_run
        "${CMAKE_SOURCE_DIR}/tests/entry_default_void_run.wio"
        "Entry default void works"
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
        wio_test_backend_info_resolves_paths
        "${CMAKE_SOURCE_DIR}/tests/test1.wio"
        "User include dirs: .*${CMAKE_SOURCE_DIR}/tests/native"
        --show-backend-info
        --dry-run
        --include-dir
        tests/native
        --link-dir
        tests/native
        --backend-arg
        tests/native/module_lifecycle_host.cpp
    )

    add_wio_emit_cpp_file_test(
        wio_test_emit_cpp_generates_preamble
        "${CMAKE_SOURCE_DIR}/tests/test1.wio"
        "// Generated by the Wio compiler\\."
    )

    add_wio_emit_cpp_file_test(
        wio_test_native_pod_component_emits_alias
        "${CMAKE_SOURCE_DIR}/tests/native/native_pod_component_bridge.wio"
        "using [^\\n]* = vec2;"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_emit_cpp_file_test(
        wio_test_native_pod_component_ref_dispatch_uses_direct_alias
        "${CMAKE_SOURCE_DIR}/tests/native/native_pod_component_bridge.wio"
        "native_pod::Translate\\(\\*value, dx, dy\\);"
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
        "Dict members: true/true/7/true/-1/true/7/11/4/true/true/true/true/5/true/4/1/10/5/50/3/5/true"
        --run
    )

    add_wio_output_test(
        wio_test_string_member_methods_run
        "${CMAKE_SOURCE_DIR}/tests/string_member_methods.wio"
        "String members: 9/true/true/true/true/4/8/W/a/\\?/Arena/Arena/Wio/Arena/Wio/Arena/wio arena/WIO ARENA/Wio World/Wio wio/xoxoxo/2/Arena/3/c/007/700/cba/!DLROWOIW/hi/true"
        --run
    )

    add_wio_output_test(
        wio_test_direct_indexing_run
        "${CMAKE_SOURCE_DIR}/tests/direct_indexing_run.wio"
        "Direct indexing: ok"
        --run
    )

    add_wio_output_test(
        wio_test_fixed_array_auto_extent_run
        "${CMAKE_SOURCE_DIR}/tests/fixed_array_auto_extent_run.wio"
        "fixed-array-auto-extent-ok"
        --run
    )
    set_tests_properties(
        wio_test_fixed_array_auto_extent_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_ref_indexed_element_run
        "${CMAKE_SOURCE_DIR}/tests/ref_indexed_element_run.wio"
        "Ref indexed element: ok"
        --run
    )

    add_wio_output_test(
        wio_test_control_flow_return_analysis_run
        "${CMAKE_SOURCE_DIR}/tests/control_flow_return_analysis_run.wio"
        "Control-flow returns: ok"
        --run
    )

    add_wio_output_test(
        wio_test_else_if_chain_run
        "${CMAKE_SOURCE_DIR}/tests/else_if_chain_run.wio"
        "Else-if chain: 2"
        --run
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_direct_index_out_of_range
        "${CMAKE_SOURCE_DIR}/tests/direct_index_out_of_range_runtime_failure.wio"
        "Runtime Error: Index access index 3 is out of range for size 3\\."
        "stl_vector|Assertion '__n < this->size\\(\\)' failed"
        --run
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_string_to_i32_invalid
        "${CMAKE_SOURCE_DIR}/tests/string_to_i32_runtime_failure.wio"
        "Runtime Error: Cannot parse 'not-a-number' as i32: invalid format"
        "segmentation fault|Assertion|std::invalid_argument"
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
        wio_test_std_convert_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_convert_surface_run.wio"
        "Std convert: -2147483648/42/42/3\\.5/true/true/65535/true/true/true/42/false/wio/BEEF/1010/true/true/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_text_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_text_surface_run.wio"
        "Std text: alpha\\|beta\\|gamma/alpha beta gamma/true/true/true/-1/2/convert/report\\.txt/a/b/c\\.txt/Wio/abc\\.\\.\\./true/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_path_fs_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_path_fs_surface_run.wio"
        "Std path/fs surface: true/true/true/3/gamma/true/true/logs/final\\.log/final\\.log/2/logs/true/alpha\\|beta\\|gamma/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_fs_recursive_run
        "${CMAKE_SOURCE_DIR}/tests/std_fs_recursive_run.wio"
        "Std fs recursive: true/1/true/true/true/true"
        --run
    )

    add_test(
        NAME wio_test_cli_path_dispatch_console_output
        COMMAND
            ${CMAKE_COMMAND}
            -DWIO_EXE=$<TARGET_FILE:wio_app>
            -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/entry_default_void_run.wio
            -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/cli_path_dispatch_console_output
            -P "${CMAKE_SOURCE_DIR}/cmake/RunWioCliPathDispatchConsoleSmoke.cmake"
    )
    set_tests_properties(
        wio_test_cli_path_dispatch_console_output
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_fs_executable_run
        "${CMAKE_SOURCE_DIR}/tests/std_fs_executable_run.wio"
        "Std fs executable: true/true/true/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_environment_statistics_run
        "${CMAKE_SOURCE_DIR}/tests/std_environment_statistics_run.wio"
        "Std environment/statistics: true/true/true/true/true/true/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_platform_run
        "${CMAKE_SOURCE_DIR}/tests/std_platform_run.wio"
        "Std platform: true/true/true/true/true"
        --run
    )

    add_wio_output_test(
        wio_test_std_console_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_console_surface_run.wio"
        "Std console surface: 1,2,3 xxx a/b"
        --run
    )

    add_wio_output_test(
        wio_test_std_console_native_types_run
        "${CMAKE_SOURCE_DIR}/tests/std_console_native_types_run.wio"
        "IO std-console-native-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_vector_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_vector_surface_run.wio"
        "std-vector-surface-ok"
        --run
    )

    add_wio_output_test(
        wio_test_const_enum_flagset_reflection_run
        "${CMAKE_SOURCE_DIR}/tests/const_enum_flagset_reflection_run.wio"
        "enum-reflection-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_enum_flagset_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_enum_flagset_surface_run.wio"
        "enum-flagset-surface-ok"
        --run
    )

    add_wio_output_test(
        wio_test_enum_member_scope_run
        "${CMAKE_SOURCE_DIR}/tests/enum_member_scope_run.wio"
        "enum-member-scope-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_io_surface_run
        "${CMAKE_SOURCE_DIR}/tests/std_io_surface_run.wio"
        "Std io surface: 2/3/3/8/AB/CD/read\\|share_read\\|sequential/none"
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
        wio_test_std_algorithms_utility_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_algorithms_utility_wave_run.wio"
        "Std algorithms utility: 4/4/3/3/4/3/2/2/4/15/3/1/true/3/5/true"
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
        wio_test_std_hash_random_time_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_hash_random_time_wave_run.wio"
        "std-hash-random-time-wave-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_containers_buffer_span_sort_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_containers_buffer_span_sort_wave_run.wio"
        "std-containers-buffer-span-sort-wave-ok"
        --run
    )

    add_wio_emit_cpp_file_test(
        wio_test_exported_nullable_descriptor
        "${CMAKE_SOURCE_DIR}/tests/native/exported_nullable_descriptor_library.wio"
        "WIO_MODULE_TYPE_DESC_NULLABLE"
        --target
        shared
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_native_exception_boundary
        "${CMAKE_SOURCE_DIR}/tests/native/native_exception_boundary.wio"
        "Runtime Error: Native call 'native_exception_boundary::ThrowStandardException' failed: native-boom"
        "terminate called|std::runtime_error|Unhandled native exception"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_panic_runs_cleanup
        "${CMAKE_SOURCE_DIR}/tests/panic_cleanup_runtime_failure.wio"
        "panic-cleanup-ran"
        "terminate called|Aborted|segmentation fault"
        --run
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_std_resource_use_after_dispose
        "${CMAKE_SOURCE_DIR}/tests/std_resource_use_after_dispose_runtime_failure.wio"
        "Runtime Error: Assertion failed: resource::Owned<T>.Value cannot access a disposed resource"
        "terminate called|segmentation fault"
        --run
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_integer_divide_by_zero
        "${CMAKE_SOURCE_DIR}/tests/integer_divide_by_zero_runtime_failure.wio"
        "Runtime Error: Integer division by zero\."
        "floating point exception|SIGFPE|Assertion"
        --run
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_enum_from_value_unknown
        "${CMAKE_SOURCE_DIR}/tests/enum_from_value_runtime_failure.wio"
        "Runtime Error: Enum value is outside the declared value set\."
        "segmentation fault|Assertion|SIG"
        --run
    )

    add_wio_output_test(
        wio_test_std_container_contracts_run
        "${CMAKE_SOURCE_DIR}/tests/std_container_contracts_run.wio"
        "std-container-contracts-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_numeric_integer_matrix_run
        "${CMAKE_SOURCE_DIR}/tests/std_numeric_integer_matrix_run.wio"
        "std-numeric-integer-matrix-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_path_fs_contract_run
        "${CMAKE_SOURCE_DIR}/tests/std_path_fs_contract_run.wio"
        "std-path-fs-contract-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_regex_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_regex_wave_run.wio"
        "std-regex-wave-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_traits_reflection_wave_run
        "${CMAKE_SOURCE_DIR}/tests/std_traits_reflection_wave_run.wio"
        "std-traits-reflection-wave-ok"
        --run
    )

    add_wio_non_dry_failure_test_clean(
        wio_runtime_std_assert_not_implemented_yet
        "${CMAKE_SOURCE_DIR}/tests/std_assert_not_implemented_yet_runtime_failure.wio"
        "Runtime Error: Assertion failed: Not implemented yet: vector swizzle assignment"
        "Unhandled exception|segmentation fault|Assertion '__n < this->size"
        --run
    )

    add_wio_output_test(
        wio_test_std_contract_v1_run
        "${CMAKE_SOURCE_DIR}/tests/std_contract_v1_run.wio"
        "Std contract v1: true/5/9/surface\\.txt/WIO-STD-V1/true"
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
        wio_test_generic_apply_function_constraints_run
        "${CMAKE_SOURCE_DIR}/tests/generic_apply_function_constraints.wio"
        "Generic apply fn: 42, wio"
        --run
    )

    add_wio_output_test(
        wio_test_generic_apply_list_constraints_run
        "${CMAKE_SOURCE_DIR}/tests/generic_apply_list_constraints.wio"
        "Generic apply list: true, true"
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
        wio_test_type_alias_constructor_run
        "${CMAKE_SOURCE_DIR}/tests/type_alias_constructor_run.wio"
        "type-alias-ctor-ok"
        --run
    )

    add_wio_output_test(
        wio_test_operator_overloading_run
        "${CMAKE_SOURCE_DIR}/tests/operator_overloading_run.wio"
        "operator-overload-ok"
        --run
    )

    add_wio_output_test(
        wio_test_operator_overloading_advanced_run
        "${CMAKE_SOURCE_DIR}/tests/operator_overloading_advanced_run.wio"
        "operator-overload-advanced-ok"
        --run
    )

    add_wio_output_test(
        wio_test_read_reference_value_flow_run
        "${CMAKE_SOURCE_DIR}/tests/read_reference_value_flow_run.wio"
        "read-reference-flow-ok"
        --run
    )

    add_wio_output_test(
        wio_test_operator_conversion_subscript_run
        "${CMAKE_SOURCE_DIR}/tests/operator_conversion_subscript_run.wio"
        "operator-conversion-subscript-ok"
        --run
    )

    add_wio_output_test(
        wio_test_operator_call_run
        "${CMAKE_SOURCE_DIR}/tests/operator_call_run.wio"
        "operator-call-ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_fit_target_run
        "${CMAKE_SOURCE_DIR}/tests/generic_fit_target_run.wio"
        "generic-fit-target-ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_operator_body_validation_run
        "${CMAKE_SOURCE_DIR}/tests/generic_operator_body_validation_run.wio"
        "generic-operator-body-validation-ok"
        --run
    )

    add_wio_output_test(
        wio_test_generic_apply_type_and_object_run
        "${CMAKE_SOURCE_DIR}/tests/generic_apply_type_and_object_run.wio"
        "Generic apply types: 3, 1.5"
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

    add_wio_success_test(
        wio_test_generic_fit_target_constraints_dry_run
        "${CMAKE_SOURCE_DIR}/tests/generic_fit_target_constraints_run.wio"
    )

    add_wio_success_test(
        wio_test_generic_fit_target_specialization_dry_run
        "${CMAKE_SOURCE_DIR}/tests/generic_fit_target_specialization_run.wio"
    )

    add_wio_success_test(
        wio_test_generic_parameterized_type_parameter_dry_run
        "${CMAKE_SOURCE_DIR}/tests/generic_parameterized_type_parameter_run.wio"
    )

    add_wio_success_test(
        wio_test_generic_parameterized_type_parameter_realm_dry_run
        "${CMAKE_SOURCE_DIR}/tests/generic_parameterized_type_parameter_realm_run.wio"
    )

    add_wio_success_test(
        wio_test_ref_mut_container_members_dry_run
        "${CMAKE_SOURCE_DIR}/tests/ref_mut_container_members_run.wio"
    )

    add_wio_success_test(
        wio_test_ref_mut_container_field_members_dry_run
        "${CMAKE_SOURCE_DIR}/tests/ref_mut_container_field_members_run.wio"
    )

    add_wio_success_test(
        wio_test_object_method_outer_const_dry_run
        "${CMAKE_SOURCE_DIR}/tests/object_method_outer_const_run.wio"
    )

    add_wio_success_test(
        wio_test_object_method_outer_const_realm_dry_run
        "${CMAKE_SOURCE_DIR}/tests/object_method_outer_const_realm_run.wio"
    )

    add_wio_success_test(
        wio_test_realm_object_method_calls_dry_run
        "${CMAKE_SOURCE_DIR}/tests/realm_object_method_calls_run.wio"
    )

    add_wio_success_test(
        wio_test_object_const_access_dry_run
        "${CMAKE_SOURCE_DIR}/tests/object_const_access_run.wio"
    )

    add_wio_success_test(
        wio_test_any_basic_boxing_dry_run
        "${CMAKE_SOURCE_DIR}/tests/any_basic_boxing_run.wio"
    )

    add_wio_success_test(
        wio_test_any_object_fit_dry_run
        "${CMAKE_SOURCE_DIR}/tests/any_object_fit_run.wio"
    )

    add_wio_success_test(
        wio_test_any_interface_boxing_dry_run
        "${CMAKE_SOURCE_DIR}/tests/any_interface_boxing_run.wio"
    )

    add_wio_success_test(
        wio_test_any_generic_box_fit_dry_run
        "${CMAKE_SOURCE_DIR}/tests/any_generic_box_fit_run.wio"
    )

    add_wio_success_test(
        wio_test_any_opaque_boundary_dry_run
        "${CMAKE_SOURCE_DIR}/tests/any_opaque_boundary_run.wio"
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_success_test(
        wio_test_std_heap_box_dry_run
        "${CMAKE_SOURCE_DIR}/tests/std_heap_box_run.wio"
    )

    add_wio_success_test(
        wio_test_std_event_any_dry_run
        "${CMAKE_SOURCE_DIR}/tests/std_event_any_run.wio"
    )

    add_wio_success_test(
        wio_test_std_result_unwrap_sugar_dry_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_unwrap_sugar_run.wio"
    )

    add_wio_success_test(
        wio_test_std_result_propagate_sugar_dry_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_propagate_sugar_run.wio"
    )

    add_wio_success_test(
        wio_test_std_result_generic_helpers_dry_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_generic_helpers_run.wio"
    )

    add_wio_success_test(
        wio_test_std_result_explicit_generic_sugar_dry_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_explicit_generic_sugar_run.wio"
    )

    add_wio_output_test(
        wio_test_object_const_access_run
        "${CMAKE_SOURCE_DIR}/tests/object_const_access_run.wio"
        "27"
        --run
    )

    add_wio_output_test(
        wio_test_any_basic_boxing_run
        "${CMAKE_SOURCE_DIR}/tests/any_basic_boxing_run.wio"
        "any-basic-ok"
        --run
    )

    add_wio_output_test(
        wio_test_any_object_fit_run
        "${CMAKE_SOURCE_DIR}/tests/any_object_fit_run.wio"
        "any-object-ok"
        --run
    )

    add_wio_output_test(
        wio_test_any_interface_fit_run
        "${CMAKE_SOURCE_DIR}/tests/any_interface_fit_run.wio"
        "any-interface-ok"
        --run
    )

    add_wio_output_test(
        wio_test_any_interface_boxing_run
        "${CMAKE_SOURCE_DIR}/tests/any_interface_boxing_run.wio"
        "any-interface-boxing-ok"
        --run
    )

    add_wio_output_test(
        wio_test_any_generic_box_fit_run
        "${CMAKE_SOURCE_DIR}/tests/any_generic_box_fit_run.wio"
        "any-generic-box-ok"
        --run
    )

    add_wio_output_test(
        wio_test_any_opaque_boundary_run
        "${CMAKE_SOURCE_DIR}/tests/any_opaque_boundary_run.wio"
        "any-opaque-boundary-ok"
        --run
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_output_test(
        wio_test_std_heap_box_run
        "${CMAKE_SOURCE_DIR}/tests/std_heap_box_run.wio"
        "std-heap-box-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_heap_box_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/std_heap_box_semantics_run.wio"
        "std-heap-box-semantics-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_event_any_run
        "${CMAKE_SOURCE_DIR}/tests/std_event_any_run.wio"
        "std-event-any-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_result_console_io_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_console_io_run.wio"
        "std-result-console-io-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_result_unwrap_sugar_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_unwrap_sugar_run.wio"
        "std-result-unwrap-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_result_propagate_sugar_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_propagate_sugar_run.wio"
        "std-result-propagate-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_result_generic_helpers_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_generic_helpers_run.wio"
        "std-result-generic-helpers-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_result_combinators_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_combinators_run.wio"
        "std-result-combinators-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_result_explicit_generic_sugar_run
        "${CMAKE_SOURCE_DIR}/tests/std_result_explicit_generic_sugar_run.wio"
        "std-result-explicit-generic-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_unit_result_alias_run
        "${CMAKE_SOURCE_DIR}/tests/std_unit_result_alias_run.wio"
        "std-unit-result-alias-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_unit_result_alias_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_attribute_with_using_native_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_with_using_native_run.wio"
        "attribute-with-using-native-ok"
        --run
    )
    set_tests_properties(
        wio_test_attribute_with_using_native_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_attribute_declaration_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_declaration_run.wio"
        "attribute-declaration-ok"
        --run
    )
    set_tests_properties(
        wio_test_attribute_declaration_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_attribute_compact_declaration_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_compact_declaration_run.wio"
        "attribute-compact-declaration-ok"
        --run
    )
    set_tests_properties(
        wio_test_attribute_compact_declaration_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_attribute_named_unknown
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_unknown.wio"
        "has no parameter named 'verb'"
    )
    add_wio_failure_test(
        wio_invalid_attribute_named_duplicate
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_duplicate.wio"
        "parameter 'method' is assigned more than once"
    )
    add_wio_failure_test(
        wio_invalid_attribute_named_missing_required
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_missing_required.wio"
        "missing required argument 'method'"
    )
    add_wio_failure_test(
        wio_invalid_attribute_compact_conflict
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_compact_conflict.wio"
        "through conflict group 'serialization'"
    )
    set_tests_properties(
        wio_invalid_attribute_named_unknown
        wio_invalid_attribute_named_duplicate
        wio_invalid_attribute_named_missing_required
        wio_invalid_attribute_compact_conflict
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_attribute_conflict_valid_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_conflict_valid_run.wio"
        "attribute-conflict-ok"
        --run
    )

    add_wio_output_test(
        wio_test_attribute_runtime_reflection_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_runtime_reflection_run.wio"
        "attribute-runtime-reflection-ok"
        --run
    )

    add_wio_output_test(
        wio_test_attribute_scoped_activation_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_scoped_activation_run.wio"
        "attribute-scoped-activation-ok"
        --run
    )
    set_tests_properties(wio_test_attribute_scoped_activation_run PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")

    add_wio_output_test(
        wio_test_attribute_legacy_equivalence_run
        "${CMAKE_SOURCE_DIR}/tests/attribute_legacy_equivalence_run.wio"
        "attribute-legacy-equivalence-ok"
        --run
    )
    set_tests_properties(wio_test_attribute_legacy_equivalence_run PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")

    add_wio_failure_test(wio_invalid_attribute_using_requires_scoped
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_using_requires_scoped.wio"
        "cannot be activated with 'using'")
    add_wio_failure_test(wio_invalid_attribute_scoped_conflict
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_scoped_conflict.wio"
        "through conflict group 'serialization'")
    add_wio_failure_test(wio_invalid_attribute_with_empty
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_with_empty.wio"
        "Expected an attribute name after 'with'")
    add_wio_failure_test(wio_invalid_attribute_with_trailing_comma
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_with_trailing_comma.wio"
        "Expected an attribute name after ',' in a 'with' clause")
    add_wio_failure_test(wio_invalid_attribute_with_duplicate_clause
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_with_duplicate_clause.wio"
        "only one postfix 'with' clause")
    set_tests_properties(
        wio_invalid_attribute_using_requires_scoped
        wio_invalid_attribute_scoped_conflict
        wio_invalid_attribute_with_empty
        wio_invalid_attribute_with_trailing_comma
        wio_invalid_attribute_with_duplicate_clause
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_attribute_runtime_reflection_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_attribute_conflict_valid_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_attribute_conflict
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_conflict.wio"
        "through conflict group 'serialization'"
    )
    set_tests_properties(
        wio_invalid_attribute_conflict
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_attribute_wrong_target
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_wrong_target.wio"
        "cannot target 'component'"
    )
    set_tests_properties(
        wio_invalid_attribute_wrong_target
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_attribute_argument_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_argument_type.wio"
        "must be 'string'"
    )
    set_tests_properties(
        wio_invalid_attribute_argument_type
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_pipeline_operator_run
        "${CMAKE_SOURCE_DIR}/tests/pipeline_operator_run.wio"
        "pipeline-operator-ok"
        --run
    )

    add_wio_output_test(
        wio_test_pipeline_stabilization_run
        "${CMAKE_SOURCE_DIR}/tests/pipeline_stabilization_run.wio"
        "pipeline-stabilization-ok"
        --run
    )
    set_tests_properties(wio_test_pipeline_stabilization_run PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")

    add_wio_failure_test(
        wio_invalid_pipeline_requires_callable
        "${CMAKE_SOURCE_DIR}/tests/invalid/pipeline_requires_callable.wio"
        "Pipeline target is not callable"
    )
    set_tests_properties(wio_invalid_pipeline_requires_callable PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")
    set_tests_properties(
        wio_test_pipeline_operator_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_match_option_result_patterns_run
        "${CMAKE_SOURCE_DIR}/tests/match_option_result_patterns_run.wio"
        "match-option-result-patterns-ok"
        --run
    )

    add_wio_output_test(
        wio_test_match_array_patterns_run
        "${CMAKE_SOURCE_DIR}/tests/match_array_patterns_run.wio"
        "match-array-patterns-ok"
        --run
    )

    add_wio_output_test(
        wio_test_match_stabilization_run
        "${CMAKE_SOURCE_DIR}/tests/match_stabilization_run.wio"
        "match-stabilization-ok"
        --run
    )
    set_tests_properties(wio_test_match_stabilization_run PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")

    add_wio_failure_test(wio_invalid_match_guard_after_unguarded
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_guard_after_unguarded.wio"
        "earlier unguarded case already covers it")
    add_wio_failure_test(wio_invalid_match_array_guard_after_unguarded
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_array_guard_after_unguarded.wio"
        "Unreachable 'array-length' match pattern")
    add_wio_failure_test(wio_invalid_match_duplicate_binding
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_duplicate_binding.wio"
        "Pattern binding 'value' is duplicated")
    add_wio_failure_test(wio_invalid_match_binding_is_immutable
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_binding_is_immutable.wio"
        "immutable")
    set_tests_properties(
        wio_invalid_match_guard_after_unguarded
        wio_invalid_match_array_guard_after_unguarded
        wio_invalid_match_duplicate_binding
        wio_invalid_match_binding_is_immutable
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_match_array_patterns_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_match_array_pattern_wrong_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_array_pattern_wrong_type.wio"
        "Array destructuring patterns require an array value"
    )
    set_tests_properties(
        wio_invalid_match_array_pattern_wrong_type
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_match_option_result_patterns_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_match_pattern_wrong_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/match_pattern_wrong_type.wio"
        "requires a matching std::Option or std::Result"
    )
    set_tests_properties(
        wio_invalid_match_pattern_wrong_type
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_unicode_builders_run
        "${CMAKE_SOURCE_DIR}/tests/std_unicode_builders_run.wio"
        "std-unicode-builders-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_binary_serialization_run
        "${CMAKE_SOURCE_DIR}/tests/std_binary_serialization_run.wio"
        "std-binary-serialization-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_net_udp_loopback_run
        "${CMAKE_SOURCE_DIR}/tests/std_net_udp_loopback_run.wio"
        "std-net-udp-loopback-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_structured_log_run
        "${CMAKE_SOURCE_DIR}/tests/std_structured_log_run.wio"
        "std-structured-log-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_unicode_grapheme_run
        "${CMAKE_SOURCE_DIR}/tests/std_unicode_grapheme_run.wio"
        "std-unicode-grapheme-ok"
        --run
    )

    add_wio_output_test(
        wio_test_std_concurrency_future_run
        "${CMAKE_SOURCE_DIR}/tests/std_concurrency_future_run.wio"
        "std-concurrency-future-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_concurrency_future_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_std_unicode_grapheme_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_std_structured_log_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_std_net_udp_loopback_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_std_binary_serialization_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_std_unicode_builders_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_json_hardening_run
        "${CMAKE_SOURCE_DIR}/tests/std_json_hardening_run.wio"
        "std-json-hardening-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_json_hardening_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_concurrency_foundation_run
        "${CMAKE_SOURCE_DIR}/tests/std_concurrency_foundation_run.wio"
        "std-concurrency-foundation-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_concurrency_foundation_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_net_foundation_run
        "${CMAKE_SOURCE_DIR}/tests/std_net_foundation_run.wio"
        "std-net-foundation-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_net_foundation_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_os_facilities_run
        "${CMAKE_SOURCE_DIR}/tests/std_os_facilities_run.wio"
        "std-os-facilities-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_os_facilities_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_data_utilities_run
        "${CMAKE_SOURCE_DIR}/tests/std_data_utilities_run.wio"
        "std-data-utilities-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_data_utilities_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_identity_random_hardening_run
        "${CMAKE_SOURCE_DIR}/tests/std_identity_random_hardening_run.wio"
        "std-identity-random-hardening-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_identity_random_hardening_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_binary_bigint_compression_run
        "${CMAKE_SOURCE_DIR}/tests/std_binary_bigint_compression_run.wio"
        "std-binary-bigint-compression-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_binary_bigint_compression_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_application_lifecycle_run
        "${CMAKE_SOURCE_DIR}/tests/application_lifecycle_run.wio"
        "application-lifecycle-ok"
        --run
    )

    add_wio_output_test(
        wio_test_application_stabilization_run
        "${CMAKE_SOURCE_DIR}/tests/application_stabilization_run.wio"
        "application-stabilization-ok"
        --run
    )
    set_tests_properties(wio_test_application_stabilization_run PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")

    add_wio_failure_test(wio_invalid_application_with_entry
        "${CMAKE_SOURCE_DIR}/tests/invalid/application_with_entry.wio"
        "cannot define both application 'Tool' and an ordinary Entry")
    add_wio_failure_test(wio_invalid_application_multiple_roots
        "${CMAKE_SOURCE_DIR}/tests/invalid/application_multiple_roots.wio"
        "may declare only one application root")
    add_wio_failure_test(wio_invalid_application_nested_realm
        "${CMAKE_SOURCE_DIR}/tests/invalid/application_nested_realm.wio"
        "application root must be declared at module top level")
    add_wio_failure_test(wio_invalid_application_missing_update
        "${CMAKE_SOURCE_DIR}/tests/invalid/application_missing_update.wio"
        "must declare exactly one 'on update' handler")
    add_wio_failure_test(wio_invalid_application_duplicate_handler
        "${CMAKE_SOURCE_DIR}/tests/invalid/application_duplicate_handler.wio"
        "handler 'update' is declared more than once")
    add_wio_failure_test(wio_invalid_application_handler_parameters
        "${CMAKE_SOURCE_DIR}/tests/invalid/application_handler_parameters.wio"
        "supports parameterless lifecycle handlers")
    add_wio_failure_test(wio_invalid_system_duplicate_handler
        "${CMAKE_SOURCE_DIR}/tests/invalid/system_duplicate_handler.wio"
        "System handler 'update' is declared more than once")
    set_tests_properties(
        wio_invalid_application_with_entry
        wio_invalid_application_multiple_roots
        wio_invalid_application_nested_realm
        wio_invalid_application_missing_update
        wio_invalid_application_duplicate_handler
        wio_invalid_application_handler_parameters
        wio_invalid_system_duplicate_handler
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_application_lifecycle_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_std_regex_time_hardening_run
        "${CMAKE_SOURCE_DIR}/tests/std_regex_time_hardening_run.wio"
        "std-regex-time-hardening-ok"
        --run
    )
    set_tests_properties(
        wio_test_std_regex_time_hardening_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_any_value_reference_semantics_run
        "${CMAKE_SOURCE_DIR}/tests/any_value_reference_semantics_run.wio"
        "any-value-reference-semantics-ok"
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

    add_wio_failure_test_clean(
        wio_invalid_non_void_when_without_else
        "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_when_without_else.wio"
        "Functions with a return value must provide an 'else' fallback for 'when' guards"
        "Compiling failed:"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_non_void_missing_return
        "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_missing_return.wio"
        "Non-void function 'Broken' must return a value on all control-flow paths\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_non_void_if_missing_return
        "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_if_missing_return.wio"
        "Non-void function 'Broken' must return a value on all control-flow paths\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_array_index_requires_integer
        "${CMAKE_SOURCE_DIR}/tests/invalid/array_index_requires_integer.wio"
        "Array and string indices must be integer values\\."
        "stl_vector|Unhandled compiler failure"
    )

    add_wio_failure_test_clean(
        wio_invalid_static_array_index_out_of_range
        "${CMAKE_SOURCE_DIR}/tests/invalid/static_array_index_out_of_range.wio"
        "Index 2 is out of range for compile-time array size 2\\."
        "stl_vector|Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_generic_fit_target_without_supported_constraint
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_fit_target_without_supported_constraint.wio"
        "The 'fit' operator can only be used with numeric types or object/interface casts\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_any_arithmetic_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/any_arithmetic_not_supported.wio"
        "Any values support only assignment, the 'is' operator, the 'fit' operator, and equality comparisons with null\\. Operator '\\+' is not supported\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_any_non_null_equality_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/any_non_null_equality_not_supported.wio"
        "Any values support only assignment, the 'is' operator, the 'fit' operator, and equality comparisons with null\\. Operator '==' is not supported\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_operator_overload_invalid_arity
        "${CMAKE_SOURCE_DIR}/tests/invalid/operator_overload_invalid_arity.wio"
        "Invalid member operator overload arity for the selected operator\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_read_requires_reference
        "${CMAKE_SOURCE_DIR}/tests/invalid/read_requires_reference.wio"
        "The 'deref' operator requires a readable reference\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_operator_subscript_assignment_requires_ref
        "${CMAKE_SOURCE_DIR}/tests/invalid/operator_subscript_assignment_requires_ref.wio"
        "Subscript assignment requires operator '\\[\\]' to return a mutable reference\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_operator_call_requires_member
        "${CMAKE_SOURCE_DIR}/tests/invalid/operator_call_requires_member.wio"
        "Call operator overloads must be declared as member functions\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_opaque_is_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/opaque_is_not_supported.wio"
        "The left side of the 'is' operator must be an object or interface value/reference\\. Got 'opaque'\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_result_unwrap_requires_result
        "${CMAKE_SOURCE_DIR}/tests/invalid/result_unwrap_requires_result.wio"
        "The '!\\(\\)' unwrap syntax requires the called function to return std::Result<T>\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_result_propagate_requires_result_context
        "${CMAKE_SOURCE_DIR}/tests/invalid/result_propagate_requires_result_context.wio"
        "The '\\?\\(\\)' propagation syntax requires the enclosing function to return std::Result<T>\\."
        "Generated C\\+\\+ output:"
    )

    add_wio_failure_test_clean(
        wio_invalid_result_propagate_requires_result_call
        "${CMAKE_SOURCE_DIR}/tests/invalid/result_propagate_requires_result_call.wio"
        "The '\\?\\(\\)' propagation syntax requires the called function to return std::Result<T>\\."
        "Generated C\\+\\+ output:"
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
        wio_invalid_extension_mutable_receiver_requires_mut
        "${CMAKE_SOURCE_DIR}/tests/invalid/extension_mutable_receiver_requires_mut.wio"
        "Extension method 'AddOne' requires a mutable receiver"
    )

    add_wio_failure_test_clean(
        wio_invalid_return_local_component_reference
        "${CMAKE_SOURCE_DIR}/tests/invalid/return_local_component_reference.wio"
        "Cannot return a reference borrowed from a local value"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_return_local_array_reference
        "${CMAKE_SOURCE_DIR}/tests/invalid/return_local_array_reference.wio"
        "Cannot return a reference borrowed from a local value"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_temporary_object_view_escape
        "${CMAKE_SOURCE_DIR}/tests/invalid/temporary_object_view_escape.wio"
        "Cannot store a reference borrowed from a temporary value"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_native_reference_return_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_reference_return_not_supported.wio"
        "@Native functions cannot return ref/view values"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_floating_modulo_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/floating_modulo_not_supported.wio"
        "Operator '%' requires integer operands"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_null_primitive_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/null_primitive_not_supported.wio"
        "Type 'i32' cannot be initialized with null"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_null_component_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/null_component_not_supported.wio"
        "Type 'Point' cannot be initialized with null"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_null_array_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/null_array_not_supported.wio"
        "Type 'i32\\[\\]' cannot be initialized with null"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_null_inference_requires_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/null_inference_requires_type.wio"
        "Cannot infer a variable type from null"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_use_path_empty_segment
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_path_empty_segment.wio"
        "Expected a module name after '::'"
        "Compiling failed:"
    )

    add_wio_failure_test(
        wio_invalid_extension_cannot_access_private_member
        "${CMAKE_SOURCE_DIR}/tests/invalid/extension_cannot_access_private_member.wio"
        "Cannot access private member 'value'"
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
        "Cannot access private member 'secret' declared on 'PrivateBase' from 'PrivateDerived'"
    )

    add_wio_failure_test(
        wio_invalid_protected_member_access_outside_hierarchy
        "${CMAKE_SOURCE_DIR}/tests/invalid/protected_member_access_outside_hierarchy.wio"
        "Cannot access protected member 'hp' declared on 'GuardedBase' from '<non-object context>'"
    )

    add_wio_failure_test(
        wio_invalid_trust_requires_struct_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/trust_requires_struct_type.wio"
        "@Trust expects object/component/interface type names"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_entry_invalid_parameter_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/entry_invalid_parameter_type.wio"
        "Error \\[.*entry_invalid_parameter_type\\.wio:1:1\\]: Entry parameter must be string\\[\\] when present\\. Got 'i32'\\."
        "Compiling failed:"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_entry_invalid_return_type
        "${CMAKE_SOURCE_DIR}/tests/invalid/entry_invalid_return_type.wio"
        "Error \\[.*entry_invalid_return_type\\.wio:1:1\\]: Entry must return i32 or void\\."
        "Compiling failed:"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_entry_too_many_parameters
        "${CMAKE_SOURCE_DIR}/tests/invalid/entry_too_many_parameters.wio"
        "Error \\[.*entry_too_many_parameters\\.wio:1:1\\]: Entry must declare zero parameters or exactly one string\\[\\] parameter\\."
        "Compiling failed:"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_entry_missing_body
        "${CMAKE_SOURCE_DIR}/tests/invalid/entry_missing_body.wio"
        "Error \\[.*entry_missing_body\\.wio:1:1\\]: Entry functions must define a Wio body\\."
        "Compiling failed:"
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_entry_method_not_top_level
        "${CMAKE_SOURCE_DIR}/tests/invalid/entry_method_not_top_level.wio"
        "Error \\[.*entry_method_not_top_level\\.wio:2:5\\]: Entry is currently supported only for top-level functions\\."
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
        wio_invalid_use_alias_hides_unqualified_root_symbol
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_alias_hides_unqualified_root_symbol.wio"
        "Undefined symbol: 'GetValue'"
        --module-dir
        "${CMAKE_SOURCE_DIR}/tests/modules"
    )

    add_wio_failure_test(
        wio_invalid_use_without_direct_import_keeps_namespaced_access
        "${CMAKE_SOURCE_DIR}/tests/invalid/use_without_direct_import_keeps_namespaced_access.wio"
        "Undefined symbol: 'Abs'"
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
        wio_invalid_let_without_type_or_initializer
        "${CMAKE_SOURCE_DIR}/tests/invalid/let_without_type_or_initializer.wio"
        "A variable without an initializer must have an explicit type."
    )

    add_wio_failure_test(
        wio_invalid_conditional_operator_requires_bool
        "${CMAKE_SOURCE_DIR}/tests/invalid/conditional_operator_requires_bool.wio"
        "Conditional operator condition must be bool, but got 'i32'\\."
    )

    add_wio_failure_test(
        wio_invalid_conditional_operator_branch_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/conditional_operator_branch_type_mismatch.wio"
        "Conditional operator branches must have compatible types\\. Got 'i32' and 'string'\\."
    )

    add_wio_failure_test(
        wio_invalid_const_without_initializer
        "${CMAKE_SOURCE_DIR}/tests/invalid/const_without_initializer.wio"
        "Constants must be initialized."
    )

    add_wio_output_test(
        wio_test_const_string_text_run
        "${CMAKE_SOURCE_DIR}/tests/const_string_text_run.wio"
        "const-string-text-ok"
        --run
    )
    set_tests_properties(
        wio_test_const_string_text_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(
        wio_invalid_const_interpolated_text_not_folded
        "${CMAKE_SOURCE_DIR}/tests/invalid/const_interpolated_text_not_folded.wio"
        "Const initializer must be a compile-time expression and may reference only other const declarations."
    )

    add_wio_failure_test(
        wio_invalid_const_from_non_const_identifier
        "${CMAKE_SOURCE_DIR}/tests/invalid/const_from_non_const_identifier.wio"
        "Const initializer must be a compile-time expression and may reference only other const declarations."
    )

    add_wio_failure_test(
        wio_invalid_const_function_call_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/const_function_call_not_supported.wio"
        "Const initializer must be a compile-time expression and may reference only other const declarations."
    )

    add_wio_failure_test(
        wio_invalid_const_array_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/const_array_not_supported.wio"
        "Const declarations support scalar, string, text, enum, and flagset values. Got 'i32\\[\\]'."
    )

    add_wio_failure_test(
        wio_invalid_default_parameter_non_trailing
        "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_non_trailing.wio"
        "Parameters with default values must be trailing."
    )

    add_wio_failure_test(
        wio_invalid_default_parameter_type_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_type_mismatch.wio"
        "Default argument type mismatch for parameter 'x'. Expected 'i32', but got 'string'."
    )

    add_wio_failure_test(
        wio_invalid_default_parameter_declaration_only_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_declaration_only_not_supported.wio"
        "Default parameters are currently supported only on functions with Wio bodies or on @Native declarations."
    )

    add_wio_failure_test(
        wio_invalid_default_parameter_entry_not_allowed
        "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_entry_not_allowed.wio"
        "Entry cannot declare default parameters."
    )

    add_wio_failure_test(
        wio_invalid_default_parameter_overload_conflict
        "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_overload_conflict.wio"
        "Default parameters on 'Score' would synthesize an overload with 1 arguments, but that signature is already declared explicitly."
    )

    add_wio_failure_test(
        wio_invalid_pack_field_requires_generic_pack
        "${CMAKE_SOURCE_DIR}/tests/invalid/pack_field_requires_generic_pack.wio"
        "Pack fields must declare a trailing generic pack type such as 'pack values: Args\\.\\.\\.;'"
    )

    add_wio_failure_test(
        wio_invalid_pack_index_requires_compile_time_constant
        "${CMAKE_SOURCE_DIR}/tests/invalid/pack_index_requires_compile_time_constant.wio"
        "Pack indexing requires a non-negative compile-time integer index."
    )

    add_wio_failure_test(
        wio_invalid_pack_index_requires_const_declaration
        "${CMAKE_SOURCE_DIR}/tests/invalid/pack_index_requires_const_declaration.wio"
        "Pack type indexing requires a non-negative compile-time integer index."
    )

    add_wio_failure_test(
        wio_invalid_pack_index_from_size_requires_subtraction
        "${CMAKE_SOURCE_DIR}/tests/invalid/pack_index_from_size_requires_subtraction.wio"
        "Pack type indexing from '.size' must subtract at least 1."
    )

    add_wio_failure_test(
        wio_invalid_type_pack_to_static_array_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/type_pack_to_static_array_not_supported.wio"
        "Type packs cannot be converted to runtime static arrays."
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
        "Static array types must declare a size"
    )

    add_wio_failure_test(
        wio_invalid_fixed_array_auto_extent_without_initializer
        "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_without_initializer.wio"
        "Inferred static array extent '\\[T; _\\]' requires an initializer."
    )

    add_wio_failure_test(
        wio_invalid_fixed_array_auto_extent_parameter
        "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_parameter.wio"
        "Inferred static array extent '\\[T; _\\]' is supported only on variable declarations with an initializer."
    )

    add_wio_failure_test(
        wio_invalid_fixed_array_auto_extent_dynamic_source
        "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_dynamic_source.wio"
        "Inferred static array extent requires an array literal or concrete fixed-size array initializer."
    )

    add_wio_failure_test(
        wio_invalid_fixed_array_auto_extent_ragged_nested
        "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_ragged_nested.wio"
        "Array elements must be of the same type. Expected '\\[i32; 2\\]', but found '\\[i32; 1\\]'."
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
        "@Native methods are currently supported only on object methods and on object/component OnConstruct/OnDestruct lifecycle functions"
    )

    add_wio_failure_test(
        wio_invalid_native_component_non_pod_field_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_component_non_pod_field_not_supported.wio"
        "Declaration-level @Native component 'BadNativeComponent' field 'name' uses type 'string' which is not POD-native-compatible yet"
    )

    add_wio_failure_test(
        wio_invalid_native_component_dynamic_array_field_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_component_dynamic_array_field_not_supported.wio"
        "Declaration-level @Native component 'BadNativeArrayField' field 'values' uses type 'i32\\[\\]' which is not POD-native-compatible yet"
    )

    add_wio_failure_test(
        wio_invalid_native_generic_component_non_pod_specialization_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_generic_component_non_pod_specialization_not_supported.wio"
        "Declaration-level @Native component 'Pair<string>' field 'first' resolves to type 'string' which is not POD-native-compatible yet"
    )

    add_wio_failure_test(
        wio_invalid_native_object_declaration_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_object_declaration_not_supported.wio"
        "Declaration-level @Native POD interop is currently supported only on components"
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
        wio_invalid_numeric_literal_context_u8_out_of_range
        "${CMAKE_SOURCE_DIR}/tests/invalid/numeric_literal_context_u8_out_of_range.wio"
        "Integer literal '300' does not fit into expected type 'u8'"
    )

    add_wio_failure_test(
        wio_invalid_numeric_literal_context_f32_out_of_range
        "${CMAKE_SOURCE_DIR}/tests/invalid/numeric_literal_context_f32_out_of_range.wio"
        "Float literal '1\\.0e100' does not fit into expected type 'f32'"
    )

    add_wio_failure_test_clean(
        wio_invalid_null_object_requires_nullable
        "${CMAKE_SOURCE_DIR}/tests/invalid/null_object_requires_nullable.wio"
        "Type 'Service' cannot be initialized with null"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_nullable_member_requires_narrowing
        "${CMAKE_SOURCE_DIR}/tests/invalid/nullable_member_requires_narrowing.wio"
        "Member access requires a non-null value"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_duplicate_nullable_suffix
        "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_nullable_suffix.wio"
        "A type can only have one nullable suffix '\\?'"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_nullable_dynamic_array_handle
        "${CMAKE_SOURCE_DIR}/tests/invalid/nullable_dynamic_array_handle.wio"
        "Type 'i32\\[\\]' cannot be nullable"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_destructor_parameters_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/destructor_parameters_not_supported.wio"
        "OnDestruct must not declare parameters"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_lifecycle_return_must_be_void
        "${CMAKE_SOURCE_DIR}/tests/invalid/lifecycle_return_must_be_void.wio"
        "OnDestruct must return void"
        "Compiling failed:"
    )

    add_wio_failure_test_clean(
        wio_invalid_non_null_local_requires_initializer
        "${CMAKE_SOURCE_DIR}/tests/invalid/non_null_local_requires_initializer.wio"
        "Non-null type 'Service' requires an initializer"
        "Compiling failed:"
    )

    add_wio_failure_test(
        wio_invalid_implicit_narrowing_requires_fit
        "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_requires_fit.wio"
        "Implicit narrowing conversion from 'i32' to 'u8' requires explicit 'fit'"
    )

    add_wio_failure_test(
        wio_invalid_implicit_narrowing_assignment_requires_fit
        "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_assignment_requires_fit.wio"
        "Implicit narrowing conversion from 'i32' to 'u8' requires explicit 'fit'"
    )

    add_wio_failure_test(
        wio_invalid_implicit_narrowing_return_requires_fit
        "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_return_requires_fit.wio"
        "Implicit narrowing return conversion from 'i32' to 'u8' requires explicit 'fit'"
    )

    add_wio_failure_test(
        wio_invalid_implicit_narrowing_argument_requires_fit
        "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_argument_requires_fit.wio"
        "Implicit narrowing argument conversion from 'i32' to 'u8' at index 0 requires explicit 'fit'"
    )

    add_wio_backend_failure_test(
        wio_invalid_backend_cpp_line_mapping_native_name
        "${CMAKE_SOURCE_DIR}/tests/invalid/backend_cpp_line_mapping_native_name.wio"
        "Error \\[.*backend_cpp_line_mapping_native_name\\.wio:3:16\\]:"
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

    add_wio_non_dry_failure_test_clean(
        wio_invalid_emit_cpp_with_dry_run
        "${CMAKE_SOURCE_DIR}/tests/test1.wio"
        "--emit-cpp cannot be combined with --dry-run\\."
        "Unexpected Error:"
        --emit-cpp
        --dry-run
    )

    add_wio_non_dry_failure_test_clean(
        wio_invalid_emit_cpp_with_run
        "${CMAKE_SOURCE_DIR}/tests/test1.wio"
        "--emit-cpp cannot be combined with --run\\."
        "Unexpected Error:"
        --emit-cpp
        --run
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
        wio_invalid_opaque_arithmetic_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/opaque_arithmetic_not_supported.wio"
        "Opaque values support only assignment and equality comparisons\\. Operator '\\+' is not supported\\."
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
        "Ambiguous function call to 'Echo' with arguments \\(i32\\)\\. Candidates: Echo<T>\\(T\\) -> T; Echo<U>\\(U\\) -> U\\."
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
        "@Instantiate expects exactly 2 arguments\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_instantiate_non_concrete
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_non_concrete.wio"
        "@Instantiate must use fully concrete type constraints or supported predicates like std::traits::IsInteger<T>\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_instantiate_requires_native_or_export
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_requires_native_or_export.wio"
        "@Instantiate is currently supported only together with @Native or @Export\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_export_instantiate_non_abi_safe
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_instantiate_non_abi_safe.wio"
        "@Export instantiated with '<string>' produces a non-C-ABI-safe parameter 0 of type 'string'\\."
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
        "Only @Apply is currently supported on type aliases\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_apply_reject_function_call
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_reject_function_call.wio"
        "Generic callable 'Echo' rejects type arguments <string> because of @Apply constraints\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_apply_list_reject_function_call
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_list_reject_function_call.wio"
        "Generic callable 'Square' rejects type arguments <i32> because of @Apply constraints\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_apply_reject_type_alias
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_reject_type_alias.wio"
        "Generic type alias 'IntList' rejects type arguments <string> because of @Apply constraints\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_apply_reject_constructor
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_reject_constructor.wio"
        "Generic type 'NumberBox' rejects type arguments <string> because of @Apply constraints\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_apply_non_generic_function
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_non_generic_function.wio"
        "@Apply can only be used on generic functions\\."
    )

    add_wio_failure_test(
        wio_invalid_generic_parameter_pack_non_trailing
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_parameter_pack_non_trailing.wio"
        "Generic parameter packs must be trailing\\."
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_non_trailing
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_non_trailing.wio"
        "Function parameter packs must be trailing\\."
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_missing_matching_generic_pack
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_missing_matching_generic_pack.wio"
        "Function parameter packs require a matching trailing generic parameter pack\\."
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_bare_use_not_allowed
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_bare_use_not_allowed.wio"
        "Parameter pack 'args' must be expanded as 'args\\.\\.\\.' inside a function call\\."
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_default_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_default_not_supported.wio"
        "Parameter pack 'args' cannot declare a default value\\. Default values apply only to fixed parameters before the trailing pack\\."
        --target
        static
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_method_not_supported
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_method_not_supported.wio"
        "Generic parameter packs are currently supported only on top-level functions\\."
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_apply_reject_non_integer
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_apply_reject_non_integer.wio"
        "Generic callable 'CountIntegers' rejects type arguments <i32, f32> because of @Apply constraints\\."
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_export_requires_instantiate
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_export_requires_instantiate.wio"
        "Generic @Export pack functions must declare at least one concrete @Instantiate\\(\\.\\.\\.\\)\\."
        --target
        shared
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_instantiate_predicate_not_allowed
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_instantiate_predicate_not_allowed.wio"
        "@Instantiate on generic pack declarations must provide concrete pack element types for 'Args\\.\\.\\.'\\. Predicates belong in @Apply\\."
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
        --target
        static
    )

    add_wio_failure_test(
        wio_invalid_parameter_pack_view_assignment_not_allowed
        "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_view_assignment_not_allowed.wio"
        "Cannot assign through raw pack values or pack views\\. Assign to a mutable pack storage field instead\\."
        --target
        static
    )

    add_wio_failure_test(
        wio_invalid_generic_native_predicate_reject_non_integer
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_native_predicate_reject_non_integer.wio"
        "Generic interop function 'DoubleValue' does not declare @Instantiate<f32>\\."
        --include-dir
        "${CMAKE_SOURCE_DIR}/tests/native"
    )

    add_wio_failure_test(
        wio_invalid_generic_object_constructor_missing_arguments
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_object_constructor_missing_arguments.wio"
        "Cannot infer generic arguments for constructor 'Box'. Use explicit type arguments."
    )

    add_wio_failure_test(
        wio_invalid_generic_specialization_requires_primary
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_requires_primary.wio"
        "@Specialize object 'MissingPrimary' requires an earlier generic object declaration with the same name\."
    )

    add_wio_failure_test(
        wio_invalid_generic_specialization_argument_count_mismatch
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_argument_count_mismatch.wio"
        "@Specialize for 'Pair' expects 2 type arguments, but got 1\."
    )

    add_wio_failure_test(
        wio_invalid_generic_specialization_duplicate
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_duplicate.wio"
        "Duplicate explicit specialization for 'Box<i32>'\."
    )

    add_wio_failure_test(
        wio_invalid_generic_specialization_native_component
        "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_native_component.wio"
        "A declaration-level @Native specialization requires an @Native generic component primary"
    )

    add_wio_failure_test(
        wio_invalid_native_extension_requires_native_component
        "${CMAKE_SOURCE_DIR}/tests/invalid/native_extension_requires_native_component.wio"
        "@Native extension methods require a declaration-level @Native component target"
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

    add_wio_output_test(
        wio_test_async_coroutine_run
        "${CMAKE_SOURCE_DIR}/tests/async_coroutine_run.wio"
        "async-coroutine-ok"
        --run
    )
    set_tests_properties(wio_test_async_coroutine_run PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}")

    add_wio_output_test(
        wio_test_async_structured_run
        "${CMAKE_SOURCE_DIR}/tests/async_structured_run.wio"
        "async-structured-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_block_on_run
        "${CMAKE_SOURCE_DIR}/tests/async_block_on_run.wio"
        "async-block-on-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_freeze_run
        "${CMAKE_SOURCE_DIR}/tests/async_freeze_run.wio"
        "async-freeze-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_detached_exit_run
        "${CMAKE_SOURCE_DIR}/tests/async_detached_exit_run.wio"
        "async-detached-exit-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_worker_config_run
        "${CMAKE_SOURCE_DIR}/tests/async_worker_config_run.wio"
        "async-worker-config-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_correctness_run
        "${CMAKE_SOURCE_DIR}/tests/async_correctness_run.wio"
        "async-correctness-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_task_surface_run
        "${CMAKE_SOURCE_DIR}/tests/async_task_surface_run.wio"
        "async-task-surface-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_scope_run
        "${CMAKE_SOURCE_DIR}/tests/async_scope_run.wio"
        "async-scope-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_scope_syntax_run
        "${CMAKE_SOURCE_DIR}/tests/async_scope_syntax_run.wio"
        "async-scope-syntax-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_scope_auto_import_run
        "${CMAKE_SOURCE_DIR}/tests/async_scope_auto_import_run.wio"
        "async-scope-auto-import-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_main_executor_run
        "${CMAKE_SOURCE_DIR}/tests/async_main_executor_run.wio"
        "async-main-executor-ok"
        --run
    )
    add_wio_output_test(
        wio_test_application_async_main_run
        "${CMAKE_SOURCE_DIR}/tests/application_async_main_run.wio"
        "application-async-main-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_fs_io_run
        "${CMAKE_SOURCE_DIR}/tests/async_fs_io_run.wio"
        "async-fs-io-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_process_run
        "${CMAKE_SOURCE_DIR}/tests/async_process_run.wio"
        "async-process-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_process_stream_run
        "${CMAKE_SOURCE_DIR}/tests/async_process_stream_run.wio"
        "async-process-stream-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_channel_run
        "${CMAKE_SOURCE_DIR}/tests/async_channel_run.wio"
        "async-channel-ok"
        --run
    )
    add_wio_output_test(
        wio_test_std_module_reimport_alias_run
        "${CMAKE_SOURCE_DIR}/tests/std_module_reimport_alias_run.wio"
        "std-module-reimport-alias-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_file_watcher_run
        "${CMAKE_SOURCE_DIR}/tests/async_file_watcher_run.wio"
        "async-file-watcher-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_net_io_run
        "${CMAKE_SOURCE_DIR}/tests/async_net_io_run.wio"
        "async-net-io-ok"
        --run
    )
    add_wio_output_test(
        wio_test_async_net_accept_run
        "${CMAKE_SOURCE_DIR}/tests/async_net_accept_run.wio"
        "async-net-accept-ok"
        --run
    )
    set_tests_properties(
        wio_test_async_net_io_run
        wio_test_async_net_accept_run
        PROPERTIES TIMEOUT 30
    )
    add_wio_output_test(
        wio_test_std_net_timeout_run
        "${CMAKE_SOURCE_DIR}/tests/std_net_timeout_run.wio"
        "std-net-timeout-ok"
        --run
    )

    add_wio_output_test(
        wio_test_lambda_capture_value_run
        "${CMAKE_SOURCE_DIR}/tests/lambda_capture_value_run.wio"
        "lambda-capture-value-ok"
        --run
    )
    set_tests_properties(
        wio_test_lambda_capture_value_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_async_structured_run
        wio_test_async_block_on_run
        wio_test_async_freeze_run
        wio_test_async_detached_exit_run
        wio_test_async_correctness_run
        wio_test_async_task_surface_run
        wio_test_async_scope_run
        wio_test_async_scope_syntax_run
        wio_test_async_scope_auto_import_run
        wio_test_async_main_executor_run
        wio_test_application_async_main_run
        wio_test_async_fs_io_run
        wio_test_async_process_run
        wio_test_async_process_stream_run
        wio_test_async_channel_run
        wio_test_std_module_reimport_alias_run
        wio_test_async_file_watcher_run
        wio_test_async_net_io_run
        wio_test_async_net_accept_run
        wio_test_std_net_timeout_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        wio_test_async_process_stream_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR};WIO_ASYNC_IO_WORKERS=1"
    )

    add_wio_failure_test(
        wio_invalid_spawn_outside_async_scope
        "${CMAKE_SOURCE_DIR}/tests/invalid/spawn_outside_async_scope.wio"
        "'spawn' requires an enclosing 'async scope'\."
    )
    add_wio_failure_test(
        wio_invalid_async_scope_outside_async
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_scope_outside_async.wio"
        "'await' can only be used inside an async function or method"
    )
    add_wio_failure_test(
        wio_invalid_await_main_outside_async
        "${CMAKE_SOURCE_DIR}/tests/invalid/await_main_outside_async.wio"
        "'await main' can only be used inside an async function or method"
    )
    add_wio_failure_test(
        wio_invalid_spawn_worker_borrow_capture
        "${CMAKE_SOURCE_DIR}/tests/invalid/spawn_worker_borrow_capture.wio"
        "spawn worker cannot transfer capture 'borrowed'"
    )
    add_wio_failure_test(
        wio_invalid_spawn_blocking_borrow_capture
        "${CMAKE_SOURCE_DIR}/tests/invalid/spawn_blocking_borrow_capture.wio"
        "spawn blocking cannot transfer capture 'borrowed'"
    )
    set_tests_properties(
        wio_test_async_worker_config_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR};WIO_ASYNC_WORKERS=3"
    )
    set_tests_properties(wio_test_async_detached_exit_run PROPERTIES TIMEOUT 15)

    add_wio_non_dry_failure_test_clean(
        wio_runtime_async_timeout
        "${CMAKE_SOURCE_DIR}/tests/async_timeout_runtime_failure.wio"
        "Runtime Error: Unhandled native exception: asynchronous operation timed out"
        "terminate called|segmentation fault|Assertion"
        --run
    )
    add_wio_non_dry_failure_test_clean(
        wio_runtime_async_cancel
        "${CMAKE_SOURCE_DIR}/tests/async_cancel_runtime_failure.wio"
        "Runtime Error: Unhandled native exception: asynchronous operation was cancelled"
        "terminate called|segmentation fault|Assertion"
        --run
    )
    set_tests_properties(
        wio_runtime_async_timeout
        wio_runtime_async_cancel
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_failure_test(wio_invalid_async_await_outside_async
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_await_outside_async.wio"
        "'await' can only be used inside an async function or method")
    add_wio_failure_test(wio_invalid_async_await_requires_coroutine
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_await_requires_coroutine.wio"
        "await.*operand must be a coroutine")
    add_wio_failure_test(wio_invalid_async_reference_parameter
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_reference_parameter.wio"
        "Async parameter 'value' cannot be ref/view")
    add_wio_failure_test(wio_invalid_async_reference_return
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_reference_return.wio"
        "Async functions cannot return ref/view")
    add_wio_failure_test(wio_invalid_async_component_method
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_component_method.wio"
        "Stack-resident component methods cannot be async")
    add_wio_failure_test(wio_invalid_async_extension_receiver
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_extension_receiver.wio"
        "Async extension methods are not yet lifetime-safe")
    add_wio_failure_test(wio_invalid_async_lifecycle_method
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_lifecycle_method.wio"
        "OnConstruct cannot be async")
    add_wio_failure_test(wio_invalid_async_coroutine_arity
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_coroutine_arity.wio"
        "coroutine.*requires exactly one result type")
    add_wio_failure_test(wio_invalid_async_run_blocking_borrow_capture
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_blocking_borrow_capture.wio"
        "RunBlocking cannot transfer capture 'borrowed'")
    add_wio_failure_test(wio_invalid_async_run_borrow_capture
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_borrow_capture.wio"
        "Run cannot transfer capture 'borrowed'")
    add_wio_failure_test(wio_invalid_async_run_blocking_object_capture
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_blocking_object_capture.wio"
        "RunBlocking cannot transfer capture 'counter'")
    add_wio_failure_test(wio_invalid_async_run_blocking_self_capture
        "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_blocking_self_capture.wio"
        "RunBlocking cannot capture 'self'")
    set_tests_properties(
        wio_invalid_async_await_outside_async
        wio_invalid_async_await_requires_coroutine
        wio_invalid_async_reference_parameter
        wio_invalid_async_reference_return
        wio_invalid_async_component_method
        wio_invalid_async_extension_receiver
        wio_invalid_async_lifecycle_method
        wio_invalid_async_coroutine_arity
        wio_invalid_async_run_blocking_borrow_capture
        wio_invalid_async_run_borrow_capture
        wio_invalid_async_run_blocking_object_capture
        wio_invalid_async_run_blocking_self_capture
        wio_invalid_spawn_worker_borrow_capture
        wio_invalid_spawn_blocking_borrow_capture
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )

    add_wio_output_test(
        wio_test_unicode_text_literal_run
        "${CMAKE_SOURCE_DIR}/tests/unicode_text_literal_run.wio"
        "unicode-text-ok"
        --run
    )
    add_wio_failure_test(
        wio_invalid_text_requires_explicit_utf8_validation
        "${CMAKE_SOURCE_DIR}/tests/invalid/text_requires_explicit_utf8_validation.wio"
        "Type mismatch for 'validated'"
    )
    add_wio_failure_test(
        wio_invalid_text_operator
        "${CMAKE_SOURCE_DIR}/tests/invalid/text_invalid_operator.wio"
        "Textual values do not support operator '-'"
    )
    add_wio_failure_test(
        wio_invalid_text_string_mixed_operation
        "${CMAKE_SOURCE_DIR}/tests/invalid/text_string_mixed_operation.wio"
        "Mixed 'string' and 'text' operations require an explicit conversion"
    )
    add_wio_failure_test(
        wio_invalid_text_index_assignment
        "${CMAKE_SOURCE_DIR}/tests/invalid/text_index_assignment.wio"
        "Text indexing is read-only"
    )
    set_tests_properties(
        wio_test_unicode_text_literal_run
        PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
    )
endif()
