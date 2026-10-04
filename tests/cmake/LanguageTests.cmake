# Language test registrations. Included in the app directory scope.

add_test(
    NAME wio_wir_cpp_packs_differential
    COMMAND
        ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/wir_cpp_packs_run.wio"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-packs-differential"
        -DWIO_NO_BUILTIN=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_differential_test.cmake"
)
wio_test_labels(wio_wir_cpp_packs_differential language)

set_tests_properties(
    wio_wir_cpp_packs_differential
    PROPERTIES LABELS "wir;cpp-backend;differential;release-gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    TIMEOUT 120
)

add_test(
    NAME wio_wir_cpp_behavioral_differential
    COMMAND
        ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-behavioral-differential"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_behavioral_differential_test.cmake"
)
wio_test_labels(wio_wir_cpp_behavioral_differential language)

set_tests_properties(
    wio_wir_cpp_behavioral_differential
    PROPERTIES LABELS "wir;cpp-backend;attributes;differential;release-gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    TIMEOUT 600
)

add_wio_success_test(
    wio_test1_dry_run
    "${CMAKE_SOURCE_DIR}/tests/test1.wio"
)
wio_test_labels(wio_test1_dry_run language)

add_wio_success_test(
    wio_test2_dry_run
    "${CMAKE_SOURCE_DIR}/tests/test2.wio"
)
wio_test_labels(wio_test2_dry_run language)

add_wio_success_test(
    wio_test_use_alias_dry_run
    "${CMAKE_SOURCE_DIR}/tests/test_use_alias.wio"
)
wio_test_labels(wio_test_use_alias_dry_run language traceability)

add_wio_success_test(
    wio_test_user_module_merge_dry_run
    "${CMAKE_SOURCE_DIR}/tests/modules/single_file_root.wio"
)
wio_test_labels(wio_test_user_module_merge_dry_run language)

add_wio_output_test(
    wio_test_user_module_alias_run
    "${CMAKE_SOURCE_DIR}/tests/modules/user_alias_root.wio"
    "Value: 42"
    --run
)
wio_test_labels(wio_test_user_module_alias_run language)

add_wio_output_test(
    wio_test_direct_module_import_run
    "${CMAKE_SOURCE_DIR}/tests/modules/direct_import_root.wio"
    "Direct import math: 5, 3"
    --run
)
wio_test_labels(wio_test_direct_module_import_run language traceability)

add_wio_output_test(
    wio_test_direct_module_import_alias_run
    "${CMAKE_SOURCE_DIR}/tests/modules/direct_import_alias_root.wio"
    "Direct import alias math: 5, 3, 4"
    --run
)
wio_test_labels(wio_test_direct_module_import_alias_run language)

add_wio_output_test(
    wio_test_mutable_data_ergonomics_run
    "${CMAKE_SOURCE_DIR}/tests/mutable_data_ergonomics_run.wio"
    "Mutable ergonomics: ok"
    --run
)
wio_test_labels(wio_test_mutable_data_ergonomics_run language traceability)

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
wio_test_labels(wio_test_generated_cpp_cleanup language)

