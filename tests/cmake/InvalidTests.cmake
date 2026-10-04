# Invalid test registrations. Included in the app directory scope.

add_wio_failure_test_clean(
    wio_invalid_imported_parser_error_no_cascade
    "${CMAKE_SOURCE_DIR}/tests/invalid/imported_parser_error_no_cascade.wio"
    "Unexpected token: expected identifier"
    "Undefined symbol|No matching overload|Generated C\\+\\+ output:"
    --module-dir
    "${CMAKE_SOURCE_DIR}/tests/modules"
)
wio_test_labels(wio_invalid_imported_parser_error_no_cascade invalid)

add_wio_failure_test_clean(
    wio_invalid_semantic_poison_no_cascade
    "${CMAKE_SOURCE_DIR}/tests/invalid/semantic_poison_no_cascade.wio"
    "Unknown type: 'MissingType'"
    "Type mismatch in binary operation|Argument mismatch at index|No matching overload|Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_semantic_poison_no_cascade invalid)

add_wio_failure_test_clean(
    wio_invalid_overload_poison_no_cascade
    "${CMAKE_SOURCE_DIR}/tests/invalid/overload_poison_no_cascade.wio"
    "Undefined symbol: 'missingArgument'"
    "No matching overload|Ambiguous function call|Called expression is undefined|Called expression is not a function|Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_overload_poison_no_cascade invalid)

add_wio_failure_test(
    wio_invalid_generic_partial_specialization_structural_ambiguous
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_partial_specialization_structural_ambiguous.wio"
    "Ambiguous partial specialization"
)
wio_test_labels(wio_invalid_generic_partial_specialization_structural_ambiguous invalid coherence)

set_tests_properties(
    wio_invalid_generic_partial_specialization_structural_ambiguous
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_extension_generic_constraint_reject
    "${CMAKE_SOURCE_DIR}/tests/invalid/extension_generic_constraint_reject.wio"
    "rejects type arguments"
)
wio_test_labels(wio_invalid_extension_generic_constraint_reject invalid coherence)

set_tests_properties(
    wio_invalid_extension_generic_constraint_reject
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_match_enum_non_exhaustive
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_enum_non_exhaustive.wio"
    "must cover every member or include an 'assumed' fallback"
)
wio_test_labels(wio_invalid_match_enum_non_exhaustive invalid freeze coherence)

set_tests_properties(
    wio_invalid_match_enum_non_exhaustive
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_match_enum_duplicate_unguarded
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_enum_duplicate_unguarded.wio"
    "Unreachable enum match case"
)
wio_test_labels(wio_invalid_match_enum_duplicate_unguarded invalid freeze coherence)

set_tests_properties(
    wio_invalid_match_enum_duplicate_unguarded
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_match_literal_guard_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_literal_guard_type_mismatch.wio"
    "Match guards must have type 'bool'"
)
wio_test_labels(wio_invalid_match_literal_guard_type_mismatch invalid freeze coherence)

set_tests_properties(
    wio_invalid_match_literal_guard_type_mismatch
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_match_assumed_guard
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_assumed_guard.wio"
    "The final 'assumed' match case cannot have a guard"
)
wio_test_labels(wio_invalid_match_assumed_guard invalid freeze coherence)

set_tests_properties(
    wio_invalid_match_assumed_guard
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_generic_where_constraint_reject
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_where_constraint_reject.wio"
    "rejects type arguments <string> because of @Apply constraints"
)
wio_test_labels(wio_invalid_generic_where_constraint_reject invalid)

add_wio_failure_test(
    wio_invalid_generic_where_unknown_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_where_unknown_parameter.wio"
    "Where clause references an unknown generic parameter 'U'"
)
wio_test_labels(wio_invalid_generic_where_unknown_parameter invalid)

add_wio_failure_test(
    wio_invalid_generic_where_conjunction_reject
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_where_conjunction_reject.wio"
    "rejects type arguments <u32> because of @Apply constraints"
)
wio_test_labels(wio_invalid_generic_where_conjunction_reject invalid)

add_wio_failure_test(
    wio_invalid_generic_invariance_reject
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_invariance_reject.wio"
    "Argument mismatch at index 0: Expected 'Holder<Animal>', but got 'Holder<Dog>'"
)
wio_test_labels(wio_invalid_generic_invariance_reject invalid)

add_wio_failure_test(
    wio_invalid_generic_partial_specialization_ambiguous
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_partial_specialization_ambiguous.wio"
    "Ambiguous partial specialization for 'Choice\\(i32, string\\)'"
)
wio_test_labels(wio_invalid_generic_partial_specialization_ambiguous invalid)

add_wio_failure_test(
    wio_invalid_generic_default_non_trailing
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_default_non_trailing.wio"
    "Generic parameters without defaults cannot follow a defaulted parameter"
)
wio_test_labels(wio_invalid_generic_default_non_trailing invalid)

add_wio_failure_test(
    wio_invalid_generic_default_forward_reference
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_default_forward_reference.wio"
    "defaults may only use earlier parameters"
)
wio_test_labels(wio_invalid_generic_default_forward_reference invalid)

add_wio_failure_test(
    wio_invalid_generic_const_argument_kind_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_argument_kind_mismatch.wio"
    "expects a compile-time value"
)
wio_test_labels(wio_invalid_generic_const_argument_kind_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_const_function_argument_kind_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_function_argument_kind_mismatch.wio"
    "expects a compile-time value"
)
wio_test_labels(wio_invalid_generic_const_function_argument_kind_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_const_default_forward_reference
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_default_forward_reference.wio"
    "defaults may only use earlier parameters"
)
wio_test_labels(wio_invalid_generic_const_default_forward_reference invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_direct_index_out_of_range
    "${CMAKE_SOURCE_DIR}/tests/direct_index_out_of_range_runtime_failure.wio"
    "Runtime Error: Index access index 3 is out of range for size 3\\."
    "stl_vector|Assertion '__n < this->size\\(\\)' failed"
    --run
)
wio_test_labels(wio_runtime_direct_index_out_of_range invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_runtime_string_to_i32_invalid
    "${CMAKE_SOURCE_DIR}/tests/string_to_i32_runtime_failure.wio"
    "Runtime Error: Cannot parse 'not-a-number' as i32: invalid format"
    "segmentation fault|Assertion|std::invalid_argument"
    --run
)
wio_test_labels(wio_runtime_string_to_i32_invalid invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_runtime_native_exception_boundary
    "${CMAKE_SOURCE_DIR}/tests/native/native_exception_boundary.wio"
    "Runtime Error: Native call 'native_exception_boundary::ThrowStandardException' failed: native-boom"
    "terminate called|std::runtime_error|Unhandled native exception"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_runtime_native_exception_boundary invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_panic_runs_cleanup
    "${CMAKE_SOURCE_DIR}/tests/panic_cleanup_runtime_failure.wio"
    "panic-cleanup-ran"
    "terminate called|Aborted|segmentation fault"
    --run
)
wio_test_labels(wio_runtime_panic_runs_cleanup invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_std_resource_use_after_dispose
    "${CMAKE_SOURCE_DIR}/tests/std_resource_use_after_dispose_runtime_failure.wio"
    "Runtime Error: Assertion failed: resource::Owned<T>.Value cannot access a disposed resource"
    "terminate called|segmentation fault"
    --run
)
wio_test_labels(wio_runtime_std_resource_use_after_dispose invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_integer_divide_by_zero
    "${CMAKE_SOURCE_DIR}/tests/integer_divide_by_zero_runtime_failure.wio"
    "Runtime Error: Integer division by zero\."
    "floating point exception|SIGFPE|Assertion"
    --run
)
wio_test_labels(wio_runtime_integer_divide_by_zero invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_enum_from_value_unknown
    "${CMAKE_SOURCE_DIR}/tests/enum_from_value_runtime_failure.wio"
    "Runtime Error: Enum value is outside the declared value set\."
    "segmentation fault|Assertion|SIG"
    --run
)
wio_test_labels(wio_runtime_enum_from_value_unknown invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_std_assert_not_implemented_yet
    "${CMAKE_SOURCE_DIR}/tests/std_assert_not_implemented_yet_runtime_failure.wio"
    "Runtime Error: Assertion failed: Not implemented yet: vector swizzle assignment"
    "Unhandled exception|segmentation fault|Assertion '__n < this->size"
    --run
)
wio_test_labels(wio_runtime_std_assert_not_implemented_yet invalid)

add_wio_failure_test(
    wio_invalid_generic_const_parameter_bool_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_const_parameter_bool_not_supported.wio"
    "must use an integer, string, or text type"
)
wio_test_labels(wio_invalid_generic_const_parameter_bool_not_supported invalid coherence)

add_wio_failure_test(
    wio_invalid_generic_string_const_array_extent
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_string_const_array_extent.wio"
    "Static array extent must be an integer const generic parameter or integer literal"
)
wio_test_labels(wio_invalid_generic_string_const_array_extent invalid coherence)

add_wio_failure_test(
    wio_invalid_generic_text_const_kind_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_text_const_kind_mismatch.wio"
    "is not compatible with parameter 'Name: string'"
)
wio_test_labels(wio_invalid_generic_text_const_kind_mismatch invalid freeze coherence)

add_wio_failure_test(
    wio_invalid_native_string_const_generic_component
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_string_const_generic_component.wio"
    "@Native components support only integer const generic parameters"
)
wio_test_labels(wio_invalid_native_string_const_generic_component invalid coherence)

add_wio_failure_test(
    wio_invalid_native_text_const_generic_function
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_text_const_generic_function.wio"
    "@Native functions support only integer const generic parameters"
)
wio_test_labels(wio_invalid_native_text_const_generic_function invalid coherence)

add_wio_failure_test(
    wio_invalid_const_cyclic_dependency
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_cyclic_dependency.wio"
    "Const initializer contains a cyclic const dependency"
)
wio_test_labels(wio_invalid_const_cyclic_dependency invalid coherence)

add_wio_failure_test(
    wio_invalid_const_evaluation_depth_limit
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_evaluation_depth_limit.wio"
    "Const initializer exceeds the maximum evaluation depth of 128"
)
wio_test_labels(wio_invalid_const_evaluation_depth_limit invalid coherence)

set_tests_properties(
    wio_invalid_generic_const_parameter_bool_not_supported
    wio_invalid_generic_string_const_array_extent
    wio_invalid_generic_text_const_kind_mismatch
    wio_invalid_native_string_const_generic_component
    wio_invalid_native_text_const_generic_function
    wio_invalid_const_cyclic_dependency
    wio_invalid_const_evaluation_depth_limit
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_text_requires_unicode_literal
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_text_requires_unicode_literal.wio"
    "must be 'text'"
)
wio_test_labels(wio_invalid_attribute_text_requires_unicode_literal invalid freeze coherence)

add_wio_failure_test(
    wio_invalid_attribute_string_rejects_unicode_literal
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_string_rejects_unicode_literal.wio"
    "must be 'string'"
)
wio_test_labels(wio_invalid_attribute_string_rejects_unicode_literal invalid freeze coherence)

set_tests_properties(
    wio_invalid_attribute_text_requires_unicode_literal
    wio_invalid_attribute_string_rejects_unicode_literal
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_named_unknown
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_unknown.wio"
    "has no parameter named 'verb'"
)
wio_test_labels(wio_invalid_attribute_named_unknown invalid freeze coherence)

add_wio_failure_test(
    wio_invalid_attribute_named_duplicate
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_duplicate.wio"
    "parameter 'method' is assigned more than once"
)
wio_test_labels(wio_invalid_attribute_named_duplicate invalid freeze coherence)

add_wio_failure_test(
    wio_invalid_attribute_named_missing_required
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_missing_required.wio"
    "missing required argument 'method'"
)
wio_test_labels(wio_invalid_attribute_named_missing_required invalid freeze coherence)

add_wio_failure_test(
    wio_invalid_attribute_compact_conflict
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_compact_conflict.wio"
    "through conflict group 'serialization'"
)
wio_test_labels(wio_invalid_attribute_compact_conflict invalid freeze coherence)

set_tests_properties(
    wio_invalid_attribute_named_unknown
    wio_invalid_attribute_named_duplicate
    wio_invalid_attribute_named_missing_required
    wio_invalid_attribute_compact_conflict
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_compile_time_validator_rejects
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_compile_time_validator_rejects.wio"
    "callback target is not safe"
)
wio_test_labels(wio_invalid_attribute_compile_time_validator_rejects invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_compile_time_validator_runtime_result
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_compile_time_validator_runtime_result.wio"
    "must fold to a compile-time bool"
)
wio_test_labels(wio_invalid_attribute_compile_time_validator_runtime_result invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_typed_validator_target_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_typed_validator_target_mismatch.wio"
    "requires a target compatible with 'Named'"
)
wio_test_labels(wio_invalid_attribute_typed_validator_target_mismatch invalid freeze)

set_tests_properties(
    wio_invalid_attribute_compile_time_validator_rejects
    wio_invalid_attribute_compile_time_validator_runtime_result
    wio_invalid_attribute_typed_validator_target_mismatch
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_typed_receiver_pre_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_typed_receiver_pre_mismatch.wio"
    "requires an object method target compatible with 'Alive'"
)
wio_test_labels(wio_invalid_attribute_typed_receiver_pre_mismatch invalid freeze)

add_wio_runtime_failure_test(
    wio_invalid_attribute_around_proceed_twice
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_around_proceed_twice.wio"
    "Attribute Proceed may be invoked at most once"
)
wio_test_labels(wio_invalid_attribute_around_proceed_twice invalid freeze)

add_wio_runtime_failure_test(
    wio_invalid_attribute_around_proceed_escaped
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_around_proceed_escaped.wio"
    "Attribute Proceed escaped its Around invocation"
)
wio_test_labels(wio_invalid_attribute_around_proceed_escaped invalid freeze)

add_wio_runtime_failure_test(
    wio_invalid_attribute_around_result_proceed_escaped
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_around_result_proceed_escaped.wio"
    "Attribute Proceed escaped its Around invocation"
)
wio_test_labels(wio_invalid_attribute_around_result_proceed_escaped invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_behavioral_missing_hook
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_behavioral_missing_hook.wio"
    "post processor 'BrokenPost' must declare"
)
wio_test_labels(wio_invalid_attribute_behavioral_missing_hook invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_post_result_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_post_result_type_mismatch.wio"
    "expects result type 'string'"
)
wio_test_labels(wio_invalid_attribute_post_result_type_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_behavioral_non_callable_target
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_behavioral_non_callable_target.wio"
    "can target only functions or methods"
)
wio_test_labels(wio_invalid_attribute_behavioral_non_callable_target invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_receiver_guard_free_function
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_receiver_guard_free_function.wio"
    "requires an object method target"
)
wio_test_labels(wio_invalid_attribute_receiver_guard_free_function invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_boolean_guard_non_unit
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_boolean_guard_non_unit.wio"
    "can skip only a unit-returning function or method"
)
wio_test_labels(wio_invalid_attribute_boolean_guard_non_unit invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_derive_requires_member
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_derive_requires_member.wio"
    "must expose at least one method"
)
wio_test_labels(wio_invalid_attribute_derive_requires_member invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_derive_member_requires_any_receiver
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_derive_member_requires_any_receiver.wio"
    "first parameter is 'any'"
)
wio_test_labels(wio_invalid_attribute_derive_member_requires_any_receiver invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_derive_wrong_target
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_derive_wrong_target.wio"
    "can target only components or objects"
)
wio_test_labels(wio_invalid_attribute_derive_wrong_target invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_derive_typed_target_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_derive_typed_target_mismatch.wio"
    "requires a target compatible with 'Named'"
)
wio_test_labels(wio_invalid_attribute_derive_typed_target_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_derive_processor_requires_default_constructor
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_derive_processor_requires_default_constructor.wio"
    "must be default constructible"
)
wio_test_labels(wio_invalid_attribute_derive_processor_requires_default_constructor invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_derive_member_default_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_derive_member_default_parameter.wio"
    "public parameters have no defaults or packs"
)
wio_test_labels(wio_invalid_attribute_derive_member_default_parameter invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_around_requires_proceed_contract
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_around_requires_proceed_contract.wio"
    "expects result type 'void'"
)
wio_test_labels(wio_invalid_attribute_around_requires_proceed_contract invalid freeze)

set_tests_properties(
    wio_invalid_attribute_typed_receiver_pre_mismatch
    wio_invalid_attribute_around_proceed_twice
    wio_invalid_attribute_around_proceed_escaped
    wio_invalid_attribute_around_result_proceed_escaped
    wio_invalid_attribute_behavioral_missing_hook
    wio_invalid_attribute_post_result_type_mismatch
    wio_invalid_attribute_behavioral_non_callable_target
    wio_invalid_attribute_receiver_guard_free_function
    wio_invalid_attribute_boolean_guard_non_unit
    wio_invalid_attribute_derive_requires_member
    wio_invalid_attribute_derive_member_requires_any_receiver
    wio_invalid_attribute_derive_wrong_target
    wio_invalid_attribute_derive_typed_target_mismatch
    wio_invalid_attribute_derive_processor_requires_default_constructor
    wio_invalid_attribute_derive_member_default_parameter
    wio_invalid_attribute_around_requires_proceed_contract
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_builtin_requires_any_missing
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_builtin_requires_any_missing.wio"
    "requires at least one compatible attribute"
)
wio_test_labels(wio_invalid_attribute_builtin_requires_any_missing invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_builtin_conflict
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_builtin_conflict.wio"
    "conflicts with attribute 'std::attribute::Export'"
)
wio_test_labels(wio_invalid_attribute_builtin_conflict invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_builtin_wrong_target
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_builtin_wrong_target.wio"
    "cannot target 'variable'"
)
wio_test_labels(wio_invalid_attribute_builtin_wrong_target invalid freeze)

set_tests_properties(
    wio_invalid_attribute_builtin_requires_any_missing
    wio_invalid_attribute_builtin_conflict
    wio_invalid_attribute_builtin_wrong_target
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_bracket_empty
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_bracket_empty.wio"
    "An attribute list cannot be empty"
)
wio_test_labels(wio_invalid_attribute_bracket_empty invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_bracket_trailing_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_bracket_trailing_comma.wio"
    "Expected an attribute after ','"
)
wio_test_labels(wio_invalid_attribute_bracket_trailing_comma invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_requires_missing
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_requires_missing.wio"
    "requires attribute 'export_marker'"
)
wio_test_labels(wio_invalid_attribute_requires_missing invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_named_conflict
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_named_conflict.wio"
    "conflicts with attribute 'cached'"
)
wio_test_labels(wio_invalid_attribute_named_conflict invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_composition_cycle
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_composition_cycle.wio"
    "Attribute composition cycle detected"
)
wio_test_labels(wio_invalid_attribute_composition_cycle invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_processor_wrong_phase
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_processor_wrong_phase.wio"
    "must implement exactly one of Validator, DeriveProcessor, PreProcessor, PostProcessor, FinallyProcessor, or AroundProcessor"
)
wio_test_labels(wio_invalid_attribute_processor_wrong_phase invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_ordering_cycle
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_ordering_cycle.wio"
    "Attribute processor ordering cycle detected"
)
wio_test_labels(wio_invalid_attribute_ordering_cycle invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_parameter_wrong_target
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_parameter_wrong_target.wio"
    "cannot target 'parameter'"
)
wio_test_labels(wio_invalid_attribute_parameter_wrong_target invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_requires_any_missing
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_requires_any_missing.wio"
    "requires at least one compatible attribute"
)
wio_test_labels(wio_invalid_attribute_requires_any_missing invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_only_with_violation
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_only_with_violation.wio"
    "is not permitted alongside 'command_model'"
)
wio_test_labels(wio_invalid_attribute_only_with_violation invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_exclusive_group
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_exclusive_group.wio"
    "through conflict group 'storage'"
)
wio_test_labels(wio_invalid_attribute_exclusive_group invalid freeze)

add_wio_failure_test(
    wio_invalid_attribute_cardinality_max
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_cardinality_max.wio"
    "requires cardinality 0..2"
)
wio_test_labels(wio_invalid_attribute_cardinality_max invalid freeze)

set_tests_properties(
    wio_invalid_attribute_bracket_empty
    wio_invalid_attribute_bracket_trailing_comma
    wio_invalid_attribute_requires_missing
    wio_invalid_attribute_named_conflict
    wio_invalid_attribute_composition_cycle
    wio_invalid_attribute_processor_wrong_phase
    wio_invalid_attribute_ordering_cycle
    wio_invalid_attribute_parameter_wrong_target
    wio_invalid_attribute_requires_any_missing
    wio_invalid_attribute_only_with_violation
    wio_invalid_attribute_exclusive_group
    wio_invalid_attribute_cardinality_max
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(wio_invalid_attribute_using_requires_scoped
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_using_requires_scoped.wio"
    "cannot be activated with 'using'")
wio_test_labels(wio_invalid_attribute_using_requires_scoped invalid freeze)

add_wio_failure_test(wio_invalid_attribute_scoped_conflict
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_scoped_conflict.wio"
    "through conflict group 'serialization'")
wio_test_labels(wio_invalid_attribute_scoped_conflict invalid freeze)

add_wio_failure_test(wio_invalid_attribute_with_empty
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_with_empty.wio"
    "Expected an attribute name after 'with'")
wio_test_labels(wio_invalid_attribute_with_empty invalid freeze)

add_wio_failure_test(wio_invalid_attribute_with_trailing_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_with_trailing_comma.wio"
    "Expected an attribute name after ',' in a 'with' clause")
wio_test_labels(wio_invalid_attribute_with_trailing_comma invalid freeze)

add_wio_failure_test(wio_invalid_attribute_with_duplicate_clause
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_with_duplicate_clause.wio"
    "only one postfix 'with' clause")
wio_test_labels(wio_invalid_attribute_with_duplicate_clause invalid freeze)

set_tests_properties(
    wio_invalid_attribute_using_requires_scoped
    wio_invalid_attribute_scoped_conflict
    wio_invalid_attribute_with_empty
    wio_invalid_attribute_with_trailing_comma
    wio_invalid_attribute_with_duplicate_clause
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_conflict
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_conflict.wio"
    "through conflict group 'serialization'"
)
wio_test_labels(wio_invalid_attribute_conflict invalid freeze)

set_tests_properties(
    wio_invalid_attribute_conflict
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_wrong_target
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_wrong_target.wio"
    "cannot target 'component'"
)
wio_test_labels(wio_invalid_attribute_wrong_target invalid freeze)

set_tests_properties(
    wio_invalid_attribute_wrong_target
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_attribute_argument_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_argument_type.wio"
    "must be 'string'"
)
wio_test_labels(wio_invalid_attribute_argument_type invalid freeze)

set_tests_properties(
    wio_invalid_attribute_argument_type
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_pipeline_requires_callable
    "${CMAKE_SOURCE_DIR}/tests/invalid/pipeline_requires_callable.wio"
    "Pipeline target is not callable"
)
wio_test_labels(wio_invalid_pipeline_requires_callable invalid freeze)

set_tests_properties(
    wio_invalid_pipeline_requires_callable
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(wio_invalid_match_guard_after_unguarded
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_guard_after_unguarded.wio"
    "earlier unguarded case already covers it")
wio_test_labels(wio_invalid_match_guard_after_unguarded invalid freeze)

add_wio_failure_test(wio_invalid_match_array_guard_after_unguarded
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_array_guard_after_unguarded.wio"
    "Unreachable 'array-length' match pattern")
wio_test_labels(wio_invalid_match_array_guard_after_unguarded invalid freeze)

add_wio_failure_test(wio_invalid_match_duplicate_binding
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_duplicate_binding.wio"
    "Pattern binding 'value' is duplicated")
wio_test_labels(wio_invalid_match_duplicate_binding invalid freeze)

add_wio_failure_test(wio_invalid_match_binding_is_immutable
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_binding_is_immutable.wio"
    "immutable")
wio_test_labels(wio_invalid_match_binding_is_immutable invalid freeze)

set_tests_properties(
    wio_invalid_match_guard_after_unguarded
    wio_invalid_match_array_guard_after_unguarded
    wio_invalid_match_duplicate_binding
    wio_invalid_match_binding_is_immutable
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_match_array_pattern_wrong_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_array_pattern_wrong_type.wio"
    "Array destructuring patterns require an array value"
)
wio_test_labels(wio_invalid_match_array_pattern_wrong_type invalid freeze)

set_tests_properties(
    wio_invalid_match_array_pattern_wrong_type
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_match_pattern_wrong_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_pattern_wrong_type.wio"
    "requires a matching std::Option or std::Result"
)
wio_test_labels(wio_invalid_match_pattern_wrong_type invalid freeze)

set_tests_properties(
    wio_invalid_match_pattern_wrong_type
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_std_serialization_codec_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/std_serialization_codec_type_mismatch.wio"
    "No matching overload for 'Encode' with arguments"
)
wio_test_labels(wio_invalid_std_serialization_codec_type_mismatch invalid freeze)

add_wio_non_dry_failure_test_clean(
    wio_runtime_application_start_rollback
    "${CMAKE_SOURCE_DIR}/tests/application_start_rollback_runtime_failure.wio"
    "exec-ready-start exec-failing-start exec-ready-close exec-application-close"
    "exec-failing-close-unexpected|terminate called|segmentation fault"
    --run
)
wio_test_labels(wio_runtime_application_start_rollback invalid freeze)

add_wio_failure_test(wio_invalid_application_update_delta_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_update_delta_type.wio"
    "Application update delta parameter must have type f64")
wio_test_labels(wio_invalid_application_update_delta_type invalid freeze)

add_wio_failure_test(wio_invalid_system_start_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/system_start_parameter.wio"
    "Only system 'on update' accepts parameters")
wio_test_labels(wio_invalid_system_start_parameter invalid)

add_wio_failure_test(wio_invalid_application_schedule_cycle
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_schedule_cycle.wio"
    "schedule dependencies contain a cycle")
wio_test_labels(wio_invalid_application_schedule_cycle invalid freeze)

add_wio_failure_test(wio_invalid_application_schedule_unknown_system
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_schedule_unknown_system.wio"
    "runs unknown application system 'missing'")
wio_test_labels(wio_invalid_application_schedule_unknown_system invalid freeze)

add_wio_failure_test(wio_invalid_application_fixed_stage_frequency
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_fixed_stage_frequency.wio"
    "requires an 'at <frequency> hz' clause")
wio_test_labels(wio_invalid_application_fixed_stage_frequency invalid freeze)

add_wio_failure_test(wio_invalid_application_schedule_unknown_resource
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_schedule_unknown_resource.wio"
    "borrows unknown application resource 'world'")
wio_test_labels(wio_invalid_application_schedule_unknown_resource invalid freeze)

add_wio_failure_test(wio_invalid_system_update_owned_resource_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/system_update_owned_resource_parameter.wio"
    "System update resource parameters must use ref or view")
wio_test_labels(wio_invalid_system_update_owned_resource_parameter invalid)

add_wio_failure_test(wio_invalid_application_attribute_worker_rejected
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_attribute_worker_rejected.wio"
    "Worker.*reserved until ref/view conflict analysis")
wio_test_labels(wio_invalid_application_attribute_worker_rejected invalid freeze)

add_wio_failure_test(wio_invalid_application_attribute_unknown_dependency
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_attribute_unknown_dependency.wio"
    "depends on unknown stage 'MissingStage'")
wio_test_labels(wio_invalid_application_attribute_unknown_dependency invalid freeze)

add_wio_failure_test(wio_invalid_application_attribute_fixed_frequency
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_attribute_fixed_frequency.wio"
    "Fixed.*frequency must be greater than zero")
wio_test_labels(wio_invalid_application_attribute_fixed_frequency invalid freeze)

add_wio_failure_test(wio_invalid_system_application_schedule_attribute
    "${CMAKE_SOURCE_DIR}/tests/invalid/system_application_schedule_attribute.wio"
    "schedule application functions, not system functions")
wio_test_labels(wio_invalid_system_application_schedule_attribute invalid freeze)

add_wio_failure_test(wio_invalid_application_attribute_multiple_lifecycle
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_attribute_multiple_lifecycle.wio"
    "may declare only one of")
wio_test_labels(wio_invalid_application_attribute_multiple_lifecycle invalid freeze)

add_wio_failure_test(wio_invalid_application_attribute_start_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_attribute_start_parameter.wio"
    "Application start and close functions do not accept parameters")
wio_test_labels(wio_invalid_application_attribute_start_parameter invalid freeze)

add_wio_failure_test(wio_invalid_application_attribute_update_return
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_attribute_update_return.wio"
    "Application lifecycle and scheduled functions must return unit")
wio_test_labels(wio_invalid_application_attribute_update_return invalid freeze)

add_wio_failure_test(wio_invalid_system_attribute_update_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/system_attribute_update_type.wio"
    "The first system update parameter must have type f64")
wio_test_labels(wio_invalid_system_attribute_update_type invalid freeze)

add_wio_failure_test(wio_invalid_application_with_entry
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_with_entry.wio"
    "cannot define both application 'Tool' and an ordinary Entry")
wio_test_labels(wio_invalid_application_with_entry invalid freeze)

add_wio_failure_test(wio_invalid_application_multiple_roots
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_multiple_roots.wio"
    "may declare only one application root")
wio_test_labels(wio_invalid_application_multiple_roots invalid freeze)

add_wio_failure_test(wio_invalid_application_nested_realm
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_nested_realm.wio"
    "application root must be declared at module top level")
wio_test_labels(wio_invalid_application_nested_realm invalid freeze)

add_wio_failure_test(wio_invalid_application_missing_update
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_missing_update.wio"
    "must declare exactly one 'on update' handler")
wio_test_labels(wio_invalid_application_missing_update invalid freeze)

add_wio_failure_test(wio_invalid_application_duplicate_handler
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_duplicate_handler.wio"
    "handler 'update' is declared more than once")
wio_test_labels(wio_invalid_application_duplicate_handler invalid freeze)

add_wio_failure_test(wio_invalid_application_handler_parameters
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_handler_parameters.wio"
    "Only application 'on update' accepts a delta parameter")
wio_test_labels(wio_invalid_application_handler_parameters invalid freeze)

add_wio_failure_test(wio_invalid_system_duplicate_handler
    "${CMAKE_SOURCE_DIR}/tests/invalid/system_duplicate_handler.wio"
    "System handler 'update' is declared more than once")
wio_test_labels(wio_invalid_system_duplicate_handler invalid)

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

add_wio_failure_test(
    wio_invalid_interface_method_body
    "${CMAKE_SOURCE_DIR}/tests/invalid/interface_method_body.wio"
    "Interface methods cannot have a body"
)
wio_test_labels(wio_invalid_interface_method_body invalid)

add_wio_failure_test_clean(
    wio_invalid_non_void_when_without_else
    "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_when_without_else.wio"
    "Functions with a return value must provide an 'else' fallback for 'when' guards"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_non_void_when_without_else invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_non_void_missing_return
    "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_missing_return.wio"
    "Non-void function 'Broken' must return a value on all control-flow paths\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_non_void_missing_return invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_non_void_if_missing_return
    "${CMAKE_SOURCE_DIR}/tests/invalid/non_void_if_missing_return.wio"
    "Non-void function 'Broken' must return a value on all control-flow paths\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_non_void_if_missing_return invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_array_index_requires_integer
    "${CMAKE_SOURCE_DIR}/tests/invalid/array_index_requires_integer.wio"
    "Array, string, and text indices must be integer values\\."
    "stl_vector|Unhandled compiler failure"
)
wio_test_labels(wio_invalid_array_index_requires_integer invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_static_array_index_out_of_range
    "${CMAKE_SOURCE_DIR}/tests/invalid/static_array_index_out_of_range.wio"
    "Index 2 is out of range for compile-time array size 2\\."
    "stl_vector|Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_static_array_index_out_of_range invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_generic_fit_target_without_supported_constraint
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_fit_target_without_supported_constraint.wio"
    "The 'fit' operator can only be used with numeric types or object/interface casts\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_generic_fit_target_without_supported_constraint invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_any_arithmetic_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/any_arithmetic_not_supported.wio"
    "Any values support only assignment, the 'is' operator, the 'fit' operator, and equality comparisons with null\\. Operator '\\+' is not supported\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_any_arithmetic_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_any_non_null_equality_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/any_non_null_equality_not_supported.wio"
    "Any values support only assignment, the 'is' operator, the 'fit' operator, and equality comparisons with null\\. Operator '==' is not supported\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_any_non_null_equality_not_supported invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_operator_overload_invalid_arity
    "${CMAKE_SOURCE_DIR}/tests/invalid/operator_overload_invalid_arity.wio"
    "Invalid member operator overload arity for the selected operator\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_operator_overload_invalid_arity invalid)

add_wio_failure_test_clean(
    wio_invalid_read_requires_reference
    "${CMAKE_SOURCE_DIR}/tests/invalid/read_requires_reference.wio"
    "The 'deref' operator requires a readable reference\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_read_requires_reference invalid)

add_wio_failure_test_clean(
    wio_invalid_operator_subscript_assignment_requires_ref
    "${CMAKE_SOURCE_DIR}/tests/invalid/operator_subscript_assignment_requires_ref.wio"
    "Subscript assignment requires operator '\\[\\]' to return a mutable reference\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_operator_subscript_assignment_requires_ref invalid)

add_wio_failure_test_clean(
    wio_invalid_operator_call_requires_member
    "${CMAKE_SOURCE_DIR}/tests/invalid/operator_call_requires_member.wio"
    "Call operator overloads must be declared as member functions\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_operator_call_requires_member invalid)

add_wio_failure_test_clean(
    wio_invalid_opaque_is_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/opaque_is_not_supported.wio"
    "The left side of the 'is' operator must be an object or interface value/reference\\. Got 'opaque'\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_opaque_is_not_supported invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_result_unwrap_requires_result
    "${CMAKE_SOURCE_DIR}/tests/invalid/result_unwrap_requires_result.wio"
    "The '!\\(\\)' unwrap syntax requires the called function to return std::Result<T>\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_result_unwrap_requires_result invalid)

add_wio_failure_test_clean(
    wio_invalid_result_propagate_requires_result_context
    "${CMAKE_SOURCE_DIR}/tests/invalid/result_propagate_requires_result_context.wio"
    "The '\\?\\(\\)' propagation syntax requires the enclosing function to return std::Result<T>\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_result_propagate_requires_result_context invalid)

add_wio_failure_test_clean(
    wio_invalid_result_propagate_requires_result_call
    "${CMAKE_SOURCE_DIR}/tests/invalid/result_propagate_requires_result_call.wio"
    "The '\\?\\(\\)' propagation syntax requires the called function to return std::Result<T>\\."
    "Generated C\\+\\+ output:"
)
wio_test_labels(wio_invalid_result_propagate_requires_result_call invalid)

add_wio_failure_test_clean(
    wio_invalid_match_value_requires_assumed
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_value_requires_assumed.wio"
    "Value-producing match expressions must include an 'assumed' fallback case"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_match_value_requires_assumed invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_match_assumed_must_be_last
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_assumed_must_be_last.wio"
    "The 'assumed' match case must be the last case"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_match_assumed_must_be_last invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_match_duplicate_assumed
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_duplicate_assumed.wio"
    "Match expressions can only contain one 'assumed' case"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_match_duplicate_assumed invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_match_case_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_case_type_mismatch.wio"
    "Match case value type 'string' is not compatible with matched value type 'i32'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_match_case_type_mismatch invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_match_range_subject_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_range_subject_type_mismatch.wio"
    "Range match cases require the matched value to be numeric, but got 'string'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_match_range_subject_type_mismatch invalid freeze)

add_wio_failure_test_clean(
    wio_invalid_match_return_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/match_return_type_mismatch.wio"
    "All value-producing match cases must return compatible types\\. Expected 'i32', but got 'string'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_match_return_type_mismatch invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_when_fallback_return_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/when_fallback_return_type_mismatch.wio"
    "When guard fallback type mismatch! Expected 'i32', but got 'string'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_when_fallback_return_type_mismatch invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_when_condition_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/when_condition_type_mismatch.wio"
    "When guard condition must be a boolean, numeric, or reference type\\. Got: string"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_when_condition_type_mismatch invalid traceability freeze)

add_wio_failure_test_clean(
    wio_invalid_ref_requires_addressable_operand
    "${CMAKE_SOURCE_DIR}/tests/invalid/ref_requires_addressable_operand.wio"
    "The 'ref' operator requires an addressable variable, member, or indexed element"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_ref_requires_addressable_operand invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_ref_immutable_to_mutable_param
    "${CMAKE_SOURCE_DIR}/tests/invalid/ref_immutable_to_mutable_param.wio"
    "expects a mutable reference"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_ref_immutable_to_mutable_param invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_is_left_requires_object_or_interface
    "${CMAKE_SOURCE_DIR}/tests/invalid/is_left_requires_object_or_interface.wio"
    "The left side of the 'is' operator must be an object or interface value/reference\\. Got 'i32'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_is_left_requires_object_or_interface invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_is_right_requires_object_or_interface
    "${CMAKE_SOURCE_DIR}/tests/invalid/is_right_requires_object_or_interface.wio"
    "The right side of the 'is' operator must be an object or interface type"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_is_right_requires_object_or_interface invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_fit_requires_numeric_or_object_interface
    "${CMAKE_SOURCE_DIR}/tests/invalid/fit_requires_numeric_or_object_interface.wio"
    "The 'fit' operator can only be used with numeric types or object/interface casts"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_fit_requires_numeric_or_object_interface invalid traceability)

add_wio_failure_test_clean(
    wio_invalid_if_fit_requires_is_condition
    "${CMAKE_SOURCE_DIR}/tests/invalid/if_fit_requires_is_condition.wio"
    "Pattern matching 'fit' can only be used with the 'is' operator"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_if_fit_requires_is_condition invalid traceability)

add_wio_failure_test(
    wio_invalid_component_inheritance
    "${CMAKE_SOURCE_DIR}/tests/invalid/component_inheritance.wio"
    "Components must be POD \\(Plain Old Data\\) and cannot inherit from objects or interfaces"
)
wio_test_labels(wio_invalid_component_inheritance invalid)

add_wio_failure_test(
    wio_invalid_component_method_not_allowed
    "${CMAKE_SOURCE_DIR}/tests/invalid/component_method_not_allowed.wio"
    "Components cannot define ordinary methods"
)
wio_test_labels(wio_invalid_component_method_not_allowed invalid traceability)

add_wio_failure_test(
    wio_invalid_extension_mutable_receiver_requires_mut
    "${CMAKE_SOURCE_DIR}/tests/invalid/extension_mutable_receiver_requires_mut.wio"
    "Extension method 'AddOne' requires a mutable receiver"
)
wio_test_labels(wio_invalid_extension_mutable_receiver_requires_mut invalid)

add_wio_failure_test_clean(
    wio_invalid_return_local_component_reference
    "${CMAKE_SOURCE_DIR}/tests/invalid/return_local_component_reference.wio"
    "Cannot return a reference borrowed from a local value"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_return_local_component_reference invalid)

add_wio_failure_test_clean(
    wio_invalid_return_local_array_reference
    "${CMAKE_SOURCE_DIR}/tests/invalid/return_local_array_reference.wio"
    "Cannot return a reference borrowed from a local value"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_return_local_array_reference invalid)

add_wio_failure_test_clean(
    wio_invalid_temporary_object_view_escape
    "${CMAKE_SOURCE_DIR}/tests/invalid/temporary_object_view_escape.wio"
    "Cannot store a reference borrowed from a temporary value"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_temporary_object_view_escape invalid)

add_wio_failure_test_clean(
    wio_invalid_native_reference_return_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_reference_return_not_supported.wio"
    "@Native functions cannot return ref/view values"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_native_reference_return_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_floating_modulo_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/floating_modulo_not_supported.wio"
    "Operator '%' requires integer operands"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_floating_modulo_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_null_primitive_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/null_primitive_not_supported.wio"
    "Type 'i32' cannot be initialized with null"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_null_primitive_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_null_component_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/null_component_not_supported.wio"
    "Type 'Point' cannot be initialized with null"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_null_component_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_null_array_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/null_array_not_supported.wio"
    "Type 'i32\\[\\]' cannot be initialized with null"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_null_array_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_null_inference_requires_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/null_inference_requires_type.wio"
    "Cannot infer a variable type from null"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_null_inference_requires_type invalid)

add_wio_failure_test_clean(
    wio_invalid_use_path_empty_segment
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_path_empty_segment.wio"
    "Expected a module name after '::'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_use_path_empty_segment invalid)

add_wio_failure_test(
    wio_invalid_extension_cannot_access_private_member
    "${CMAKE_SOURCE_DIR}/tests/invalid/extension_cannot_access_private_member.wio"
    "Cannot access private member 'value'"
)
wio_test_labels(wio_invalid_extension_cannot_access_private_member invalid)

add_wio_failure_test(
    wio_invalid_object_inherits_component
    "${CMAKE_SOURCE_DIR}/tests/invalid/object_inherits_component.wio"
    "Object 'BadActor' cannot inherit from component 'Position'"
)
wio_test_labels(wio_invalid_object_inherits_component invalid traceability)

add_wio_failure_test(
    wio_invalid_final_object_inheritance
    "${CMAKE_SOURCE_DIR}/tests/invalid/final_object_inheritance.wio"
    "Object 'DerivedFromFinal' cannot inherit from final object 'FinalBase'"
)
wio_test_labels(wio_invalid_final_object_inheritance invalid traceability)

add_wio_failure_test(
    wio_invalid_default_attribute_invalid_access
    "${CMAKE_SOURCE_DIR}/tests/invalid/default_attribute_invalid_access.wio"
    "@Default expects exactly one access modifier: public, private, or protected"
)
wio_test_labels(wio_invalid_default_attribute_invalid_access invalid traceability freeze)

add_wio_failure_test(
    wio_invalid_private_base_member_access
    "${CMAKE_SOURCE_DIR}/tests/invalid/private_base_member_access.wio"
    "Cannot access private member 'secret' declared on 'PrivateBase' from 'PrivateDerived'"
)
wio_test_labels(wio_invalid_private_base_member_access invalid traceability)

add_wio_failure_test(
    wio_invalid_protected_member_access_outside_hierarchy
    "${CMAKE_SOURCE_DIR}/tests/invalid/protected_member_access_outside_hierarchy.wio"
    "Cannot access protected member 'hp' declared on 'GuardedBase' from '<non-object context>'"
)
wio_test_labels(wio_invalid_protected_member_access_outside_hierarchy invalid traceability)

add_wio_failure_test(
    wio_invalid_trust_requires_struct_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/trust_requires_struct_type.wio"
    "@Trust expects object/component/interface type names"
)
wio_test_labels(wio_invalid_trust_requires_struct_type invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_entry_invalid_parameter_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/entry_invalid_parameter_type.wio"
    "Error \\[.*entry_invalid_parameter_type\\.wio:1:1\\]: Entry parameter must be string\\[\\] when present\\. Got 'i32'\\."
    "Compiling failed:"
)
wio_test_labels(wio_invalid_entry_invalid_parameter_type invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_entry_invalid_return_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/entry_invalid_return_type.wio"
    "Error \\[.*entry_invalid_return_type\\.wio:1:1\\]: Entry must return i32 or void\\."
    "Compiling failed:"
)
wio_test_labels(wio_invalid_entry_invalid_return_type invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_entry_too_many_parameters
    "${CMAKE_SOURCE_DIR}/tests/invalid/entry_too_many_parameters.wio"
    "Error \\[.*entry_too_many_parameters\\.wio:1:1\\]: Entry must declare zero parameters or exactly one string\\[\\] parameter\\."
    "Compiling failed:"
)
wio_test_labels(wio_invalid_entry_too_many_parameters invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_entry_missing_body
    "${CMAKE_SOURCE_DIR}/tests/invalid/entry_missing_body.wio"
    "Error \\[.*entry_missing_body\\.wio:1:1\\]: Entry functions must define a Wio body\\."
    "Compiling failed:"
)
wio_test_labels(wio_invalid_entry_missing_body invalid traceability)

add_wio_non_dry_failure_test_clean(
    wio_invalid_entry_method_not_top_level
    "${CMAKE_SOURCE_DIR}/tests/invalid/entry_method_not_top_level.wio"
    "Error \\[.*entry_method_not_top_level\\.wio:2:5\\]: Entry is currently supported only for top-level functions\\."
    "Compiling failed:"
)
wio_test_labels(wio_invalid_entry_method_not_top_level invalid traceability)

add_wio_failure_test(
    wio_invalid_multiple_object_inheritance
    "${CMAKE_SOURCE_DIR}/tests/invalid/multiple_object_inheritance.wio"
    "cannot inherit from multiple objects/components"
)
wio_test_labels(wio_invalid_multiple_object_inheritance invalid)

add_wio_failure_test(
    wio_invalid_readonly_external_mutation
    "${CMAKE_SOURCE_DIR}/tests/invalid/readonly_external_mutation.wio"
    "Cannot modify @Readonly member 'hp' from outside its object"
)
wio_test_labels(wio_invalid_readonly_external_mutation invalid)

add_wio_failure_test(
    wio_invalid_single_file_ignores_user_imports
    "${CMAKE_SOURCE_DIR}/tests/modules/single_file_root.wio"
    "Undefined symbol: 'GetValue'"
    --single-file
)
wio_test_labels(wio_invalid_single_file_ignores_user_imports invalid)

add_wio_failure_test(
    wio_invalid_no_builtin_ignores_stdlib_imports
    "${CMAKE_SOURCE_DIR}/tests/test_use_alias.wio"
    "Undefined symbol: 'console'"
    --no-builtin
)
wio_test_labels(wio_invalid_no_builtin_ignores_stdlib_imports invalid)

add_wio_failure_test(
    wio_invalid_use_path_missing_module_name
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_path_missing_module_name.wio"
    "Unfinished use statement\\. Use statements should finish with a module name\\."
)
wio_test_labels(wio_invalid_use_path_missing_module_name invalid)

add_wio_failure_test(
    wio_invalid_use_missing_user_module
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_missing_user_module.wio"
    "Module file was not found: missing_gameplay\\.wio"
)
wio_test_labels(wio_invalid_use_missing_user_module invalid traceability)

add_wio_failure_test(
    wio_invalid_use_missing_std_module
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_missing_std_module.wio"
    "Module file was not found: missing_std_module\\.wio"
)
wio_test_labels(wio_invalid_use_missing_std_module invalid traceability)

add_wio_failure_test(
    wio_invalid_use_alias_conflicts_with_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_alias_conflicts_with_symbol.wio"
    "Symbol 'console' already exists and cannot be used as an import alias\\."
)
wio_test_labels(wio_invalid_use_alias_conflicts_with_symbol invalid traceability)

add_wio_failure_test(
    wio_invalid_use_alias_hides_unqualified_root_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_alias_hides_unqualified_root_symbol.wio"
    "Undefined symbol: 'GetValue'"
    --module-dir
    "${CMAKE_SOURCE_DIR}/tests/modules"
)
wio_test_labels(wio_invalid_use_alias_hides_unqualified_root_symbol invalid traceability)

add_wio_failure_test(
    wio_invalid_use_without_direct_import_keeps_namespaced_access
    "${CMAKE_SOURCE_DIR}/tests/invalid/use_without_direct_import_keeps_namespaced_access.wio"
    "Undefined symbol: 'Abs'"
)
wio_test_labels(wio_invalid_use_without_direct_import_keeps_namespaced_access invalid traceability)

add_wio_failure_test(
    wio_invalid_realm_conflicts_with_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/realm_conflicts_with_symbol.wio"
    "Symbol 'gameplay' already exists and is not a realm\\."
)
wio_test_labels(wio_invalid_realm_conflicts_with_symbol invalid traceability)

add_wio_failure_test(
    wio_invalid_function_call_missing_argument
    "${CMAKE_SOURCE_DIR}/tests/invalid/function_call_missing_argument.wio"
    "Expected function call argument after ','."
)
wio_test_labels(wio_invalid_function_call_missing_argument invalid)

add_wio_failure_test(
    wio_invalid_let_without_type_or_initializer
    "${CMAKE_SOURCE_DIR}/tests/invalid/let_without_type_or_initializer.wio"
    "A variable without an initializer must have an explicit type."
)
wio_test_labels(wio_invalid_let_without_type_or_initializer invalid traceability)

add_wio_failure_test(
    wio_invalid_conditional_operator_requires_bool
    "${CMAKE_SOURCE_DIR}/tests/invalid/conditional_operator_requires_bool.wio"
    "Conditional operator condition must be bool, but got 'i32'\\."
)
wio_test_labels(wio_invalid_conditional_operator_requires_bool invalid)

add_wio_failure_test(
    wio_invalid_conditional_operator_branch_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/conditional_operator_branch_type_mismatch.wio"
    "Conditional operator branches must have compatible types\\. Got 'i32' and 'string'\\."
)
wio_test_labels(wio_invalid_conditional_operator_branch_type_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_const_without_initializer
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_without_initializer.wio"
    "Constants must be initialized."
)
wio_test_labels(wio_invalid_const_without_initializer invalid traceability)

add_wio_failure_test(
    wio_invalid_const_interpolated_text_not_folded
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_interpolated_text_not_folded.wio"
    "Const initializer must be a compile-time expression and may reference only other const declarations."
)
wio_test_labels(wio_invalid_const_interpolated_text_not_folded invalid traceability coherence)

add_wio_failure_test(
    wio_invalid_const_from_non_const_identifier
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_from_non_const_identifier.wio"
    "Const initializer must be a compile-time expression and may reference only other const declarations."
)
wio_test_labels(wio_invalid_const_from_non_const_identifier invalid traceability)

add_wio_failure_test(
    wio_invalid_const_function_call_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_function_call_not_supported.wio"
    "Const initializer must be a compile-time expression and may reference only other const declarations."
)
wio_test_labels(wio_invalid_const_function_call_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_const_array_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/const_array_not_supported.wio"
    "Const declarations support scalar, string, text, enum, and flagset values. Got 'i32\\[\\]'."
)
wio_test_labels(wio_invalid_const_array_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_default_parameter_non_trailing
    "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_non_trailing.wio"
    "Parameters with default values must be trailing."
)
wio_test_labels(wio_invalid_default_parameter_non_trailing invalid traceability)

add_wio_failure_test(
    wio_invalid_default_parameter_type_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_type_mismatch.wio"
    "Default argument type mismatch for parameter 'x'. Expected 'i32', but got 'string'."
)
wio_test_labels(wio_invalid_default_parameter_type_mismatch invalid traceability freeze)

add_wio_failure_test(
    wio_invalid_default_parameter_declaration_only_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_declaration_only_not_supported.wio"
    "Default parameters are currently supported only on functions with Wio bodies or on @Native declarations."
)
wio_test_labels(wio_invalid_default_parameter_declaration_only_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_default_parameter_entry_not_allowed
    "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_entry_not_allowed.wio"
    "Entry cannot declare default parameters."
)
wio_test_labels(wio_invalid_default_parameter_entry_not_allowed invalid traceability)

add_wio_failure_test(
    wio_invalid_default_parameter_overload_conflict
    "${CMAKE_SOURCE_DIR}/tests/invalid/default_parameter_overload_conflict.wio"
    "Default parameters on 'Score' would synthesize an overload with 1 arguments, but that signature is already declared explicitly."
)
wio_test_labels(wio_invalid_default_parameter_overload_conflict invalid traceability)

add_wio_failure_test(
    wio_invalid_pack_field_requires_generic_pack
    "${CMAKE_SOURCE_DIR}/tests/invalid/pack_field_requires_generic_pack.wio"
    "Pack fields must declare a trailing generic pack type such as 'pack values: Args\\.\\.\\.;'"
)
wio_test_labels(wio_invalid_pack_field_requires_generic_pack invalid traceability)

add_wio_failure_test(
    wio_invalid_pack_index_requires_compile_time_constant
    "${CMAKE_SOURCE_DIR}/tests/invalid/pack_index_requires_compile_time_constant.wio"
    "Pack indexing requires a non-negative compile-time integer index."
)
wio_test_labels(wio_invalid_pack_index_requires_compile_time_constant invalid traceability)

add_wio_failure_test(
    wio_invalid_pack_index_requires_const_declaration
    "${CMAKE_SOURCE_DIR}/tests/invalid/pack_index_requires_const_declaration.wio"
    "Pack type indexing requires a non-negative compile-time integer index."
)
wio_test_labels(wio_invalid_pack_index_requires_const_declaration invalid traceability)

add_wio_failure_test(
    wio_invalid_pack_index_from_size_requires_subtraction
    "${CMAKE_SOURCE_DIR}/tests/invalid/pack_index_from_size_requires_subtraction.wio"
    "Pack type indexing from '.size' must subtract at least 1."
)
wio_test_labels(wio_invalid_pack_index_from_size_requires_subtraction invalid traceability)

add_wio_failure_test(
    wio_invalid_type_pack_to_static_array_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/type_pack_to_static_array_not_supported.wio"
    "Type packs cannot be converted to runtime static arrays."
)
wio_test_labels(wio_invalid_type_pack_to_static_array_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_array_literal_missing_element
    "${CMAKE_SOURCE_DIR}/tests/invalid/array_literal_missing_element.wio"
    "Expected array element after ','."
)
wio_test_labels(wio_invalid_array_literal_missing_element invalid)

add_wio_failure_test(
    wio_invalid_dictionary_literal_missing_entry
    "${CMAKE_SOURCE_DIR}/tests/invalid/dictionary_literal_missing_entry.wio"
    "Expected dictionary entry after ','."
)
wio_test_labels(wio_invalid_dictionary_literal_missing_entry invalid)

add_wio_failure_test(
    wio_invalid_static_array_type_missing_size
    "${CMAKE_SOURCE_DIR}/tests/invalid/static_array_type_missing_size.wio"
    "Static array types must declare a size"
)
wio_test_labels(wio_invalid_static_array_type_missing_size invalid)

add_wio_failure_test(
    wio_invalid_fixed_array_auto_extent_without_initializer
    "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_without_initializer.wio"
    "Inferred static array extent '\\[T; _\\]' requires an initializer."
)
wio_test_labels(wio_invalid_fixed_array_auto_extent_without_initializer invalid traceability coherence)

add_wio_failure_test(
    wio_invalid_fixed_array_auto_extent_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_parameter.wio"
    "Inferred static array extent '\\[T; _\\]' is supported only on variable declarations with an initializer."
)
wio_test_labels(wio_invalid_fixed_array_auto_extent_parameter invalid traceability coherence)

add_wio_failure_test(
    wio_invalid_fixed_array_auto_extent_dynamic_source
    "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_dynamic_source.wio"
    "Inferred static array extent requires an array literal or concrete fixed-size array initializer."
)
wio_test_labels(wio_invalid_fixed_array_auto_extent_dynamic_source invalid traceability coherence)

add_wio_failure_test(
    wio_invalid_fixed_array_auto_extent_ragged_nested
    "${CMAKE_SOURCE_DIR}/tests/invalid/fixed_array_auto_extent_ragged_nested.wio"
    "Array elements must be of the same type. Expected '\\[i32; 2\\]', but found '\\[i32; 1\\]'."
)
wio_test_labels(wio_invalid_fixed_array_auto_extent_ragged_nested invalid traceability coherence)

add_wio_failure_test(
    wio_invalid_function_type_missing_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/function_type_missing_parameter.wio"
    "Expected function type parameter after ','."
)
wio_test_labels(wio_invalid_function_type_missing_parameter invalid)

add_wio_failure_test(
    wio_invalid_function_parameter_missing_after_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/function_parameter_missing_after_comma.wio"
    "Expected function parameter after ','."
)
wio_test_labels(wio_invalid_function_parameter_missing_after_comma invalid)

add_wio_failure_test(
    wio_invalid_lambda_parameter_missing_after_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/lambda_parameter_missing_after_comma.wio"
    "Expected lambda parameter after ','."
)
wio_test_labels(wio_invalid_lambda_parameter_missing_after_comma invalid)

add_wio_failure_test(
    wio_invalid_generic_parameter_missing_after_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_parameter_missing_after_comma.wio"
    "Expected generic parameter after ','."
)
wio_test_labels(wio_invalid_generic_parameter_missing_after_comma invalid)

add_wio_failure_test(
    wio_invalid_attribute_argument_missing_after_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/attribute_argument_missing_after_comma.wio"
    "Expected attribute argument after ','."
)
wio_test_labels(wio_invalid_attribute_argument_missing_after_comma invalid freeze)

add_wio_failure_test_clean(
    wio_invalid_lexer_invalid_escape_sequence
    "${CMAKE_SOURCE_DIR}/tests/invalid/lexer_invalid_escape_sequence.wio"
    "Error \\[.*lexer_invalid_escape_sequence\\.wio:2:[0-9]+\\]: Invalid escape sequence: \\\\q"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_lexer_invalid_escape_sequence invalid)

add_wio_failure_test(
    wio_invalid_explicit_type_argument_missing_after_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/explicit_type_argument_missing_after_comma.wio"
    "Expected explicit type argument after ','."
)
wio_test_labels(wio_invalid_explicit_type_argument_missing_after_comma invalid)

add_wio_failure_test(
    wio_invalid_generic_type_argument_missing_after_comma
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_type_argument_missing_after_comma.wio"
    "Expected generic type argument after ','."
)
wio_test_labels(wio_invalid_generic_type_argument_missing_after_comma invalid)

add_wio_failure_test(
    wio_invalid_native_function_with_body
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_function_with_body.wio"
    "@Native functions cannot define a Wio body"
)
wio_test_labels(wio_invalid_native_function_with_body invalid)

add_wio_failure_test_clean(
    wio_invalid_native_function_with_body_clean
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_function_with_body.wio"
    "@Native functions cannot define a Wio body"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_native_function_with_body_clean invalid)

add_wio_failure_test(
    wio_invalid_native_method_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_method_not_supported.wio"
    "@Native methods are currently supported only on object methods and on object/component OnConstruct/OnDestruct lifecycle functions"
)
wio_test_labels(wio_invalid_native_method_not_supported invalid)

add_wio_failure_test(
    wio_invalid_native_component_non_pod_field_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_component_non_pod_field_not_supported.wio"
    "Declaration-level @Native component 'BadNativeComponent' field 'name' uses type 'string' which is not POD-native-compatible yet"
)
wio_test_labels(wio_invalid_native_component_non_pod_field_not_supported invalid)

add_wio_failure_test(
    wio_invalid_native_component_dynamic_array_field_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_component_dynamic_array_field_not_supported.wio"
    "Declaration-level @Native component 'BadNativeArrayField' field 'values' uses type 'i32\\[\\]' which is not POD-native-compatible yet"
)
wio_test_labels(wio_invalid_native_component_dynamic_array_field_not_supported invalid)

add_wio_failure_test(
    wio_invalid_native_generic_component_non_pod_specialization_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_generic_component_non_pod_specialization_not_supported.wio"
    "Declaration-level @Native component 'Pair<string>' field 'first' resolves to type 'string' which is not POD-native-compatible yet"
)
wio_test_labels(wio_invalid_native_generic_component_non_pod_specialization_not_supported invalid)

add_wio_failure_test(
    wio_invalid_native_object_declaration_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_object_declaration_not_supported.wio"
    "Declaration-level @Native POD interop is currently supported only on components"
)
wio_test_labels(wio_invalid_native_object_declaration_not_supported invalid)

add_wio_failure_test(
    wio_invalid_missing_cpp_header_use
    "${CMAKE_SOURCE_DIR}/tests/invalid/missing_cpp_header_use.wio"
    "Native C\\+\\+ header 'missing_cpp_header_use_test.h' referenced by use @CppHeader in '.*missing_cpp_header_use\\.wio' was not found"
)
wio_test_labels(wio_invalid_missing_cpp_header_use invalid)

add_wio_failure_test(
    wio_invalid_missing_cpp_header_native_attr
    "${CMAKE_SOURCE_DIR}/tests/invalid/missing_cpp_header_native_attr.wio"
    "Native C\\+\\+ header 'missing_cpp_header_attr_test.h' referenced by @CppHeader on native function 'Invoke' in '.*missing_cpp_header_native_attr\\.wio' was not found"
)
wio_test_labels(wio_invalid_missing_cpp_header_native_attr invalid)

add_wio_failure_test(
    wio_invalid_numeric_literal_context_u8_out_of_range
    "${CMAKE_SOURCE_DIR}/tests/invalid/numeric_literal_context_u8_out_of_range.wio"
    "Integer literal '300' does not fit into expected type 'u8'"
)
wio_test_labels(wio_invalid_numeric_literal_context_u8_out_of_range invalid)

add_wio_failure_test(
    wio_invalid_numeric_literal_context_f32_out_of_range
    "${CMAKE_SOURCE_DIR}/tests/invalid/numeric_literal_context_f32_out_of_range.wio"
    "Float literal '1\\.0e100' does not fit into expected type 'f32'"
)
wio_test_labels(wio_invalid_numeric_literal_context_f32_out_of_range invalid)

add_wio_failure_test_clean(
    wio_invalid_null_object_requires_nullable
    "${CMAKE_SOURCE_DIR}/tests/invalid/null_object_requires_nullable.wio"
    "Type 'Service' cannot be initialized with null"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_null_object_requires_nullable invalid)

add_wio_failure_test_clean(
    wio_invalid_nullable_member_requires_narrowing
    "${CMAKE_SOURCE_DIR}/tests/invalid/nullable_member_requires_narrowing.wio"
    "Member access requires a non-null value"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_nullable_member_requires_narrowing invalid)

add_wio_failure_test_clean(
    wio_invalid_duplicate_nullable_suffix
    "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_nullable_suffix.wio"
    "A type can only have one nullable suffix '\\?'"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_duplicate_nullable_suffix invalid)

add_wio_failure_test_clean(
    wio_invalid_nullable_dynamic_array_handle
    "${CMAKE_SOURCE_DIR}/tests/invalid/nullable_dynamic_array_handle.wio"
    "Type 'i32\\[\\]' cannot be nullable"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_nullable_dynamic_array_handle invalid)

add_wio_failure_test_clean(
    wio_invalid_destructor_parameters_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/destructor_parameters_not_supported.wio"
    "OnDestruct must not declare parameters"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_destructor_parameters_not_supported invalid)

add_wio_failure_test_clean(
    wio_invalid_lifecycle_return_must_be_void
    "${CMAKE_SOURCE_DIR}/tests/invalid/lifecycle_return_must_be_void.wio"
    "OnDestruct must return void"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_lifecycle_return_must_be_void invalid)

add_wio_failure_test_clean(
    wio_invalid_non_null_local_requires_initializer
    "${CMAKE_SOURCE_DIR}/tests/invalid/non_null_local_requires_initializer.wio"
    "Non-null type 'Service' requires an initializer"
    "Compiling failed:"
)
wio_test_labels(wio_invalid_non_null_local_requires_initializer invalid)

add_wio_failure_test(
    wio_invalid_implicit_narrowing_requires_fit
    "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_requires_fit.wio"
    "Implicit narrowing conversion from 'i32' to 'u8' requires explicit 'fit'"
)
wio_test_labels(wio_invalid_implicit_narrowing_requires_fit invalid)

add_wio_failure_test(
    wio_invalid_implicit_narrowing_assignment_requires_fit
    "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_assignment_requires_fit.wio"
    "Implicit narrowing conversion from 'i32' to 'u8' requires explicit 'fit'"
)
wio_test_labels(wio_invalid_implicit_narrowing_assignment_requires_fit invalid)

add_wio_failure_test(
    wio_invalid_implicit_narrowing_return_requires_fit
    "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_return_requires_fit.wio"
    "Implicit narrowing return conversion from 'i32' to 'u8' requires explicit 'fit'"
)
wio_test_labels(wio_invalid_implicit_narrowing_return_requires_fit invalid)

add_wio_failure_test(
    wio_invalid_implicit_narrowing_argument_requires_fit
    "${CMAKE_SOURCE_DIR}/tests/invalid/implicit_narrowing_argument_requires_fit.wio"
    "Implicit narrowing argument conversion from 'i32' to 'u8' at index 0 requires explicit 'fit'"
)
wio_test_labels(wio_invalid_implicit_narrowing_argument_requires_fit invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_cpp_line_mapping_native_name
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_cpp_line_mapping_native_name.wio"
    "Error \\[.*backend_cpp_line_mapping_native_name\\.wio:3:16\\]:"
    --run
)
wio_test_labels(wio_invalid_backend_cpp_line_mapping_native_name invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_linker_missing_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_linker_missing_symbol.wio"
    "Error \\[backend:linker\\]: Native linker: (undefined reference to|unresolved external symbol|symbol\\(s\\) not found|cannot find)"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_invalid_backend_linker_missing_symbol invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_compile_error_from_native_source
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_compile_error_from_native_source.wio"
    "Error \\[.*backend_compile_error_source\\.cpp:[0-9]+:[0-9]+\\]: Native C\\+\\+ compiler:"
    --backend-arg
    "${CMAKE_SOURCE_DIR}/tests/native/backend_compile_error_source.cpp"
)
wio_test_labels(wio_invalid_backend_compile_error_from_native_source invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_generated_code_failure
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_generated_code_failure.wio"
    "Error \\[backend:generated-cpp:[0-9]+:[0-9]+\\]: Generated C\\+\\+ wrapper:"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_invalid_backend_generated_code_failure invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_include_chain_failure
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_include_chain_failure.wio"
    "Note \\[.*backend_include_chain_root\\.h:[0-9]+\\]: Native C\\+\\+ compiler: Included from here"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
    --run
)
wio_test_labels(wio_invalid_backend_include_chain_failure invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_include_chain_generated_note
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_include_chain_failure.wio"
    "Note \\[backend:generated-cpp:[0-9]+\\]: Generated C\\+\\+ wrapper: Included from here"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
    --run
)
wio_test_labels(wio_invalid_backend_include_chain_generated_note invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_template_context_note
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_compile_error_from_native_source.wio"
    "Note \\[.*backend_template_context_source\\.cpp:[0-9]+:[0-9]+\\]: Native C\\+\\+ compiler: required from here"
    --backend-arg
    "${CMAKE_SOURCE_DIR}/tests/native/backend_template_context_source.cpp"
)
wio_test_labels(wio_invalid_backend_template_context_note invalid)

add_wio_backend_failure_test(
    wio_invalid_backend_template_instantiation_note
    "${CMAKE_SOURCE_DIR}/tests/invalid/backend_compile_error_from_native_source.wio"
    "Note \\[.*backend_template_context_source\\.cpp\\]: Native C\\+\\+ compiler: In instantiation of 'void backendTemplateContextLeaf\\(\\) \\[with T = int\\]':"
    --backend-arg
    "${CMAKE_SOURCE_DIR}/tests/native/backend_template_context_source.cpp"
)
wio_test_labels(wio_invalid_backend_template_instantiation_note invalid)

add_wio_failure_test(
    wio_invalid_cpp_name_native_invalid_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/cpp_name_native_invalid_symbol.wio"
    "@CppName for @Native must be a valid C\\+\\+ identifier path like foo::bar\\."
)
wio_test_labels(wio_invalid_cpp_name_native_invalid_symbol invalid)

add_wio_failure_test(
    wio_invalid_cpp_name_export_invalid_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/cpp_name_export_invalid_symbol.wio"
    "@CppName for @Export must be a plain C/C\\+\\+ symbol name like FooBar\\."
)
wio_test_labels(wio_invalid_cpp_name_export_invalid_symbol invalid)

add_wio_failure_test(
    wio_invalid_missing_module_dir_arg
    "${CMAKE_SOURCE_DIR}/tests/import_path_root.wio"
    "Module search directory '.*missing_module_dir.*' was not found or is not a directory"
    --module-dir
    "${CMAKE_SOURCE_DIR}/tests/missing_module_dir"
)
wio_test_labels(wio_invalid_missing_module_dir_arg invalid)

add_wio_failure_test(
    wio_invalid_missing_include_dir_arg
    "${CMAKE_SOURCE_DIR}/tests/native/native_bridge.wio"
    "Include directory '.*missing_include_dir.*' was not found or is not a directory"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/missing_include_dir"
    --backend-arg
    "${CMAKE_SOURCE_DIR}/tests/native/native_math.cpp"
)
wio_test_labels(wio_invalid_missing_include_dir_arg invalid)

add_wio_failure_test(
    wio_invalid_missing_link_dir_arg
    "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
    "Link directory '.*missing_link_dir.*' was not found or is not a directory"
    --link-dir
    "${CMAKE_SOURCE_DIR}/tests/missing_link_dir"
)
wio_test_labels(wio_invalid_missing_link_dir_arg invalid)

add_wio_failure_test(
    wio_invalid_missing_backend_source_arg
    "${CMAKE_SOURCE_DIR}/tests/native/native_bridge.wio"
    "Backend source file '.*missing_backend_source\\.cpp' was not found"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
    --backend-arg
    "${CMAKE_SOURCE_DIR}/tests/native/missing_backend_source.cpp"
)
wio_test_labels(wio_invalid_missing_backend_source_arg invalid)

add_wio_failure_test_clean(
    wio_invalid_cli_unknown_build_target
    "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
    "Error \\[cli\\]: Unknown build target: nonsense\\. Expected one of: exe, static, shared\\."
    "Unexpected Error:"
    --target
    nonsense
)
wio_test_labels(wio_invalid_cli_unknown_build_target invalid)

add_wio_non_dry_failure_test_clean(
    wio_invalid_emit_cpp_with_dry_run
    "${CMAKE_SOURCE_DIR}/tests/test1.wio"
    "--emit-cpp cannot be combined with --dry-run\\."
    "Unexpected Error:"
    --emit-cpp
    --dry-run
)
wio_test_labels(wio_invalid_emit_cpp_with_dry_run invalid)

add_wio_non_dry_failure_test_clean(
    wio_invalid_emit_cpp_with_run
    "${CMAKE_SOURCE_DIR}/tests/test1.wio"
    "--emit-cpp cannot be combined with --run\\."
    "Unexpected Error:"
    --emit-cpp
    --run
)
wio_test_labels(wio_invalid_emit_cpp_with_run invalid)

add_wio_failure_test(
    wio_invalid_missing_link_library_file_arg
    "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
    "Linked library file '.*missing_link_library\\.lib' was not found"
    --link-lib
    "${CMAKE_SOURCE_DIR}/tests/native/missing_link_library.lib"
)
wio_test_labels(wio_invalid_missing_link_library_file_arg invalid)

add_wio_success_test(
    wio_invalid_export_string_return_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/export_string_return_not_supported.wio"
)
wio_test_labels(wio_invalid_export_string_return_not_supported invalid)

add_wio_failure_test(
    wio_invalid_duplicate_export_cpp_name
    "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_export_cpp_name.wio"
    "Exported C\\+\\+ symbol 'WioEcho' is already declared by another @Export or module lifecycle function\\."
)
wio_test_labels(wio_invalid_duplicate_export_cpp_name invalid)

add_wio_failure_test(
    wio_invalid_duplicate_export_module_symbol
    "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_export_module_symbol.wio"
    "Exported C\\+\\+ symbol 'WioModuleLoad' is already declared by another @Export or module lifecycle function\\."
)
wio_test_labels(wio_invalid_duplicate_export_module_symbol invalid)

add_wio_failure_test(
    wio_invalid_for_in_requires_array
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_in_requires_array.wio"
    "For-in loops currently require an array or dictionary type"
)
wio_test_labels(wio_invalid_for_in_requires_array invalid traceability)

add_wio_failure_test(
    wio_invalid_for_dict_requires_two_bindings
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_dict_requires_two_bindings.wio"
    "Dictionary iteration requires exactly 2 bindings"
)
wio_test_labels(wio_invalid_for_dict_requires_two_bindings invalid traceability)

add_wio_failure_test(
    wio_invalid_for_ref_dict_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_ref_dict_not_supported.wio"
    "Dictionary keys are immutable"
)
wio_test_labels(wio_invalid_for_ref_dict_not_supported invalid)

add_wio_failure_test(
    wio_invalid_for_component_binding_count_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_component_binding_count_mismatch.wio"
    "Component binding expects 2 names, or 3 with an index binding, but 4 were provided"
)
wio_test_labels(wio_invalid_for_component_binding_count_mismatch invalid traceability freeze)

add_wio_failure_test(
    wio_invalid_for_array_index_ref_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_array_index_ref_not_supported.wio"
    "Array index bindings do not support 'ref' or 'view'"
)
wio_test_labels(wio_invalid_for_array_index_ref_not_supported invalid)

add_wio_failure_test(
    wio_invalid_for_dict_step_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_dict_step_not_supported.wio"
    "Step clauses are currently supported only for range or array iteration"
)
wio_test_labels(wio_invalid_for_dict_step_not_supported invalid)

add_wio_failure_test(
    wio_invalid_for_array_step_requires_integer
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_array_step_requires_integer.wio"
    "Array step expressions must be integer values"
)
wio_test_labels(wio_invalid_for_array_step_requires_integer invalid)

add_wio_failure_test(
    wio_invalid_array_member_requires_mutable_receiver
    "${CMAKE_SOURCE_DIR}/tests/invalid/array_member_requires_mutable_receiver.wio"
    "Container member 'Push' requires a mutable receiver"
)
wio_test_labels(wio_invalid_array_member_requires_mutable_receiver invalid traceability)

add_wio_failure_test(
    wio_invalid_static_array_mutating_member_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/static_array_mutating_member_not_supported.wio"
    "Static arrays do not support member 'Push'. Use a dynamic array instead"
)
wio_test_labels(wio_invalid_static_array_mutating_member_not_supported invalid)

add_wio_failure_test(
    wio_invalid_dictionary_member_requires_mutable_receiver
    "${CMAKE_SOURCE_DIR}/tests/invalid/dictionary_member_requires_mutable_receiver.wio"
    "Container member 'Remove' requires a mutable receiver"
)
wio_test_labels(wio_invalid_dictionary_member_requires_mutable_receiver invalid traceability)

add_wio_failure_test(
    wio_invalid_string_member_requires_mutable_receiver
    "${CMAKE_SOURCE_DIR}/tests/invalid/string_member_requires_mutable_receiver.wio"
    "Container member 'Append' requires a mutable receiver"
)
wio_test_labels(wio_invalid_string_member_requires_mutable_receiver invalid traceability)

add_wio_failure_test(
    wio_invalid_opaque_arithmetic_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/opaque_arithmetic_not_supported.wio"
    "Opaque values support only assignment and equality comparisons\\. Operator '\\+' is not supported\\."
)
wio_test_labels(wio_invalid_opaque_arithmetic_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_for_range_ref_binding_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_range_ref_binding_not_supported.wio"
    "Range iteration does not support 'ref' or 'view' bindings"
)
wio_test_labels(wio_invalid_for_range_ref_binding_not_supported invalid)

add_wio_failure_test(
    wio_invalid_for_range_step_zero
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_range_step_zero.wio"
    "Range step cannot be zero"
)
wio_test_labels(wio_invalid_for_range_step_zero invalid traceability)

add_wio_failure_test(
    wio_invalid_for_range_requires_integer_bounds
    "${CMAKE_SOURCE_DIR}/tests/invalid/for_range_requires_integer_bounds.wio"
    "Range iteration currently requires integer bounds"
)
wio_test_labels(wio_invalid_for_range_requires_integer_bounds invalid)

add_wio_failure_test(
    wio_invalid_break_outside_loop
    "${CMAKE_SOURCE_DIR}/tests/invalid/break_outside_loop.wio"
    "'break' statement can only be used inside a loop"
)
wio_test_labels(wio_invalid_break_outside_loop invalid traceability)

add_wio_failure_test(
    wio_invalid_continue_outside_loop
    "${CMAKE_SOURCE_DIR}/tests/invalid/continue_outside_loop.wio"
    "'continue' statement can only be used inside a loop"
)
wio_test_labels(wio_invalid_continue_outside_loop invalid traceability)

add_wio_failure_test(
    wio_invalid_c_style_for_invalid_condition_type
    "${CMAKE_SOURCE_DIR}/tests/invalid/c_style_for_invalid_condition_type.wio"
    "For-loop condition must be a boolean, numeric, or reference type. Got: string"
)
wio_test_labels(wio_invalid_c_style_for_invalid_condition_type invalid traceability)

add_wio_failure_test(
    wio_invalid_module_load_wrong_return
    "${CMAKE_SOURCE_DIR}/tests/invalid/module_load_wrong_return.wio"
    "@ModuleLoad must return i32"
)
wio_test_labels(wio_invalid_module_load_wrong_return invalid)

add_wio_failure_test(
    wio_invalid_duplicate_module_update
    "${CMAKE_SOURCE_DIR}/tests/invalid/duplicate_module_update.wio"
    "Only one @ModuleUpdate function may be declared per compilation unit"
)
wio_test_labels(wio_invalid_duplicate_module_update invalid)

add_wio_failure_test(
    wio_invalid_module_restore_wrong_param
    "${CMAKE_SOURCE_DIR}/tests/invalid/module_restore_wrong_param.wio"
    "@ModuleRestoreState must declare exactly one i32 parameter"
)
wio_test_labels(wio_invalid_module_restore_wrong_param invalid)

add_wio_failure_test(
    wio_invalid_command_requires_export
    "${CMAKE_SOURCE_DIR}/tests/invalid/command_requires_export.wio"
    "@Command requires @Export on the same function"
)
wio_test_labels(wio_invalid_command_requires_export invalid)

add_wio_failure_test(
    wio_invalid_event_requires_void_return
    "${CMAKE_SOURCE_DIR}/tests/invalid/event_requires_void_return.wio"
    "@Event hooks must return void"
)
wio_test_labels(wio_invalid_event_requires_void_return invalid)

add_wio_failure_test(
    wio_invalid_generic_function_argument_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_function_argument_mismatch.wio"
    "No matching overload for 'PickFirst' with arguments \\(i32, string\\)\\. Candidates: PickFirst<T>\\(T, T\\) -> T\\."
)
wio_test_labels(wio_invalid_generic_function_argument_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_interface_method_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_interface_method_not_supported.wio"
    "Generic methods are currently supported only on object methods"
)
wio_test_labels(wio_invalid_generic_interface_method_not_supported invalid)

add_wio_failure_test(
    wio_invalid_generic_component_method_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_component_method_not_supported.wio"
    "Generic methods are currently supported only on object methods"
)
wio_test_labels(wio_invalid_generic_component_method_not_supported invalid)

add_wio_failure_test(
    wio_invalid_generic_explicit_argument_count_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_explicit_argument_count_mismatch.wio"
    "Error \\[.*generic_explicit_argument_count_mismatch\\.wio:6:16\\]: Generic call to 'MakeLabel' expects 2 explicit type arguments, but got 1\\."
)
wio_test_labels(wio_invalid_generic_explicit_argument_count_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_overload_ambiguous
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_overload_ambiguous.wio"
    "Ambiguous function call to 'Echo' with arguments \\(i32\\)\\. Candidates: Echo<T>\\(T\\) -> T; Echo<U>\\(U\\) -> U\\."
)
wio_test_labels(wio_invalid_generic_overload_ambiguous invalid)

add_wio_failure_test(
    wio_invalid_overload_no_matching_signature
    "${CMAKE_SOURCE_DIR}/tests/invalid/overload_no_matching_signature.wio"
    "No matching overload for 'Mix' with arguments \\(bool\\)\\. Candidates: Mix\\(i32\\) -> i32; Mix\\(string\\) -> string\\."
)
wio_test_labels(wio_invalid_overload_no_matching_signature invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_native_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_native_not_supported.wio"
    "Generic @Native functions require at least one @Instantiate\\(\\.\\.\\.\\) declaration\\."
)
wio_test_labels(wio_invalid_generic_native_not_supported invalid)

add_wio_failure_test(
    wio_invalid_generic_export_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_not_supported.wio"
    "Generic @Export functions require at least one @Instantiate\\(\\.\\.\\.\\) declaration\\."
)
wio_test_labels(wio_invalid_generic_export_not_supported invalid)

add_wio_failure_test(
    wio_invalid_generic_instantiate_argument_count_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_argument_count_mismatch.wio"
    "@Instantiate expects exactly 2 arguments\\."
)
wio_test_labels(wio_invalid_generic_instantiate_argument_count_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_instantiate_non_concrete
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_non_concrete.wio"
    "@Instantiate must use fully concrete type constraints or supported predicates like std::traits::IsInteger<T>\\."
)
wio_test_labels(wio_invalid_generic_instantiate_non_concrete invalid)

add_wio_failure_test(
    wio_invalid_generic_instantiate_requires_native_or_export
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_instantiate_requires_native_or_export.wio"
    "@Instantiate is currently supported only together with @Native or @Export\\."
)
wio_test_labels(wio_invalid_generic_instantiate_requires_native_or_export invalid)

add_wio_failure_test(
    wio_invalid_generic_export_instantiate_non_abi_safe
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_instantiate_non_abi_safe.wio"
    "@Export instantiated with '<string>' produces a non-C-ABI-safe parameter 0 of type 'string'\\."
)
wio_test_labels(wio_invalid_generic_export_instantiate_non_abi_safe invalid)

add_wio_failure_test(
    wio_invalid_generic_native_call_not_instantiated
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_native_call_not_instantiated.wio"
    "Generic interop function 'DoubleValue' does not declare @Instantiate<string>\\."
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_invalid_generic_native_call_not_instantiated invalid)

add_wio_failure_test(
    wio_invalid_generic_export_call_not_instantiated
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_export_call_not_instantiated.wio"
    "Generic interop function 'Echo' does not declare @Instantiate<bool>\\."
)
wio_test_labels(wio_invalid_generic_export_call_not_instantiated invalid)

add_wio_failure_test(
    wio_invalid_generic_object_constructor_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_object_constructor_not_supported.wio"
    "Generic constructors and destructors are not supported yet"
)
wio_test_labels(wio_invalid_generic_object_constructor_not_supported invalid)

add_wio_failure_test(
    wio_invalid_generic_type_alias_missing_arguments
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_type_alias_missing_arguments.wio"
    "Type alias 'Buffer' expects 1 generic arguments, but got 0"
)
wio_test_labels(wio_invalid_generic_type_alias_missing_arguments invalid)

add_wio_failure_test(
    wio_invalid_generic_type_alias_argument_count_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_type_alias_argument_count_mismatch.wio"
    "Type alias 'Ordered' expects 2 generic arguments, but got 1"
)
wio_test_labels(wio_invalid_generic_type_alias_argument_count_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_type_alias_generic_args_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/type_alias_generic_args_not_supported.wio"
    "Type alias 'Score' does not accept generic arguments"
)
wio_test_labels(wio_invalid_type_alias_generic_args_not_supported invalid)

add_wio_failure_test(
    wio_invalid_type_alias_attributes_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/type_alias_attributes_not_supported.wio"
    "Only @Apply is currently supported on type aliases\\."
)
wio_test_labels(wio_invalid_type_alias_attributes_not_supported invalid freeze)

add_wio_failure_test(
    wio_invalid_generic_apply_reject_function_call
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_reject_function_call.wio"
    "Generic callable 'Echo' rejects type arguments <string> because of @Apply constraints\\."
)
wio_test_labels(wio_invalid_generic_apply_reject_function_call invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_apply_list_reject_function_call
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_list_reject_function_call.wio"
    "Generic callable 'Square' rejects type arguments <i32> because of @Apply constraints\\."
)
wio_test_labels(wio_invalid_generic_apply_list_reject_function_call invalid)

add_wio_failure_test(
    wio_invalid_generic_apply_reject_type_alias
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_reject_type_alias.wio"
    "Generic type alias 'IntList' rejects type arguments <string> because of @Apply constraints\\."
)
wio_test_labels(wio_invalid_generic_apply_reject_type_alias invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_apply_reject_constructor
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_reject_constructor.wio"
    "Generic type 'NumberBox' rejects type arguments <string> because of @Apply constraints\\."
)
wio_test_labels(wio_invalid_generic_apply_reject_constructor invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_apply_non_generic_function
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_apply_non_generic_function.wio"
    "@Apply can only be used on generic functions\\."
)
wio_test_labels(wio_invalid_generic_apply_non_generic_function invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_parameter_pack_non_trailing
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_parameter_pack_non_trailing.wio"
    "Generic parameter packs must be trailing\\."
)
wio_test_labels(wio_invalid_generic_parameter_pack_non_trailing invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_non_trailing
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_non_trailing.wio"
    "Function parameter packs must be trailing\\."
)
wio_test_labels(wio_invalid_parameter_pack_non_trailing invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_missing_matching_generic_pack
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_missing_matching_generic_pack.wio"
    "Function parameter packs require a matching trailing generic parameter pack\\."
)
wio_test_labels(wio_invalid_parameter_pack_missing_matching_generic_pack invalid traceability freeze)

add_wio_failure_test(
    wio_invalid_parameter_pack_bare_use_not_allowed
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_bare_use_not_allowed.wio"
    "Parameter pack 'args' must be expanded as 'args\\.\\.\\.' inside a function call\\."
)
wio_test_labels(wio_invalid_parameter_pack_bare_use_not_allowed invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_default_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_default_not_supported.wio"
    "Parameter pack 'args' cannot declare a default value\\. Default values apply only to fixed parameters before the trailing pack\\."
    --target
    static
)
wio_test_labels(wio_invalid_parameter_pack_default_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_method_not_supported
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_method_not_supported.wio"
    "Generic parameter packs are currently supported only on top-level functions\\."
)
wio_test_labels(wio_invalid_parameter_pack_method_not_supported invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_apply_reject_non_integer
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_apply_reject_non_integer.wio"
    "Generic callable 'CountIntegers' rejects type arguments <i32, f32> because of @Apply constraints\\."
)
wio_test_labels(wio_invalid_parameter_pack_apply_reject_non_integer invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_export_requires_instantiate
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_export_requires_instantiate.wio"
    "Generic @Export pack functions must declare at least one concrete @Instantiate\\(\\.\\.\\.\\)\\."
    --target
    shared
)
wio_test_labels(wio_invalid_parameter_pack_export_requires_instantiate invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_instantiate_predicate_not_allowed
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_instantiate_predicate_not_allowed.wio"
    "@Instantiate on generic pack declarations must provide concrete pack element types for 'Args\\.\\.\\.'\\. Predicates belong in @Apply\\."
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
    --target
    static
)
wio_test_labels(wio_invalid_parameter_pack_instantiate_predicate_not_allowed invalid traceability)

add_wio_failure_test(
    wio_invalid_parameter_pack_view_assignment_not_allowed
    "${CMAKE_SOURCE_DIR}/tests/invalid/parameter_pack_view_assignment_not_allowed.wio"
    "Cannot assign through raw pack values or pack views\\. Assign to a mutable pack storage field instead\\."
    --target
    static
)
wio_test_labels(wio_invalid_parameter_pack_view_assignment_not_allowed invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_native_predicate_reject_non_integer
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_native_predicate_reject_non_integer.wio"
    "Generic interop function 'DoubleValue' does not declare @Instantiate<f32>\\."
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_invalid_generic_native_predicate_reject_non_integer invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_object_constructor_missing_arguments
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_object_constructor_missing_arguments.wio"
    "Cannot infer generic arguments for constructor 'Box'. Use explicit type arguments."
)
wio_test_labels(wio_invalid_generic_object_constructor_missing_arguments invalid)

add_wio_failure_test(
    wio_invalid_generic_specialization_requires_primary
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_requires_primary.wio"
    "@Specialize object 'MissingPrimary' requires an earlier generic object declaration with the same name\."
)
wio_test_labels(wio_invalid_generic_specialization_requires_primary invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_specialization_argument_count_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_argument_count_mismatch.wio"
    "@Specialize for 'Pair' expects 2 type arguments, but got 1\."
)
wio_test_labels(wio_invalid_generic_specialization_argument_count_mismatch invalid traceability freeze)

add_wio_failure_test(
    wio_invalid_generic_specialization_duplicate
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_duplicate.wio"
    "Duplicate explicit specialization for 'Box<i32>'\."
)
wio_test_labels(wio_invalid_generic_specialization_duplicate invalid traceability)

add_wio_failure_test(
    wio_invalid_generic_specialization_native_component
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_specialization_native_component.wio"
    "A declaration-level @Native specialization requires an @Native generic component primary"
)
wio_test_labels(wio_invalid_generic_specialization_native_component invalid traceability)

add_wio_failure_test(
    wio_invalid_native_extension_requires_native_component
    "${CMAKE_SOURCE_DIR}/tests/invalid/native_extension_requires_native_component.wio"
    "@Native extension methods require a declaration-level @Native component target"
)
wio_test_labels(wio_invalid_native_extension_requires_native_component invalid)

add_wio_failure_test(
    wio_invalid_generic_component_constructor_missing_arguments
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_component_constructor_missing_arguments.wio"
    "Cannot infer generic arguments for constructor 'Pair'. Use explicit type arguments."
)
wio_test_labels(wio_invalid_generic_component_constructor_missing_arguments invalid)

add_wio_failure_test(
    wio_invalid_generic_from_missing_arguments
    "${CMAKE_SOURCE_DIR}/tests/invalid/generic_from_missing_arguments.wio"
    "Generic type 'Reader' expects 1 generic arguments, but got 0"
)
wio_test_labels(wio_invalid_generic_from_missing_arguments invalid)

add_wio_failure_test(
    wio_invalid_lambda_contextual_return_mismatch
    "${CMAKE_SOURCE_DIR}/tests/invalid/lambda_contextual_return_mismatch.wio"
    "Return type mismatch"
)
wio_test_labels(wio_invalid_lambda_contextual_return_mismatch invalid freeze)

add_wio_failure_test(
    wio_invalid_exported_object_non_abi_field
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_object_non_abi_field.wio"
    "@Export object 'BadProfile' exposes public field 'worker' with type 'IWorker' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_object_non_abi_field invalid)

add_wio_failure_test(
    wio_invalid_exported_option_unsupported_payload
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_option_unsupported_payload.wio"
    "@Export object 'BadOptionalProfile' exposes public field 'worker' with type 'Option<IWorker>' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_option_unsupported_payload invalid)

add_wio_failure_test(
    wio_invalid_exported_result_unsupported_payload
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_result_unsupported_payload.wio"
    "@Export object 'BadResultProfile' exposes public field 'worker' with type 'Result<IWorker>' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_result_unsupported_payload invalid)

add_wio_failure_test(
    wio_invalid_exported_nested_collection_unsupported_payload
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_nested_collection_unsupported_payload.wio"
    "@Export object 'BadNestedCollection' exposes public field 'workers' with type 'Option<IWorker>\\[\\]' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_nested_collection_unsupported_payload invalid)

add_wio_failure_test(
    wio_invalid_exported_queue_unsupported_payload
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_queue_unsupported_payload.wio"
    "@Export object 'BadQueueProfile' exposes public field 'workers' with type 'std::Queue<Option<IWorker>>' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_queue_unsupported_payload invalid)

add_wio_failure_test(
    wio_invalid_exported_tuple_unsupported_payload
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_tuple_unsupported_payload.wio"
    "@Export object 'BadTupleProfile' exposes public field 'worker' with type 'std::Tuple<i32, IWorker>' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_tuple_unsupported_payload invalid)

add_wio_failure_test(
    wio_invalid_exported_unspecialized_generic_component
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_unspecialized_generic_component.wio"
    "@Export object 'BadGenericProfile' exposes public field 'value' with type 'GenericValue<i32>' that is not yet SDK-exportable\\."
)
wio_test_labels(wio_invalid_exported_unspecialized_generic_component invalid)

add_wio_failure_test(
    wio_invalid_exported_component_requires_host_ctor
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_component_requires_host_ctor.wio"
    "@Export component 'HiddenStats' must expose at least one public host-callable constructor with only C-ABI-safe parameters\\."
)
wio_test_labels(wio_invalid_exported_component_requires_host_ctor invalid)

add_wio_failure_test(
    wio_invalid_exported_object_private_ctor
    "${CMAKE_SOURCE_DIR}/tests/invalid/exported_object_private_ctor.wio"
    "@Export object 'HiddenFactory' constructor must be public to be host-callable\\."
)
wio_test_labels(wio_invalid_exported_object_private_ctor invalid)

add_wio_failure_test(
    wio_invalid_spawn_outside_async_scope
    "${CMAKE_SOURCE_DIR}/tests/invalid/spawn_outside_async_scope.wio"
    "'spawn' requires an enclosing 'async scope'\."
)
wio_test_labels(wio_invalid_spawn_outside_async_scope invalid freeze)

add_wio_failure_test(
    wio_invalid_async_scope_outside_async
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_scope_outside_async.wio"
    "'await' can only be used inside an async function or method"
)
wio_test_labels(wio_invalid_async_scope_outside_async invalid freeze)

add_wio_failure_test(
    wio_invalid_await_main_outside_async
    "${CMAKE_SOURCE_DIR}/tests/invalid/await_main_outside_async.wio"
    "'await main' can only be used inside an async function or method"
)
wio_test_labels(wio_invalid_await_main_outside_async invalid freeze)

add_wio_failure_test(
    wio_invalid_spawn_worker_borrow_capture
    "${CMAKE_SOURCE_DIR}/tests/invalid/spawn_worker_borrow_capture.wio"
    "spawn worker cannot transfer capture 'borrowed'"
)
wio_test_labels(wio_invalid_spawn_worker_borrow_capture invalid)

add_wio_failure_test(
    wio_invalid_spawn_blocking_borrow_capture
    "${CMAKE_SOURCE_DIR}/tests/invalid/spawn_blocking_borrow_capture.wio"
    "spawn blocking cannot transfer capture 'borrowed'"
)
wio_test_labels(wio_invalid_spawn_blocking_borrow_capture invalid)

add_wio_non_dry_failure_test_clean(
    wio_runtime_async_timeout
    "${CMAKE_SOURCE_DIR}/tests/async_timeout_runtime_failure.wio"
    "Runtime Error: Unhandled native exception: asynchronous operation timed out"
    "terminate called|segmentation fault|Assertion"
    --run
)
wio_test_labels(wio_runtime_async_timeout invalid freeze)

add_wio_non_dry_failure_test_clean(
    wio_runtime_async_cancel
    "${CMAKE_SOURCE_DIR}/tests/async_cancel_runtime_failure.wio"
    "Runtime Error: Unhandled native exception: asynchronous operation was cancelled"
    "terminate called|segmentation fault|Assertion"
    --run
)
wio_test_labels(wio_runtime_async_cancel invalid freeze)

set_tests_properties(
    wio_runtime_async_timeout
    wio_runtime_async_cancel
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(wio_invalid_async_await_outside_async
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_await_outside_async.wio"
    "'await' can only be used inside an async function or method")
wio_test_labels(wio_invalid_async_await_outside_async invalid freeze)

add_wio_failure_test(wio_invalid_async_await_requires_coroutine
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_await_requires_coroutine.wio"
    "await.*operand must be a coroutine")
wio_test_labels(wio_invalid_async_await_requires_coroutine invalid freeze)

add_wio_failure_test(wio_invalid_async_reference_parameter
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_reference_parameter.wio"
    "Async parameter 'value' cannot be ref/view")
wio_test_labels(wio_invalid_async_reference_parameter invalid freeze)

add_wio_failure_test(wio_invalid_async_reference_return
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_reference_return.wio"
    "Async functions cannot return ref/view")
wio_test_labels(wio_invalid_async_reference_return invalid freeze)

add_wio_failure_test(wio_invalid_async_component_method
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_component_method.wio"
    "Stack-resident component methods cannot be async")
wio_test_labels(wio_invalid_async_component_method invalid freeze)

add_wio_failure_test(wio_invalid_async_extension_receiver
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_extension_receiver.wio"
    "Async extension methods are not yet lifetime-safe")
wio_test_labels(wio_invalid_async_extension_receiver invalid freeze)

add_wio_failure_test(wio_invalid_async_lifecycle_method
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_lifecycle_method.wio"
    "OnConstruct cannot be async")
wio_test_labels(wio_invalid_async_lifecycle_method invalid freeze)

add_wio_failure_test(wio_invalid_application_async_helper
    "${CMAKE_SOURCE_DIR}/tests/invalid/application_async_helper.wio"
    "Application functions are synchronous because their mutable stack receiver cannot cross suspension")
wio_test_labels(wio_invalid_application_async_helper invalid freeze)

add_wio_failure_test(wio_invalid_system_async_helper
    "${CMAKE_SOURCE_DIR}/tests/invalid/system_async_helper.wio"
    "System functions are synchronous because their mutable stack receiver cannot cross suspension")
wio_test_labels(wio_invalid_system_async_helper invalid freeze)

add_wio_failure_test(wio_invalid_async_coroutine_arity
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_coroutine_arity.wio"
    "coroutine.*requires exactly one result type")
wio_test_labels(wio_invalid_async_coroutine_arity invalid freeze)

add_wio_failure_test(wio_invalid_async_run_blocking_borrow_capture
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_blocking_borrow_capture.wio"
    "RunBlocking cannot transfer capture 'borrowed'")
wio_test_labels(wio_invalid_async_run_blocking_borrow_capture invalid freeze)

add_wio_failure_test(wio_invalid_async_run_borrow_capture
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_borrow_capture.wio"
    "Run cannot transfer capture 'borrowed'")
wio_test_labels(wio_invalid_async_run_borrow_capture invalid freeze)

add_wio_failure_test(wio_invalid_async_run_blocking_object_capture
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_blocking_object_capture.wio"
    "RunBlocking cannot transfer capture 'counter'")
wio_test_labels(wio_invalid_async_run_blocking_object_capture invalid freeze)

add_wio_failure_test(wio_invalid_async_run_blocking_self_capture
    "${CMAKE_SOURCE_DIR}/tests/invalid/async_run_blocking_self_capture.wio"
    "RunBlocking cannot capture 'self'")
wio_test_labels(wio_invalid_async_run_blocking_self_capture invalid freeze)

set_tests_properties(
    wio_invalid_async_await_outside_async
    wio_invalid_async_await_requires_coroutine
    wio_invalid_async_reference_parameter
    wio_invalid_async_reference_return
    wio_invalid_async_component_method
    wio_invalid_async_extension_receiver
    wio_invalid_async_lifecycle_method
    wio_invalid_application_async_helper
    wio_invalid_system_async_helper
    wio_invalid_async_coroutine_arity
    wio_invalid_async_run_blocking_borrow_capture
    wio_invalid_async_run_borrow_capture
    wio_invalid_async_run_blocking_object_capture
    wio_invalid_async_run_blocking_self_capture
    wio_invalid_spawn_worker_borrow_capture
    wio_invalid_spawn_blocking_borrow_capture
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_failure_test(
    wio_invalid_text_requires_explicit_utf8_validation
    "${CMAKE_SOURCE_DIR}/tests/invalid/text_requires_explicit_utf8_validation.wio"
    "Type mismatch for 'validated'"
)
wio_test_labels(wio_invalid_text_requires_explicit_utf8_validation invalid coherence)

add_wio_failure_test(
    wio_invalid_text_operator
    "${CMAKE_SOURCE_DIR}/tests/invalid/text_invalid_operator.wio"
    "Textual values do not support operator '-'"
)
wio_test_labels(wio_invalid_text_operator invalid coherence)

add_wio_failure_test(
    wio_invalid_text_string_mixed_operation
    "${CMAKE_SOURCE_DIR}/tests/invalid/text_string_mixed_operation.wio"
    "Mixed 'string' and 'text' operations require an explicit conversion"
)
wio_test_labels(wio_invalid_text_string_mixed_operation invalid coherence)

add_wio_failure_test(
    wio_invalid_text_index_assignment
    "${CMAKE_SOURCE_DIR}/tests/invalid/text_index_assignment.wio"
    "Text indexing is read-only"
)
wio_test_labels(wio_invalid_text_index_assignment invalid coherence)