set_tests_properties(
    wio_test_generated_cpp_cleanup
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
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
wio_test_labels(wio_test_example_plain_app_run language)

set_tests_properties(
    wio_test_example_plain_app_run
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
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
wio_test_labels(wio_test_example_observatory_run language)

set_tests_properties(
    wio_test_example_observatory_run
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
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
wio_test_labels(wio_test_example_hybrid_module_run language)

set_tests_properties(
    wio_test_example_hybrid_module_run
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_example_telemetry_pipeline_run
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/telemetry_pipeline
        -DWIO_EXPECT=Telemetry\ pipeline:\ Wio\ 0.18.1
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExampleProject.cmake"
)
wio_test_labels(wio_test_example_telemetry_pipeline_run language freeze)

set_tests_properties(
    wio_test_example_telemetry_pipeline_run
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_example_mini_game_engine_run
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/mini_game_engine
        -DWIO_EXPECT=MiniWio\ Flappy:\ mode=headless\ frames=720
        -DWIO_RUN_ARGS=--headless
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExampleProject.cmake"
)
wio_test_labels(wio_test_example_mini_game_engine_run language)

set_tests_properties(
    wio_test_example_mini_game_engine_run
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_success_test(
    wio_test_wio_tool_run_hybrid_arena_demo_dry_run
    "${CMAKE_SOURCE_DIR}/scripts/wio/run_hybrid_arena_demo.wio"
)
wio_test_labels(wio_test_wio_tool_run_hybrid_arena_demo_dry_run language)

add_wio_output_test(
    wio_test_import_all_alias_run
    "${CMAKE_SOURCE_DIR}/tests/modules/import_all_alias_root.wio"
    "import-all-alias-ok"
    --run
)
wio_test_labels(wio_test_import_all_alias_run language)

add_wio_output_test(
    wio_test_import_path_module_dir_run
    "${CMAKE_SOURCE_DIR}/tests/import_path_root.wio"
    "Import path threat: 45"
    --run
    --module-dir
    "${CMAKE_SOURCE_DIR}/tests/imports"
)
wio_test_labels(wio_test_import_path_module_dir_run language)

add_wio_output_test(
    wio_test_realm_merge_across_modules_run
    "${CMAKE_SOURCE_DIR}/tests/modules/realm_merge_root.wio"
    "Merged realm sum: 3"
    --run
)
wio_test_labels(wio_test_realm_merge_across_modules_run language)

add_wio_output_test(
    wio_test_relative_module_imports_run
    "${CMAKE_SOURCE_DIR}/tests/modules/relative/root.wio"
    "Relative import sum: 50"
    --run
)
wio_test_labels(wio_test_relative_module_imports_run language)

add_wio_output_test(
    wio_test_realm_basic_run
    "${CMAKE_SOURCE_DIR}/tests/realm_basic.wio"
    "Realm sum: 3"
    --run
)
wio_test_labels(wio_test_realm_basic_run language traceability)

add_wio_output_test(
    wio_test_realm_object_isolation_run
    "${CMAKE_SOURCE_DIR}/tests/realm_object_isolation.wio"
    "Realm counters: 3"
    --run
)
wio_test_labels(wio_test_realm_object_isolation_run language)

add_wio_output_test(
    wio_test_realm_object_method_calls_run
    "${CMAKE_SOURCE_DIR}/tests/realm_object_method_calls_run.wio"
    "Realm object method sum: 7"
    --run
)
wio_test_labels(wio_test_realm_object_method_calls_run language)

add_wio_output_test(
    wio_test_realm_nested_run
    "${CMAKE_SOURCE_DIR}/tests/realm_nested.wio"
    "Nested realm sum: 7"
    --run
)
wio_test_labels(wio_test_realm_nested_run language traceability)

add_wio_output_test(
    wio_test_realm_qualified_types_run
    "${CMAKE_SOURCE_DIR}/tests/realm_qualified_types.wio"
    "Qualified cast hp: 7"
    --run
)
wio_test_labels(wio_test_realm_qualified_types_run language)

add_wio_output_test(
    wio_test_realm_qualified_from_run
    "${CMAKE_SOURCE_DIR}/tests/realm_qualified_from.wio"
    "Qualified from hp: 7"
    --run
)
wio_test_labels(wio_test_realm_qualified_from_run language traceability)

add_wio_output_test(
    wio_test_object_component_access_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/object_component_access_semantics.wio"
    "Object/component access semantics: 17"
    --run
)
wio_test_labels(wio_test_object_component_access_semantics_run language traceability)

add_wio_output_test(
    wio_test_trusted_access_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/trusted_access_semantics.wio"
    "Trusted access sum: 49"
    --run
)
wio_test_labels(wio_test_trusted_access_semantics_run language)

add_wio_output_test(
    wio_test_interface_ref_upcast_run
    "${CMAKE_SOURCE_DIR}/tests/interface_ref_upcast.wio"
    "Interface ref hp: 7"
    --run
)
wio_test_labels(wio_test_interface_ref_upcast_run language traceability)

add_wio_output_test(
    wio_test_component_embedded_member_ctor_run
    "${CMAKE_SOURCE_DIR}/tests/component_embedded_member_ctor.wio"
    "Embedded component: 7/3"
    --run
)
wio_test_labels(wio_test_component_embedded_member_ctor_run language traceability)

add_wio_output_test(
    wio_test_inferred_object_and_fit_locals_run
    "${CMAKE_SOURCE_DIR}/tests/inferred_object_and_fit_locals.wio"
    "Inferred locals hp: 7"
    --run
)
wio_test_labels(wio_test_inferred_object_and_fit_locals_run language)

add_wio_output_test(
    wio_test_for_in_dynamic_array_run
    "${CMAKE_SOURCE_DIR}/tests/for_in_dynamic_array.wio"
    "For in dynamic sum: 7"
    --run
)
wio_test_labels(wio_test_for_in_dynamic_array_run language traceability)

add_wio_output_test(
    wio_test_for_in_static_array_run
    "${CMAKE_SOURCE_DIR}/tests/for_in_static_array.wio"
    "For in static sum: 9"
    --run
)
wio_test_labels(wio_test_for_in_static_array_run language)

add_wio_output_test(
    wio_test_foreach_dynamic_array_run
    "${CMAKE_SOURCE_DIR}/tests/foreach_dynamic_array.wio"
    "Foreach sum: 12"
    --run
)
wio_test_labels(wio_test_foreach_dynamic_array_run language)

add_wio_output_test(
    wio_test_for_in_inclusive_range_run
    "${CMAKE_SOURCE_DIR}/tests/for_in_inclusive_range.wio"
    "Inclusive range sum: 10"
    --run
)
wio_test_labels(wio_test_for_in_inclusive_range_run language)

add_wio_output_test(
    wio_test_for_in_exclusive_range_run
    "${CMAKE_SOURCE_DIR}/tests/for_in_exclusive_range.wio"
    "Exclusive range sum: 6"
    --run
)
wio_test_labels(wio_test_for_in_exclusive_range_run language)

add_wio_output_test(
    wio_test_for_in_range_step_positive_run
    "${CMAKE_SOURCE_DIR}/tests/for_in_range_step_positive.wio"
    "Positive step sum: 16"
    --run
)
wio_test_labels(wio_test_for_in_range_step_positive_run language)

add_wio_output_test(
    wio_test_for_in_range_step_negative_run
    "${CMAKE_SOURCE_DIR}/tests/for_in_range_step_negative.wio"
    "Negative step sum: 16"
    --run
)
wio_test_labels(wio_test_for_in_range_step_negative_run language)

add_wio_output_test(
    wio_test_for_parenthesized_array_step_run
    "${CMAKE_SOURCE_DIR}/tests/for_parenthesized_array_step.wio"
    "Parenthesized array step score: 28"
    --run
)
wio_test_labels(wio_test_for_parenthesized_array_step_run language)

add_wio_output_test(
    wio_test_for_array_index_value_run
    "${CMAKE_SOURCE_DIR}/tests/for_array_index_value.wio"
    "Array index/value score: 51"
    --run
)
wio_test_labels(wio_test_for_array_index_value_run language)

add_wio_output_test(
    wio_test_for_ref_view_array_run
    "${CMAKE_SOURCE_DIR}/tests/for_ref_view_array.wio"
    "Ref/view array: 11, 12, 13 => 3"
    --run
)
wio_test_labels(wio_test_for_ref_view_array_run language)

add_wio_output_test(
    wio_test_for_dict_key_value_run
    "${CMAKE_SOURCE_DIR}/tests/for_dict_key_value.wio"
    "Dict loop score: 100"
    --run
)
wio_test_labels(wio_test_for_dict_key_value_run language traceability)

add_wio_output_test(
    wio_test_for_dict_view_ref_binding_run
    "${CMAKE_SOURCE_DIR}/tests/for_dict_view_ref_binding.wio"
    "Dict ref/value score: 50"
    --run
)
wio_test_labels(wio_test_for_dict_view_ref_binding_run language)

add_wio_output_test(
    wio_test_for_component_binding_run
    "${CMAKE_SOURCE_DIR}/tests/for_component_binding.wio"
    "Component binding total: 16"
    --run
)
wio_test_labels(wio_test_for_component_binding_run language traceability)

add_wio_output_test(
    wio_test_for_component_ref_binding_arithmetic_run
    "${CMAKE_SOURCE_DIR}/tests/for_component_ref_binding_arithmetic.wio"
    "Component ref binding total: 16, seen: 16"
    --run
)
wio_test_labels(wio_test_for_component_ref_binding_arithmetic_run language)

add_wio_output_test(
    wio_test_for_component_index_binding_run
    "${CMAKE_SOURCE_DIR}/tests/for_component_index_binding.wio"
    "Component index binding total: 11"
    --run
)
wio_test_labels(wio_test_for_component_index_binding_run language)

add_wio_output_test(
    wio_test_for_component_index_binding_step_run
    "${CMAKE_SOURCE_DIR}/tests/for_component_index_binding_step.wio"
    "Component step binding total: 16"
    --run
)
wio_test_labels(wio_test_for_component_index_binding_step_run language)

add_wio_output_test(
    wio_test_c_style_for_loop_run
    "${CMAKE_SOURCE_DIR}/tests/c_style_for_loop.wio"
    "C for sum: 10"
    --run
)
wio_test_labels(wio_test_c_style_for_loop_run language traceability)

add_wio_output_test(
    wio_test_match_value_basic_run
    "${CMAKE_SOURCE_DIR}/tests/match_value_basic.wio"
    "Match value: zero / small / mid / large"
    --run
)
wio_test_labels(wio_test_match_value_basic_run language traceability freeze)

add_wio_output_test(
    wio_test_match_statement_basic_run
    "${CMAKE_SOURCE_DIR}/tests/match_statement_basic.wio"
    "Match statement: 1 / 12 / 100"
    --run
)
wio_test_labels(wio_test_match_statement_basic_run language traceability freeze)

add_wio_output_test(
    wio_test_match_statement_non_exhaustive_run
    "${CMAKE_SOURCE_DIR}/tests/match_statement_non_exhaustive.wio"
    "Non-exhaustive statement match: 11"
    --run
)
wio_test_labels(wio_test_match_statement_non_exhaustive_run language freeze)

add_wio_output_test(
    wio_test_when_guard_fallback_value_run
    "${CMAKE_SOURCE_DIR}/tests/when_guard_fallback_value.wio"
    "When guard: 7 / 0"
    --run
)
wio_test_labels(wio_test_when_guard_fallback_value_run language traceability)

add_wio_output_test(
    wio_test_ref_view_fit_is_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/ref_view_fit_is_semantics.wio"
    "Ref/view/fit/is: 6/6/6"
    --run
)
wio_test_labels(wio_test_ref_view_fit_is_semantics_run language)

add_wio_output_test(
    wio_test_object_self_return_run
    "${CMAKE_SOURCE_DIR}/tests/object_self_return_run.wio"
    "Object self return: ok"
    --run
)
wio_test_labels(wio_test_object_self_return_run language traceability)

add_wio_output_test(
    wio_test_component_extension_run
    "${CMAKE_SOURCE_DIR}/tests/component_extension_run.wio"
    "Component extension: ok"
    --run
)
wio_test_labels(wio_test_component_extension_run language)

add_wio_output_test(
    wio_test_ref_view_lifetime_matrix_run
    "${CMAKE_SOURCE_DIR}/tests/ref_view_lifetime_matrix_run.wio"
    "ref-view-lifetime-matrix-ok"
    --run
)
wio_test_labels(wio_test_ref_view_lifetime_matrix_run language)

add_wio_output_test(
    wio_test_numeric_promotion_run
    "${CMAKE_SOURCE_DIR}/tests/numeric_promotion_run.wio"
    "numeric-promotion-ok"
    --run
)
wio_test_labels(wio_test_numeric_promotion_run language)

add_wio_output_test(
    wio_test_char_numeric_conversion_run
    "${CMAKE_SOURCE_DIR}/tests/char_numeric_conversion_run.wio"
    "char-numeric-conversion-ok"
    --run
)
wio_test_labels(wio_test_char_numeric_conversion_run language)

add_wio_output_test(
    wio_test_char_numeric_conversion_legacy_run
    "${CMAKE_SOURCE_DIR}/tests/char_numeric_conversion_run.wio"
    "char-numeric-conversion-ok"
    --run
    --cpp-backend legacy
)
wio_test_labels(wio_test_char_numeric_conversion_legacy_run language)

add_wio_output_test(
    wio_test_integer_literal_abi_run
    "${CMAKE_SOURCE_DIR}/tests/integer_literal_abi_run.wio"
    "integer-literal-abi-ok"
    --run
)
wio_test_labels(wio_test_integer_literal_abi_run language)

add_wio_output_test(
    wio_test_nullable_targets_run
    "${CMAKE_SOURCE_DIR}/tests/nullable_targets_run.wio"
    "nullable-targets-ok"
    --run
)
wio_test_labels(wio_test_nullable_targets_run language)

add_wio_output_test(
    wio_test_parser_precedence_run
    "${CMAKE_SOURCE_DIR}/tests/parser_precedence_run.wio"
    "parser-precedence-ok"
    --run
)
wio_test_labels(wio_test_parser_precedence_run language)

add_wio_output_test(
    wio_test_increment_decrement_run
    "${CMAKE_SOURCE_DIR}/tests/increment_decrement_run.wio"
    "increment-decrement-ok"
    --run
)
wio_test_labels(wio_test_increment_decrement_run language)

add_wio_output_test(
    wio_test_contextual_keyword_identifiers_run
    "${CMAKE_SOURCE_DIR}/tests/contextual_keyword_identifiers_run.wio"
    "Contextual keyword identifiers: ok"
    --run
)
wio_test_labels(wio_test_contextual_keyword_identifiers_run language)

add_wio_output_test(
    wio_test_default_initialized_variables_run
    "${CMAKE_SOURCE_DIR}/tests/default_initialized_variables_run.wio"
    "Default initialized variables: ok"
    --run
)
wio_test_labels(wio_test_default_initialized_variables_run language)

add_wio_output_test(
    wio_test_conditional_operator_run
    "${CMAKE_SOURCE_DIR}/tests/conditional_operator_run.wio"
    "Conditional operator: ok"
    --run
)
wio_test_labels(wio_test_conditional_operator_run language)

add_wio_output_test(
    wio_test_interpolated_string_nested_run
    "${CMAKE_SOURCE_DIR}/tests/interpolated_string_nested_run.wio"
    "Interpolated string nested: ok"
    --run
)
wio_test_labels(wio_test_interpolated_string_nested_run language)

add_wio_output_test(
    wio_test_lifecycle_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/lifecycle_semantics_run.wio"
    "lifecycle-semantics-ok"
    --run
)
wio_test_labels(wio_test_lifecycle_semantics_run language)

add_wio_output_test(
    wio_test_dict_option_intrinsic_run
    "${CMAKE_SOURCE_DIR}/tests/dict_option_intrinsic_run.wio"
    "dict-option-intrinsic-ok"
    --run
)
wio_test_labels(wio_test_dict_option_intrinsic_run language)

add_wio_output_test(
    wio_test_generic_object_explicit_specialization_run
    "${CMAKE_SOURCE_DIR}/tests/generic_object_explicit_specialization_run.wio"
    "Generic object specialization: ok"
    --run
)
wio_test_labels(wio_test_generic_object_explicit_specialization_run language traceability)

add_wio_output_test(
    wio_test_generic_specialization_visibility_modules_run
    "${CMAKE_SOURCE_DIR}/tests/modules/generic_specialization_visibility_root.wio"
    "Generic specialization modules: ok"
    --run
)
wio_test_labels(wio_test_generic_specialization_visibility_modules_run language)

add_wio_output_test(
    wio_test_generic_default_type_arguments_run
    "${CMAKE_SOURCE_DIR}/tests/generic_default_type_arguments_run.wio"
    "Generic defaults: ok"
    --run
)
wio_test_labels(wio_test_generic_default_type_arguments_run language)

add_wio_output_test(
    wio_test_generic_partial_specialization_run
    "${CMAKE_SOURCE_DIR}/tests/generic_partial_specialization_run.wio"
    "Generic partial specialization: ok"
    --run
)
wio_test_labels(wio_test_generic_partial_specialization_run language)

add_wio_output_test(
    wio_test_generic_partial_specialization_ordering_run
    "${CMAKE_SOURCE_DIR}/tests/generic_partial_specialization_ordering_run.wio"
    "generic-partial-ordering-ok"
    --run
)
wio_test_labels(wio_test_generic_partial_specialization_ordering_run language coherence)

set_tests_properties(
    wio_test_generic_partial_specialization_ordering_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_extension_default_parameters_run
    "${CMAKE_SOURCE_DIR}/tests/extension_default_parameters_run.wio"
    "extension-default-parameters-ok"
    --run
)
wio_test_labels(wio_test_extension_default_parameters_run language coherence)

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
wio_test_labels(wio_test_match_enum_exhaustive_guards_run language freeze coherence)

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
wio_test_labels(wio_test_extension_generic_methods_run language coherence)

set_tests_properties(
    wio_test_extension_generic_methods_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_generic_const_parameters_run
    "${CMAKE_SOURCE_DIR}/tests/generic_const_parameters_run.wio"
    "Const generics: ok"
    --run
)
wio_test_labels(wio_test_generic_const_parameters_run language)

add_wio_output_test(
    wio_test_generic_where_constraints_run
    "${CMAKE_SOURCE_DIR}/tests/generic_where_constraints_run.wio"
    "Generic where constraints: ok"
    --run
)
wio_test_labels(wio_test_generic_where_constraints_run language)

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
wio_test_labels(wio_test_hybrid_arena_demo language)

add_wio_output_test(
    wio_test_fit_numeric_clamp_run
    "${CMAKE_SOURCE_DIR}/tests/fit_numeric_clamp.wio"
    "Fit clamp: 0, 127"
    --run
)
wio_test_labels(wio_test_fit_numeric_clamp_run language traceability)

add_wio_output_test(
    wio_test_const_scalar_expressions_run
    "${CMAKE_SOURCE_DIR}/tests/const_scalar_expressions.wio"
    "Const scalar: 11, 12, true, x"
    --run
)
wio_test_labels(wio_test_const_scalar_expressions_run language traceability)

add_wio_output_test(
    wio_test_numeric_literal_suffixes_run
    "${CMAKE_SOURCE_DIR}/tests/numeric_literal_suffixes_run.wio"
    "Numeric literal suffixes: 16/3/true/true"
    --run
)
wio_test_labels(wio_test_numeric_literal_suffixes_run language traceability)

add_wio_output_test(
    wio_test_numeric_literal_context_run
    "${CMAKE_SOURCE_DIR}/tests/numeric_literal_context_run.wio"
    "Numeric literal context: 0/2/3/5/20"
    --run
)
wio_test_labels(wio_test_numeric_literal_context_run language)

add_wio_output_test(
    wio_test_numeric_literal_signed_mins_run
    "${CMAKE_SOURCE_DIR}/tests/numeric_literal_signed_mins_run.wio"
    "Signed literal mins: ok"
    --run
)
wio_test_labels(wio_test_numeric_literal_signed_mins_run language traceability)

add_wio_output_test(
    wio_test_default_parameters_run
    "${CMAKE_SOURCE_DIR}/tests/default_parameters_run.wio"
    "Default parameters: ok"
    --run
)
wio_test_labels(wio_test_default_parameters_run language traceability)

add_wio_output_test(
    wio_test_wir_ref_object_assignment_run
    "${CMAKE_SOURCE_DIR}/tests/wir_ref_object_assignment_run.wio"
    "WIR ref object assignment: ok"
    --run
)
wio_test_labels(wio_test_wir_ref_object_assignment_run language)

add_wio_output_test(
    wio_test_function_parameter_pack_run
    "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_run.wio"
    "Function packs: ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_function_parameter_pack_run language traceability)

add_wio_output_test(
    wio_test_function_parameter_pack_explicit_type_arguments_run
    "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_explicit_type_arguments_run.wio"
    "Function packs explicit args: ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_function_parameter_pack_explicit_type_arguments_run language traceability)

add_wio_output_test(
    wio_test_function_parameter_pack_constraints_run
    "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_constraints_run.wio"
    "Function pack constraints: ok"
    --run
)
wio_test_labels(wio_test_function_parameter_pack_constraints_run language traceability)

add_wio_output_test(
    wio_test_function_parameter_pack_defaults_run
    "${CMAKE_SOURCE_DIR}/tests/function_parameter_pack_defaults_run.wio"
    "Function pack defaults: ok"
    --run
)
wio_test_labels(wio_test_function_parameter_pack_defaults_run language traceability)

add_wio_output_test(
    wio_test_pack_storage_write_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/pack_storage_write_semantics_run.wio"
    "Pack storage write semantics: ok"
    --run
)
wio_test_labels(wio_test_pack_storage_write_semantics_run language traceability)

add_wio_output_test(
    wio_test_pack_meta_and_generic_declarations_run
    "${CMAKE_SOURCE_DIR}/tests/pack_meta_and_generic_declarations_run.wio"
    "Pack meta \\+ generic declarations: ok"
    --run
)
wio_test_labels(wio_test_pack_meta_and_generic_declarations_run language traceability)

add_wio_output_test(
    wio_test_entry_default_void_run
    "${CMAKE_SOURCE_DIR}/tests/entry_default_void_run.wio"
    "Entry default void works"
    --run
)
wio_test_labels(wio_test_entry_default_void_run language)

add_wio_output_test(
    wio_test_cpp_header_use_statement_run
    "${CMAKE_SOURCE_DIR}/tests/cpp_header_use_statement.wio"
    "CppHeader use: 42"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_cpp_header_use_statement_run language)

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
wio_test_labels(wio_test_backend_info_resolves_paths language)

add_wio_emit_cpp_file_test(
    wio_test_emit_cpp_generates_preamble
    "${CMAKE_SOURCE_DIR}/tests/test1.wio"
    "// Generated by the Wio compiler\\."
)
wio_test_labels(wio_test_emit_cpp_generates_preamble language)

add_wio_output_test(
    wio_test_array_member_methods_run
    "${CMAKE_SOURCE_DIR}/tests/array_member_methods.wio"
    "Array members: 4/1/3/5/2/-1/2/2/2/9/8/2/9/2/true/wi-o-arena/4/true/true/5/5/true"
    --run
)
wio_test_labels(wio_test_array_member_methods_run language traceability)

add_wio_output_test(
    wio_test_dictionary_member_methods_run
    "${CMAKE_SOURCE_DIR}/tests/dictionary_member_methods.wio"
    "Dict members: true/true/7/true/-1/true/7/11/4/true/true/true/true/5/true/4/1/10/5/50/3/5/true"
    --run
)
wio_test_labels(wio_test_dictionary_member_methods_run language traceability)

add_wio_output_test(
    wio_test_string_member_methods_run
    "${CMAKE_SOURCE_DIR}/tests/string_member_methods.wio"
    "String members: 9/true/true/true/true/4/8/W/a/\\?/Arena/Arena/Wio/Arena/Wio/Arena/wio arena/WIO ARENA/Wio World/Wio wio/xoxoxo/2/Arena/3/c/007/700/cba/!DLROWOIW/hi/true"
    --run
)
wio_test_labels(wio_test_string_member_methods_run language traceability)

add_wio_output_test(
    wio_test_direct_indexing_run
    "${CMAKE_SOURCE_DIR}/tests/direct_indexing_run.wio"
    "Direct indexing: ok"
    --run
)
wio_test_labels(wio_test_direct_indexing_run language traceability)

add_wio_output_test(
    wio_test_fixed_array_auto_extent_run
    "${CMAKE_SOURCE_DIR}/tests/fixed_array_auto_extent_run.wio"
    "fixed-array-auto-extent-ok"
    --run
)
wio_test_labels(wio_test_fixed_array_auto_extent_run language traceability coherence)

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
wio_test_labels(wio_test_ref_indexed_element_run language)

add_wio_output_test(
    wio_test_control_flow_return_analysis_run
    "${CMAKE_SOURCE_DIR}/tests/control_flow_return_analysis_run.wio"
    "Control-flow returns: ok"
    --run
)
wio_test_labels(wio_test_control_flow_return_analysis_run language traceability)

add_wio_output_test(
    wio_test_else_if_chain_run
    "${CMAKE_SOURCE_DIR}/tests/else_if_chain_run.wio"
    "Else-if chain: 2"
    --run
)
wio_test_labels(wio_test_else_if_chain_run language)

add_wio_output_test(
    wio_test_intrinsic_member_overloads_run
    "${CMAKE_SOURCE_DIR}/tests/intrinsic_member_overloads.wio"
    "Intrinsic overloads: true/true/4/4/8/Arena/Arena/  7/7  /2/3/3"
    --run
)
wio_test_labels(wio_test_intrinsic_member_overloads_run language)

add_wio_output_test(
    wio_test_const_enum_flagset_reflection_run
    "${CMAKE_SOURCE_DIR}/tests/const_enum_flagset_reflection_run.wio"
    "enum-reflection-ok"
    --run
)
wio_test_labels(wio_test_const_enum_flagset_reflection_run language)

add_wio_output_test(
    wio_test_enum_member_scope_run
    "${CMAKE_SOURCE_DIR}/tests/enum_member_scope_run.wio"
    "enum-member-scope-ok"
    --run
)
wio_test_labels(wio_test_enum_member_scope_run language traceability)

add_wio_output_test(
    wio_test_lambda_contextual_typing_run
    "${CMAKE_SOURCE_DIR}/tests/lambda_contextual_typing_run.wio"
    "Lambda context: true/0/0/0"
    --run
)
wio_test_labels(wio_test_lambda_contextual_typing_run language)

add_wio_output_test(
    wio_test_inline_callback_inference_run
    "${CMAKE_SOURCE_DIR}/tests/inline_callback_inference_run.wio"
    "Inline callback inference: 3/12/21"
    --run
)
wio_test_labels(wio_test_inline_callback_inference_run language)

add_wio_output_test(
    wio_test_function_value_nested_call_run
    "${CMAKE_SOURCE_DIR}/tests/function_value_nested_call_run.wio"
    "Function value chain: 7"
    --run
)
wio_test_labels(wio_test_function_value_nested_call_run language)

add_wio_output_test(
    wio_test_factory_chain_call_run
    "${CMAKE_SOURCE_DIR}/tests/factory_chain_call_run.wio"
    "Factory chain: 0/0/0"
    --run
)
wio_test_labels(wio_test_factory_chain_call_run language)

add_wio_output_test(
    wio_test_generic_function_identity_run
    "${CMAKE_SOURCE_DIR}/tests/generic_function_identity.wio"
    "Generic values: 42, wio"
    --run
)
wio_test_labels(wio_test_generic_function_identity_run language)

add_wio_output_test(
    wio_test_generic_function_ref_assign_run
    "${CMAKE_SOURCE_DIR}/tests/generic_function_ref_assign.wio"
    "Generic assign: 25"
    --run
)
wio_test_labels(wio_test_generic_function_ref_assign_run language)

add_wio_output_test(
    wio_test_generic_function_multi_explicit_run
    "${CMAKE_SOURCE_DIR}/tests/generic_function_multi_explicit.wio"
    "Generic multi: 7|deduced, 8|explicit, wio"
    --run
)
wio_test_labels(wio_test_generic_function_multi_explicit_run language)

add_wio_output_test(
    wio_test_generic_apply_function_constraints_run
    "${CMAKE_SOURCE_DIR}/tests/generic_apply_function_constraints.wio"
    "Generic apply fn: 42, wio"
    --run
)
wio_test_labels(wio_test_generic_apply_function_constraints_run language traceability)

add_wio_output_test(
    wio_test_generic_apply_list_constraints_run
    "${CMAKE_SOURCE_DIR}/tests/generic_apply_list_constraints.wio"
    "Generic apply list: true, true"
    --run
)
wio_test_labels(wio_test_generic_apply_list_constraints_run language)

add_wio_output_test(
    wio_test_generic_overload_prefers_concrete_run
    "${CMAKE_SOURCE_DIR}/tests/generic_overload_prefers_concrete.wio"
    "Generic overloads: int, generic"
    --run
)
wio_test_labels(wio_test_generic_overload_prefers_concrete_run language)

add_wio_output_test(
    wio_test_generic_overload_prefers_specialized_generic_run
    "${CMAKE_SOURCE_DIR}/tests/generic_overload_prefers_specialized_generic.wio"
    "Generic specialized overloads: same, mixed"
    --run
)
wio_test_labels(wio_test_generic_overload_prefers_specialized_generic_run language)

add_wio_output_test(
    wio_test_generic_overload_prefers_generic_over_numeric_conversion_run
    "${CMAKE_SOURCE_DIR}/tests/generic_overload_prefers_generic_over_numeric_conversion.wio"
    "Generic numeric preference: generic"
    --run
)
wio_test_labels(wio_test_generic_overload_prefers_generic_over_numeric_conversion_run language)

add_wio_output_test(
    wio_test_generic_explicit_call_prefers_generic_run
    "${CMAKE_SOURCE_DIR}/tests/generic_explicit_call_prefers_generic.wio"
    "Generic explicit overload: generic"
    --run
)
wio_test_labels(wio_test_generic_explicit_call_prefers_generic_run language)

add_wio_output_test(
    wio_test_generic_realm_explicit_call_run
    "${CMAKE_SOURCE_DIR}/tests/generic_realm_explicit_call.wio"
    "Realm generic: 5/hp"
    --run
)
wio_test_labels(wio_test_generic_realm_explicit_call_run language)

add_wio_output_test(
    wio_test_generic_type_alias_identity_run
    "${CMAKE_SOURCE_DIR}/tests/generic_type_alias_identity.wio"
    "Generic alias: 42, alias, 10"
    --run
)
wio_test_labels(wio_test_generic_type_alias_identity_run language)

add_wio_output_test(
    wio_test_generic_type_alias_nested_run
    "${CMAKE_SOURCE_DIR}/tests/generic_type_alias_nested.wio"
    "Generic alias nested: 12, 36"
    --run
)
wio_test_labels(wio_test_generic_type_alias_nested_run language)

add_wio_output_test(
    wio_test_type_alias_constructor_run
    "${CMAKE_SOURCE_DIR}/tests/type_alias_constructor_run.wio"
    "type-alias-ctor-ok"
    --run
)
wio_test_labels(wio_test_type_alias_constructor_run language)

add_wio_output_test(
    wio_test_operator_overloading_run
    "${CMAKE_SOURCE_DIR}/tests/operator_overloading_run.wio"
    "operator-overload-ok"
    --run
)
wio_test_labels(wio_test_operator_overloading_run language)

add_wio_output_test(
    wio_test_operator_overloading_advanced_run
    "${CMAKE_SOURCE_DIR}/tests/operator_overloading_advanced_run.wio"
    "operator-overload-advanced-ok"
    --run
)
wio_test_labels(wio_test_operator_overloading_advanced_run language)

add_wio_output_test(
    wio_test_read_reference_value_flow_run
    "${CMAKE_SOURCE_DIR}/tests/read_reference_value_flow_run.wio"
    "read-reference-flow-ok"
    --run
)
wio_test_labels(wio_test_read_reference_value_flow_run language)

add_wio_output_test(
    wio_test_operator_conversion_subscript_run
    "${CMAKE_SOURCE_DIR}/tests/operator_conversion_subscript_run.wio"
    "operator-conversion-subscript-ok"
    --run
)
wio_test_labels(wio_test_operator_conversion_subscript_run language)

add_wio_output_test(
    wio_test_operator_call_run
    "${CMAKE_SOURCE_DIR}/tests/operator_call_run.wio"
    "operator-call-ok"
    --run
)
wio_test_labels(wio_test_operator_call_run language)

add_wio_output_test(
    wio_test_generic_fit_target_run
    "${CMAKE_SOURCE_DIR}/tests/generic_fit_target_run.wio"
    "generic-fit-target-ok"
    --run
)
wio_test_labels(wio_test_generic_fit_target_run language)

add_wio_output_test(
    wio_test_generic_operator_body_validation_run
    "${CMAKE_SOURCE_DIR}/tests/generic_operator_body_validation_run.wio"
    "generic-operator-body-validation-ok"
    --run
)
wio_test_labels(wio_test_generic_operator_body_validation_run language)

add_wio_output_test(
    wio_test_generic_apply_type_and_object_run
    "${CMAKE_SOURCE_DIR}/tests/generic_apply_type_and_object_run.wio"
    "Generic apply types: 3, 1.5"
    --run
)
wio_test_labels(wio_test_generic_apply_type_and_object_run language traceability)

add_wio_output_test(
    wio_test_generic_component_pair_run
    "${CMAKE_SOURCE_DIR}/tests/generic_component_pair.wio"
    "Generic component: 7/wio"
    --run
)
wio_test_labels(wio_test_generic_component_pair_run language)

add_wio_output_test(
    wio_test_generic_component_constructor_deduction_run
    "${CMAKE_SOURCE_DIR}/tests/generic_component_constructor_deduction.wio"
    "Generic component ctor deduction: 7/9, wio/true"
    --run
)
wio_test_labels(wio_test_generic_component_constructor_deduction_run language traceability)

add_wio_output_test(
    wio_test_generic_component_constructor_expected_type_run
    "${CMAKE_SOURCE_DIR}/tests/generic_component_constructor_expected_type.wio"
    "Generic component expected type: 4/5, 6/7, 8/9"
    --run
)
wio_test_labels(wio_test_generic_component_constructor_expected_type_run language)

add_wio_output_test(
    wio_test_generic_object_box_run
    "${CMAKE_SOURCE_DIR}/tests/generic_object_box.wio"
    "Generic object: 7, 7"
    --run
)
wio_test_labels(wio_test_generic_object_box_run language)

add_wio_output_test(
    wio_test_generic_object_inheritance_run
    "${CMAKE_SOURCE_DIR}/tests/generic_object_inheritance.wio"
    "Generic object inheritance: 11 / 11"
    --run
)
wio_test_labels(wio_test_generic_object_inheritance_run language)

add_wio_output_test(
    wio_test_generic_object_constructor_deduction_run
    "${CMAKE_SOURCE_DIR}/tests/generic_object_constructor_deduction.wio"
    "Generic ctor deduction: 7, wio"
    --run
)
wio_test_labels(wio_test_generic_object_constructor_deduction_run language traceability)

add_wio_output_test(
    wio_test_generic_object_constructor_expected_type_run
    "${CMAKE_SOURCE_DIR}/tests/generic_object_constructor_expected_type.wio"
    "Generic ctor expected type: 7, 9, 11"
    --run
)
wio_test_labels(wio_test_generic_object_constructor_expected_type_run language)

add_wio_output_test(
    wio_test_generic_object_method_basic_run
    "${CMAKE_SOURCE_DIR}/tests/generic_object_method_basic.wio"
    "Generic method inner: true\nGeneric methods: wio, 42, 7"
    --run
)
wio_test_labels(wio_test_generic_object_method_basic_run language)

add_wio_output_test(
    wio_test_cpp_reserved_identifier_codegen_run
    "${CMAKE_SOURCE_DIR}/tests/cpp_reserved_identifier_codegen.wio"
    "Reserved identifiers: 7/11"
    --run
)
wio_test_labels(wio_test_cpp_reserved_identifier_codegen_run language)

add_wio_output_test(
    wio_test_generic_interface_inheritance_run
    "${CMAKE_SOURCE_DIR}/tests/generic_interface_inheritance.wio"
    "Generic interface inheritance: 9 / 9"
    --run
)
wio_test_labels(wio_test_generic_interface_inheritance_run language)

add_wio_success_test(
    wio_test_generic_interface_declaration_dry_run
    "${CMAKE_SOURCE_DIR}/tests/generic_interface_declaration.wio"
)
wio_test_labels(wio_test_generic_interface_declaration_dry_run language)

add_wio_success_test(
    wio_test_generic_explicit_forward_export_dry_run
    "${CMAKE_SOURCE_DIR}/tests/generic_explicit_forward_export.wio"
)
wio_test_labels(wio_test_generic_explicit_forward_export_dry_run language)

add_wio_success_test(
    wio_test_generic_fit_target_constraints_dry_run
    "${CMAKE_SOURCE_DIR}/tests/generic_fit_target_constraints_run.wio"
)
wio_test_labels(wio_test_generic_fit_target_constraints_dry_run language traceability)

add_wio_success_test(
    wio_test_generic_fit_target_specialization_dry_run
    "${CMAKE_SOURCE_DIR}/tests/generic_fit_target_specialization_run.wio"
)
wio_test_labels(wio_test_generic_fit_target_specialization_dry_run language)

add_wio_success_test(
    wio_test_generic_parameterized_type_parameter_dry_run
    "${CMAKE_SOURCE_DIR}/tests/generic_parameterized_type_parameter_run.wio"
)
wio_test_labels(wio_test_generic_parameterized_type_parameter_dry_run language traceability)

add_wio_success_test(
    wio_test_generic_parameterized_type_parameter_realm_dry_run
    "${CMAKE_SOURCE_DIR}/tests/generic_parameterized_type_parameter_realm_run.wio"
)
wio_test_labels(wio_test_generic_parameterized_type_parameter_realm_dry_run language)

add_wio_success_test(
    wio_test_ref_mut_container_members_dry_run
    "${CMAKE_SOURCE_DIR}/tests/ref_mut_container_members_run.wio"
)
wio_test_labels(wio_test_ref_mut_container_members_dry_run language traceability)

add_wio_success_test(
    wio_test_ref_mut_container_field_members_dry_run
    "${CMAKE_SOURCE_DIR}/tests/ref_mut_container_field_members_run.wio"
)
wio_test_labels(wio_test_ref_mut_container_field_members_dry_run language)

add_wio_success_test(
    wio_test_object_method_outer_const_dry_run
    "${CMAKE_SOURCE_DIR}/tests/object_method_outer_const_run.wio"
)
wio_test_labels(wio_test_object_method_outer_const_dry_run language traceability)

add_wio_success_test(
    wio_test_object_method_outer_const_realm_dry_run
    "${CMAKE_SOURCE_DIR}/tests/object_method_outer_const_realm_run.wio"
)
wio_test_labels(wio_test_object_method_outer_const_realm_dry_run language)

add_wio_success_test(
    wio_test_realm_object_method_calls_dry_run
    "${CMAKE_SOURCE_DIR}/tests/realm_object_method_calls_run.wio"
)
wio_test_labels(wio_test_realm_object_method_calls_dry_run language)

add_wio_success_test(
    wio_test_object_const_access_dry_run
    "${CMAKE_SOURCE_DIR}/tests/object_const_access_run.wio"
)
wio_test_labels(wio_test_object_const_access_dry_run language)

add_wio_success_test(
    wio_test_any_basic_boxing_dry_run
    "${CMAKE_SOURCE_DIR}/tests/any_basic_boxing_run.wio"
)
wio_test_labels(wio_test_any_basic_boxing_dry_run language)

add_wio_success_test(
    wio_test_any_object_fit_dry_run
    "${CMAKE_SOURCE_DIR}/tests/any_object_fit_run.wio"
)
wio_test_labels(wio_test_any_object_fit_dry_run language)

add_wio_success_test(
    wio_test_any_interface_boxing_dry_run
    "${CMAKE_SOURCE_DIR}/tests/any_interface_boxing_run.wio"
)
wio_test_labels(wio_test_any_interface_boxing_dry_run language)

add_wio_success_test(
    wio_test_any_generic_box_fit_dry_run
    "${CMAKE_SOURCE_DIR}/tests/any_generic_box_fit_run.wio"
)
wio_test_labels(wio_test_any_generic_box_fit_dry_run language)

add_wio_success_test(
    wio_test_any_opaque_boundary_dry_run
    "${CMAKE_SOURCE_DIR}/tests/any_opaque_boundary_run.wio"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_any_opaque_boundary_dry_run language)

add_wio_output_test(
    wio_test_object_const_access_run
    "${CMAKE_SOURCE_DIR}/tests/object_const_access_run.wio"
    "27"
    --run
)
wio_test_labels(wio_test_object_const_access_run language)

add_wio_output_test(
    wio_test_any_basic_boxing_run
    "${CMAKE_SOURCE_DIR}/tests/any_basic_boxing_run.wio"
    "any-basic-ok"
    --run
)
wio_test_labels(wio_test_any_basic_boxing_run language)

add_wio_output_test(
    wio_test_any_object_fit_run
    "${CMAKE_SOURCE_DIR}/tests/any_object_fit_run.wio"
    "any-object-ok"
    --run
)
wio_test_labels(wio_test_any_object_fit_run language)

add_wio_output_test(
    wio_test_any_interface_fit_run
    "${CMAKE_SOURCE_DIR}/tests/any_interface_fit_run.wio"
    "any-interface-ok"
    --run
)
wio_test_labels(wio_test_any_interface_fit_run language)

add_wio_output_test(
    wio_test_any_interface_boxing_run
    "${CMAKE_SOURCE_DIR}/tests/any_interface_boxing_run.wio"
    "any-interface-boxing-ok"
    --run
)
wio_test_labels(wio_test_any_interface_boxing_run language)

add_wio_output_test(
    wio_test_any_generic_box_fit_run
    "${CMAKE_SOURCE_DIR}/tests/any_generic_box_fit_run.wio"
    "any-generic-box-ok"
    --run
)
wio_test_labels(wio_test_any_generic_box_fit_run language traceability)

add_wio_output_test(
    wio_test_any_opaque_boundary_run
    "${CMAKE_SOURCE_DIR}/tests/any_opaque_boundary_run.wio"
    "any-opaque-boundary-ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_any_opaque_boundary_run language traceability)

add_wio_output_test(
    wio_test_attribute_declaration_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_declaration_run.wio"
    "attribute-declaration-ok"
    --run
)
wio_test_labels(wio_test_attribute_declaration_run language freeze)

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
wio_test_labels(wio_test_attribute_compact_declaration_run language freeze coherence)

set_tests_properties(
    wio_test_attribute_compact_declaration_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_const_text_defaults_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_const_text_defaults_run.wio"
    "attribute-const-text-defaults-ok"
    --run
)
wio_test_labels(wio_test_attribute_const_text_defaults_run language freeze coherence)

add_wio_output_test(
    wio_test_generic_text_const_parameters_run
    "${CMAKE_SOURCE_DIR}/tests/generic_text_const_parameters_run.wio"
    "generic-text-const-parameters-ok"
    --run
)
wio_test_labels(wio_test_generic_text_const_parameters_run language coherence)

add_wio_output_test(
    wio_test_generic_text_const_values_modules_run
    "${CMAKE_SOURCE_DIR}/tests/modules/generic_text_const_values_root.wio"
    "generic-text-const-modules-ok"
    --run
)
wio_test_labels(wio_test_generic_text_const_values_modules_run language coherence)

set_tests_properties(
    wio_test_generic_text_const_values_modules_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_generic_text_const_parameters_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_attribute_const_text_defaults_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_conflict_valid_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_conflict_valid_run.wio"
    "attribute-conflict-ok"
    --run
)
wio_test_labels(wio_test_attribute_conflict_valid_run language freeze)

add_wio_output_test(
    wio_test_attribute_runtime_reflection_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_runtime_reflection_run.wio"
    "attribute-runtime-reflection-ok"
    --run
)
wio_test_labels(wio_test_attribute_runtime_reflection_run language freeze)

add_wio_output_test(
    wio_test_attribute_bracket_syntax_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_bracket_syntax_run.wio"
    "attribute-bracket-syntax-ok"
    --run
)
wio_test_labels(wio_test_attribute_bracket_syntax_run language freeze)

set_tests_properties(
    wio_test_attribute_bracket_syntax_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_composition_constraints_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_composition_constraints_run.wio"
    "attribute-composition-constraints-ok"
    --run
)
wio_test_labels(wio_test_attribute_composition_constraints_run language freeze)

set_tests_properties(
    wio_test_attribute_composition_constraints_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_processor_contract_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_processor_contract_run.wio"
    "attribute-processor-contract-ok"
    --run
)
wio_test_labels(wio_test_attribute_processor_contract_run language freeze)

set_tests_properties(
    wio_test_attribute_processor_contract_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_compile_time_validator_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_compile_time_validator_run.wio"
    "attribute-compile-time-validator-ok"
    --run
)
wio_test_labels(wio_test_attribute_compile_time_validator_run language freeze)

add_wio_output_test(
    wio_test_attribute_typed_validator_target_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_typed_validator_target_run.wio"
    "attribute-typed-validator-target-ok"
    --run
)
wio_test_labels(wio_test_attribute_typed_validator_target_run language freeze)

set_tests_properties(
    wio_test_attribute_compile_time_validator_run
    wio_test_attribute_typed_validator_target_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_behavioral_pipeline_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_behavioral_pipeline_run.wio"
    "pre>body>post>finally"
    --run
)
wio_test_labels(wio_test_attribute_behavioral_pipeline_run language freeze)

add_wio_output_test(
    wio_test_attribute_typed_receiver_pre_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_typed_receiver_pre_run.wio"
    "attribute-typed-receiver-pre-ok"
    --run
)
wio_test_labels(wio_test_attribute_typed_receiver_pre_run language freeze)

add_wio_output_test(
    wio_test_attribute_behavioral_ordering_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_behavioral_ordering_run.wio"
    "first>second>body"
    --run
)
wio_test_labels(wio_test_attribute_behavioral_ordering_run language freeze)

add_wio_output_test(
    wio_test_attribute_around_pipeline_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_around_pipeline_run.wio"
    "around>pre>body>post>around-exit"
    --run
)
wio_test_labels(wio_test_attribute_around_pipeline_run language freeze)

add_wio_output_test(
    wio_test_attribute_around_skip_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_around_skip_run.wio"
    "attribute-around-skip-ok"
    --run
)
wio_test_labels(wio_test_attribute_around_skip_run language freeze)

add_wio_output_test(
    wio_test_attribute_around_result_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_around_result_run.wio"
    "attribute-around-result-ok"
    --run
)
wio_test_labels(wio_test_attribute_around_result_run language freeze)

add_wio_output_test(
    wio_test_attribute_receiver_guard_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_receiver_guard_run.wio"
    "attribute-receiver-guard-ok"
    --run
)
wio_test_labels(wio_test_attribute_receiver_guard_run language freeze)

add_wio_output_test(
    wio_test_attribute_behavioral_reflection_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_behavioral_reflection_run.wio"
    "attribute-behavioral-reflection-ok"
    --run
)
wio_test_labels(wio_test_attribute_behavioral_reflection_run language freeze)

add_wio_output_test(
    wio_test_attribute_behavioral_async_result_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_behavioral_async_result_run.wio"
    "attribute-behavioral-async-result-ok"
    --run
)
wio_test_labels(wio_test_attribute_behavioral_async_result_run language freeze)

add_wio_output_test(
    wio_test_attribute_checked_derive_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_checked_derive_run.wio"
    "attribute-checked-derive-ok"
    --run
)
wio_test_labels(wio_test_attribute_checked_derive_run language freeze)

add_wio_output_test(
    wio_test_attribute_checked_derive_generic_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_checked_derive_generic_run.wio"
    "attribute-checked-derive-generic-ok"
    --run
)
wio_test_labels(wio_test_attribute_checked_derive_generic_run language freeze)

add_wio_output_test(
    wio_test_attribute_checked_derive_typed_receiver_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_checked_derive_typed_receiver_run.wio"
    "attribute-checked-derive-typed-receiver-ok"
    --run
)
wio_test_labels(wio_test_attribute_checked_derive_typed_receiver_run language freeze)

set_tests_properties(
    wio_test_attribute_behavioral_pipeline_run
    wio_test_attribute_typed_receiver_pre_run
    wio_test_attribute_behavioral_ordering_run
    wio_test_attribute_around_pipeline_run
    wio_test_attribute_around_skip_run
    wio_test_attribute_around_result_run
    wio_test_attribute_receiver_guard_run
    wio_test_attribute_behavioral_reflection_run
    wio_test_attribute_behavioral_async_result_run
    wio_test_attribute_checked_derive_run
    wio_test_attribute_checked_derive_generic_run
    wio_test_attribute_checked_derive_typed_receiver_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_parameter_enum_case_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_parameter_enum_case_run.wio"
    "attribute-parameter-enum-case-ok"
    --run
)
wio_test_labels(wio_test_attribute_parameter_enum_case_run language freeze)

set_tests_properties(
    wio_test_attribute_parameter_enum_case_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_application_system_targets_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_application_system_targets_run.wio"
    "attribute-application-system-targets-ok"
    --run
)
wio_test_labels(wio_test_attribute_application_system_targets_run language freeze)

set_tests_properties(
    wio_test_attribute_application_system_targets_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_success_test(
    wio_test_attribute_builtin_contract_unification
    "${CMAKE_SOURCE_DIR}/tests/attribute_builtin_contract_unification.wio"
)
wio_test_labels(wio_test_attribute_builtin_contract_unification language freeze)

set_tests_properties(
    wio_test_attribute_builtin_contract_unification
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_scoped_activation_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_scoped_activation_run.wio"
    "attribute-scoped-activation-ok"
    --run
)
wio_test_labels(wio_test_attribute_scoped_activation_run language freeze)

set_tests_properties(
    wio_test_attribute_scoped_activation_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_attribute_legacy_equivalence_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_legacy_equivalence_run.wio"
    "attribute-legacy-equivalence-ok"
    --run
)
wio_test_labels(wio_test_attribute_legacy_equivalence_run language freeze)

set_tests_properties(
    wio_test_attribute_legacy_equivalence_run
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

add_wio_output_test(
    wio_test_pipeline_operator_run
    "${CMAKE_SOURCE_DIR}/tests/pipeline_operator_run.wio"
    "pipeline-operator-ok"
    --run
)
wio_test_labels(wio_test_pipeline_operator_run language freeze)

add_wio_output_test(
    wio_test_pipeline_stabilization_run
    "${CMAKE_SOURCE_DIR}/tests/pipeline_stabilization_run.wio"
    "pipeline-stabilization-ok"
    --run
)
wio_test_labels(wio_test_pipeline_stabilization_run language freeze)

set_tests_properties(
    wio_test_pipeline_stabilization_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

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
wio_test_labels(wio_test_match_option_result_patterns_run language freeze)

add_wio_output_test(
    wio_test_match_array_patterns_run
    "${CMAKE_SOURCE_DIR}/tests/match_array_patterns_run.wio"
    "match-array-patterns-ok"
    --run
)
wio_test_labels(wio_test_match_array_patterns_run language freeze)

add_wio_output_test(
    wio_test_match_stabilization_run
    "${CMAKE_SOURCE_DIR}/tests/match_stabilization_run.wio"
    "match-stabilization-ok"
    --run
)
wio_test_labels(wio_test_match_stabilization_run language freeze)

set_tests_properties(
    wio_test_match_stabilization_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_match_array_patterns_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_match_option_result_patterns_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_generic_interface_parameter_override_run
    "${CMAKE_SOURCE_DIR}/tests/generic_interface_parameter_override_run.wio"
    "generic-interface-parameter-override-ok"
    --run
)
wio_test_labels(wio_test_generic_interface_parameter_override_run language)

set_tests_properties(
    wio_test_generic_interface_parameter_override_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_lifecycle_run
    "${CMAKE_SOURCE_DIR}/tests/application_lifecycle_run.wio"
    "application-lifecycle-ok"
    --run
)
wio_test_labels(wio_test_application_lifecycle_run language freeze)

add_wio_output_test(
    wio_test_application_stabilization_run
    "${CMAKE_SOURCE_DIR}/tests/application_stabilization_run.wio"
    "application-stabilization-ok"
    --run
)
wio_test_labels(wio_test_application_stabilization_run language freeze)

set_tests_properties(
    wio_test_application_stabilization_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_delta_context_run
    "${CMAKE_SOURCE_DIR}/tests/application_delta_context_run.wio"
    "application-delta-context-ok"
    --run
)
wio_test_labels(wio_test_application_delta_context_run language freeze)

set_tests_properties(
    wio_test_application_delta_context_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_schedule_run
    "${CMAKE_SOURCE_DIR}/tests/application_schedule_run.wio"
    "application-schedule-ok"
    --run
)
wio_test_labels(wio_test_application_schedule_run language freeze)

set_tests_properties(
    wio_test_application_schedule_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_attribute_model_run
    "${CMAKE_SOURCE_DIR}/tests/application_attribute_model_run.wio"
    "application-attribute-model-ok"
    --run
)
wio_test_labels(wio_test_application_attribute_model_run language freeze)

set_tests_properties(
    wio_test_application_attribute_model_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_mixed_model_run
    "${CMAKE_SOURCE_DIR}/tests/application_mixed_model_run.wio"
    "application-mixed-model-ok"
    --run
)
wio_test_labels(wio_test_application_mixed_model_run language freeze)

set_tests_properties(
    wio_test_application_mixed_model_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_composed_attribute_run
    "${CMAKE_SOURCE_DIR}/tests/application_composed_attribute_run.wio"
    "application-composed-attribute-ok"
    --run
)
wio_test_labels(wio_test_application_composed_attribute_run language freeze)

set_tests_properties(
    wio_test_application_composed_attribute_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_resource_schedule_run
    "${CMAKE_SOURCE_DIR}/tests/application_resource_schedule_run.wio"
    "application-resource-schedule-ok"
    --run
)
wio_test_labels(wio_test_application_resource_schedule_run language freeze)

set_tests_properties(
    wio_test_application_resource_schedule_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_application_lifecycle_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_any_value_reference_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/any_value_reference_semantics_run.wio"
    "any-value-reference-semantics-ok"
    --run
)
wio_test_labels(wio_test_any_value_reference_semantics_run language traceability)

add_wio_output_test(
    wio_show_tokens_alias_stream
    "${CMAKE_SOURCE_DIR}/tests/test_use_alias.wio"
    "kwAs 'as'"
    --show-tokens
    --dry-run
)
wio_test_labels(wio_show_tokens_alias_stream language)

add_wio_output_test(
    wio_show_tokens_realm_stream
    "${CMAKE_SOURCE_DIR}/tests/realm_basic.wio"
    "kwRealm 'realm'"
    --show-tokens
    --dry-run
)
wio_test_labels(wio_show_tokens_realm_stream language)

add_wio_output_test(
    wio_test_const_string_text_run
    "${CMAKE_SOURCE_DIR}/tests/const_string_text_run.wio"
    "const-string-text-ok"
    --run
)
wio_test_labels(wio_test_const_string_text_run language traceability coherence)

add_wio_output_test(
    wio_test_const_global_type_visibility_run
    "${CMAKE_SOURCE_DIR}/tests/const_global_type_visibility_run.wio"
    "const-global-type-visibility-ok"
    --run
)
wio_test_labels(wio_test_const_global_type_visibility_run language traceability coherence)

set_tests_properties(
    wio_test_const_string_text_run
    wio_test_const_global_type_visibility_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_application_async_main_run
    "${CMAKE_SOURCE_DIR}/tests/application_async_main_run.wio"
    "application-async-main-ok"
    --run
)
wio_test_labels(wio_test_application_async_main_run language freeze)

add_wio_output_test(
    wio_test_lambda_capture_value_run
    "${CMAKE_SOURCE_DIR}/tests/lambda_capture_value_run.wio"
    "lambda-capture-value-ok"
    --run
)
wio_test_labels(wio_test_lambda_capture_value_run language freeze)

set_tests_properties(
    wio_test_lambda_capture_value_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_application_async_main_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_unicode_text_literal_run
    "${CMAKE_SOURCE_DIR}/tests/unicode_text_literal_run.wio"
    "unicode-text-ok"
    --run
)
wio_test_labels(wio_test_unicode_text_literal_run language coherence)

add_wio_output_test(
    wio_test_unicode_text_reflection_run
    "${CMAKE_SOURCE_DIR}/tests/unicode_text_reflection_run.wio"
    "unicode-text-reflection-ok"
    --run
)
wio_test_labels(wio_test_unicode_text_reflection_run language coherence)

set_tests_properties(
    wio_test_unicode_text_literal_run
    wio_test_unicode_text_reflection_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)
