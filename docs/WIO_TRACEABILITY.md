# Wio Traceability and Conformance

This document ties the current stabilized Wio surface to concrete reference
docs and representative tests.

It is intentionally practical rather than exhaustive:

- `WIO_LANGUAGE_DRAFT.md` is still the main language reference
- `WIO_STD.md` is still the std contract document
- `WIO_SDK.md` is still the host ABI contract document
- this file answers the question "which tests currently prove that contract?"

---

## 1. How To Run The Curated Conformance Suite

From a configured build directory, the curated phase-4 traceability suite can be
run with:

```powershell
cmake --build build --config Debug --target wio_tests_traceability
```

Equivalent direct `ctest` usage:

```powershell
ctest --test-dir build -C Debug -R "(wio_test_(use_alias_dry_run|realm_basic_run|realm_nested_run|realm_qualified_from_run|object_component_access_semantics_run|interface_ref_upcast_run|component_embedded_member_ctor_run|for_in_dynamic_array_run|for_dict_key_value_run|for_component_binding_run|c_style_for_loop_run|match_value_basic_run|match_statement_basic_run|when_guard_fallback_value_run|fit_numeric_clamp_run|const_scalar_expressions_run|numeric_literal_suffixes_run|numeric_literal_signed_mins_run|default_parameters_run|default_parameter_native_bridge_run|function_parameter_pack_run|generic_apply_function_constraints_run|generic_apply_type_and_object_run|generic_native_predicate_bridge_run|array_member_methods_run|dictionary_member_methods_run|string_member_methods_run|std_contract_v1_run|sdk_invalid_api_contract|sdk_module_easy_host_interop|sdk_exported_types_host_interop|sdk_exported_complex_fields_host_interop|sdk_module_hot_reload_interop|sdk_hot_reload_stale_wrappers_interop|sdk_module_stale_bindings_interop)|wio_invalid_(entry_invalid_parameter_type|entry_invalid_return_type|entry_too_many_parameters|entry_missing_body|entry_method_not_top_level|use_missing_user_module|use_missing_std_module|use_alias_conflicts_with_symbol|realm_conflicts_with_symbol|ref_requires_addressable_operand|ref_immutable_to_mutable_param|is_left_requires_object_or_interface|is_right_requires_object_or_interface|fit_requires_numeric_or_object_interface|if_fit_requires_is_condition|component_method_not_allowed|object_inherits_component|final_object_inheritance|default_attribute_invalid_access|private_base_member_access|protected_member_access_outside_hierarchy|trust_requires_struct_type|let_without_initializer|mut_without_initializer|let_without_type_or_initializer|const_without_initializer|const_string_not_supported|const_from_non_const_identifier|const_function_call_not_supported|const_array_not_supported|default_parameter_non_trailing|default_parameter_type_mismatch|default_parameter_declaration_only_not_supported|default_parameter_entry_not_allowed|default_parameter_overload_conflict|generic_parameter_pack_non_trailing|parameter_pack_non_trailing|parameter_pack_missing_matching_generic_pack|parameter_pack_bare_use_not_allowed|parameter_pack_default_not_supported|parameter_pack_method_not_supported|parameter_pack_explicit_type_arguments_not_supported|generic_apply_reject_function_call|generic_apply_reject_type_alias|generic_apply_reject_constructor|generic_apply_non_generic_function|generic_native_predicate_reject_non_integer|match_value_requires_assumed|match_assumed_must_be_last|match_duplicate_assumed|match_case_type_mismatch|match_return_type_mismatch|non_void_when_without_else|when_condition_type_mismatch|when_fallback_return_type_mismatch|for_in_requires_array|for_dict_requires_two_bindings|for_component_binding_count_mismatch|for_range_step_zero|break_outside_loop|continue_outside_loop|c_style_for_invalid_condition_type|array_member_requires_mutable_receiver|dictionary_member_requires_mutable_receiver|string_member_requires_mutable_receiver|missing_cpp_header_std_import|std_imports_private_runtime_header))" --output-on-failure
```

This suite is not the whole repository test corpus. It is the current
"stabilized-behavior proof pack" for the language, std surface, and host SDK.

---

## 2. Language Traceability

| Feature slice | Reference | Representative positive tests | Representative negative tests |
| --- | --- | --- | --- |
| Literals, numeric context, compile-time scalar `const` | `WIO_LANGUAGE_DRAFT.md` sections 4, 6 | [`numeric_literal_suffixes_run.wio`](../tests/numeric_literal_suffixes_run.wio), [`numeric_literal_signed_mins_run.wio`](../tests/numeric_literal_signed_mins_run.wio), [`const_scalar_expressions.wio`](../tests/const_scalar_expressions.wio) | [`let_without_initializer.wio`](../tests/invalid/let_without_initializer.wio), [`const_without_initializer.wio`](../tests/invalid/const_without_initializer.wio), [`const_string_not_supported.wio`](../tests/invalid/const_string_not_supported.wio), [`const_from_non_const_identifier.wio`](../tests/invalid/const_from_non_const_identifier.wio), [`const_function_call_not_supported.wio`](../tests/invalid/const_function_call_not_supported.wio), [`const_array_not_supported.wio`](../tests/invalid/const_array_not_supported.wio) |
| `ref`, `view`, `fit`, `is` | `WIO_LANGUAGE_DRAFT.md` sections 7, 8, 9 | [`interface_ref_upcast.wio`](../tests/interface_ref_upcast.wio), [`fit_numeric_clamp.wio`](../tests/fit_numeric_clamp.wio) | [`ref_requires_addressable_operand.wio`](../tests/invalid/ref_requires_addressable_operand.wio), [`ref_immutable_to_mutable_param.wio`](../tests/invalid/ref_immutable_to_mutable_param.wio), [`is_left_requires_object_or_interface.wio`](../tests/invalid/is_left_requires_object_or_interface.wio), [`is_right_requires_object_or_interface.wio`](../tests/invalid/is_right_requires_object_or_interface.wio), [`fit_requires_numeric_or_object_interface.wio`](../tests/invalid/fit_requires_numeric_or_object_interface.wio), [`if_fit_requires_is_condition.wio`](../tests/invalid/if_fit_requires_is_condition.wio) |
| `match` and `when` | `WIO_LANGUAGE_DRAFT.md` sections 13, 14 | [`match_value_basic.wio`](../tests/match_value_basic.wio), [`match_statement_basic.wio`](../tests/match_statement_basic.wio), [`when_guard_fallback_value.wio`](../tests/when_guard_fallback_value.wio) | [`match_value_requires_assumed.wio`](../tests/invalid/match_value_requires_assumed.wio), [`match_assumed_must_be_last.wio`](../tests/invalid/match_assumed_must_be_last.wio), [`match_duplicate_assumed.wio`](../tests/invalid/match_duplicate_assumed.wio), [`match_case_type_mismatch.wio`](../tests/invalid/match_case_type_mismatch.wio), [`match_return_type_mismatch.wio`](../tests/invalid/match_return_type_mismatch.wio), [`non_void_when_without_else.wio`](../tests/invalid/non_void_when_without_else.wio), [`when_condition_type_mismatch.wio`](../tests/invalid/when_condition_type_mismatch.wio), [`when_fallback_return_type_mismatch.wio`](../tests/invalid/when_fallback_return_type_mismatch.wio) |
| `for`, `foreach`, ranges, dictionary/component binding, C-style `for` | `WIO_LANGUAGE_DRAFT.md` sections 12, 24 | [`for_in_dynamic_array.wio`](../tests/for_in_dynamic_array.wio), [`for_dict_key_value.wio`](../tests/for_dict_key_value.wio), [`for_component_binding.wio`](../tests/for_component_binding.wio), [`c_style_for_loop.wio`](../tests/c_style_for_loop.wio) | [`for_in_requires_array.wio`](../tests/invalid/for_in_requires_array.wio), [`for_dict_requires_two_bindings.wio`](../tests/invalid/for_dict_requires_two_bindings.wio), [`for_component_binding_count_mismatch.wio`](../tests/invalid/for_component_binding_count_mismatch.wio), [`for_range_step_zero.wio`](../tests/invalid/for_range_step_zero.wio), [`break_outside_loop.wio`](../tests/invalid/break_outside_loop.wio), [`continue_outside_loop.wio`](../tests/invalid/continue_outside_loop.wio), [`c_style_for_invalid_condition_type.wio`](../tests/invalid/c_style_for_invalid_condition_type.wio) |
| `component`, `object`, access control, ctor rules, core attributes | `WIO_LANGUAGE_DRAFT.md` sections 15-21 | [`object_component_access_semantics.wio`](../tests/object_component_access_semantics.wio), [`component_embedded_member_ctor.wio`](../tests/component_embedded_member_ctor.wio), [`interface_ref_upcast.wio`](../tests/interface_ref_upcast.wio) | [`component_method_not_allowed.wio`](../tests/invalid/component_method_not_allowed.wio), [`object_inherits_component.wio`](../tests/invalid/object_inherits_component.wio), [`final_object_inheritance.wio`](../tests/invalid/final_object_inheritance.wio), [`default_attribute_invalid_access.wio`](../tests/invalid/default_attribute_invalid_access.wio), [`private_base_member_access.wio`](../tests/invalid/private_base_member_access.wio), [`protected_member_access_outside_hierarchy.wio`](../tests/invalid/protected_member_access_outside_hierarchy.wio), [`trust_requires_struct_type.wio`](../tests/invalid/trust_requires_struct_type.wio) |
| Default parameters, generic constraints, and function parameter packs | `WIO_LANGUAGE_DRAFT.md` sections 13, 20 | [`default_parameters_run.wio`](../tests/default_parameters_run.wio), [`default_parameter_native_bridge.wio`](../tests/native/default_parameter_native_bridge.wio), [`generic_apply_function_constraints.wio`](../tests/generic_apply_function_constraints.wio), [`generic_apply_type_and_object_run.wio`](../tests/generic_apply_type_and_object_run.wio), [`generic_native_predicate_bridge.wio`](../tests/native/generic_native_predicate_bridge.wio), [`function_parameter_pack_run.wio`](../tests/function_parameter_pack_run.wio) | [`default_parameter_non_trailing.wio`](../tests/invalid/default_parameter_non_trailing.wio), [`default_parameter_type_mismatch.wio`](../tests/invalid/default_parameter_type_mismatch.wio), [`default_parameter_declaration_only_not_supported.wio`](../tests/invalid/default_parameter_declaration_only_not_supported.wio), [`default_parameter_entry_not_allowed.wio`](../tests/invalid/default_parameter_entry_not_allowed.wio), [`default_parameter_overload_conflict.wio`](../tests/invalid/default_parameter_overload_conflict.wio), [`generic_apply_reject_function_call.wio`](../tests/invalid/generic_apply_reject_function_call.wio), [`generic_apply_reject_type_alias.wio`](../tests/invalid/generic_apply_reject_type_alias.wio), [`generic_apply_reject_constructor.wio`](../tests/invalid/generic_apply_reject_constructor.wio), [`generic_apply_non_generic_function.wio`](../tests/invalid/generic_apply_non_generic_function.wio), [`generic_native_predicate_reject_non_integer.wio`](../tests/invalid/generic_native_predicate_reject_non_integer.wio), [`generic_parameter_pack_non_trailing.wio`](../tests/invalid/generic_parameter_pack_non_trailing.wio), [`parameter_pack_non_trailing.wio`](../tests/invalid/parameter_pack_non_trailing.wio), [`parameter_pack_missing_matching_generic_pack.wio`](../tests/invalid/parameter_pack_missing_matching_generic_pack.wio), [`parameter_pack_bare_use_not_allowed.wio`](../tests/invalid/parameter_pack_bare_use_not_allowed.wio), [`parameter_pack_default_not_supported.wio`](../tests/invalid/parameter_pack_default_not_supported.wio), [`parameter_pack_method_not_supported.wio`](../tests/invalid/parameter_pack_method_not_supported.wio), [`parameter_pack_explicit_type_arguments_not_supported.wio`](../tests/invalid/parameter_pack_explicit_type_arguments_not_supported.wio) |
| Modules, realms, aliasing, multi-file lookup | `WIO_LANGUAGE_DRAFT.md` section 22 | [`test_use_alias.wio`](../tests/test_use_alias.wio), [`realm_basic.wio`](../tests/realm_basic.wio), [`realm_nested.wio`](../tests/realm_nested.wio), [`realm_qualified_from.wio`](../tests/realm_qualified_from.wio) | [`use_missing_user_module.wio`](../tests/invalid/use_missing_user_module.wio), [`use_missing_std_module.wio`](../tests/invalid/use_missing_std_module.wio), [`use_alias_conflicts_with_symbol.wio`](../tests/invalid/use_alias_conflicts_with_symbol.wio), [`realm_conflicts_with_symbol.wio`](../tests/invalid/realm_conflicts_with_symbol.wio) |
| `Entry` contract | `WIO_LANGUAGE_DRAFT.md` section 13 | [`std_contract_v1_run.wio`](../tests/std_contract_v1_run.wio) | [`entry_invalid_parameter_type.wio`](../tests/invalid/entry_invalid_parameter_type.wio), [`entry_invalid_return_type.wio`](../tests/invalid/entry_invalid_return_type.wio), [`entry_too_many_parameters.wio`](../tests/invalid/entry_too_many_parameters.wio), [`entry_missing_body.wio`](../tests/invalid/entry_missing_body.wio), [`entry_method_not_top_level.wio`](../tests/invalid/entry_method_not_top_level.wio) |

---

## 3. Std Traceability

| Module family | Reference | Representative positive tests | Representative negative tests |
| --- | --- | --- | --- |
| Stable std surface as a whole | [`WIO_STD.md`](./WIO_STD.md) sections 1-5 | [`std_contract_v1_run.wio`](../tests/std_contract_v1_run.wio) | [`missing_cpp_header_std_import.wio`](../tests/invalid/missing_cpp_header_std_import.wio), [`std_imports_private_runtime_header.wio`](../tests/invalid/std_imports_private_runtime_header.wio) |
| Language-owned collection/string member surface wrapped by std helpers | [`WIO_STD.md`](./WIO_STD.md) sections 2, 3 | [`array_member_methods.wio`](../tests/array_member_methods.wio), [`dictionary_member_methods.wio`](../tests/dictionary_member_methods.wio), [`string_member_methods.wio`](../tests/string_member_methods.wio) | [`array_member_requires_mutable_receiver.wio`](../tests/invalid/array_member_requires_mutable_receiver.wio), [`dictionary_member_requires_mutable_receiver.wio`](../tests/invalid/dictionary_member_requires_mutable_receiver.wio), [`string_member_requires_mutable_receiver.wio`](../tests/invalid/string_member_requires_mutable_receiver.wio) |

---

## 4. Host SDK and ABI Traceability

| Contract slice | Reference | Representative positive tests | Representative negative or hardening tests |
| --- | --- | --- | --- |
| Raw module API descriptor validation | [`WIO_SDK.md`](./WIO_SDK.md) sections 4, 11 | [`sdk_static_api_host.cpp`](../tests/native/sdk_static_api_host.cpp) | [`sdk_invalid_api_host.cpp`](../tests/native/sdk_invalid_api_host.cpp) |
| Top-level command/event/export loading from shared modules | [`WIO_SDK.md`](./WIO_SDK.md) sections 4, 5 | [`sdk_module_easy_host.cpp`](../tests/native/sdk_module_easy_host.cpp) | [`sdk_module_stale_bindings_host.cpp`](../tests/native/sdk_module_stale_bindings_host.cpp) |
| Exported object/component reflection and field access | [`WIO_SDK.md`](./WIO_SDK.md) sections 6, 7, 8, 9 | [`sdk_exported_types_host.cpp`](../tests/native/sdk_exported_types_host.cpp), [`sdk_exported_complex_fields_host.cpp`](../tests/native/sdk_exported_complex_fields_host.cpp) | [`sdk_invalid_api_host.cpp`](../tests/native/sdk_invalid_api_host.cpp) |
| Hot reload boundary and stale wrapper behavior | [`WIO_SDK.md`](./WIO_SDK.md) section 10 | [`sdk_module_hot_reload_host.cpp`](../tests/native/sdk_module_hot_reload_host.cpp) | [`sdk_hot_reload_stale_wrappers_host.cpp`](../tests/native/sdk_hot_reload_stale_wrappers_host.cpp), [`sdk_module_stale_bindings_host.cpp`](../tests/native/sdk_module_stale_bindings_host.cpp) |

---

## 5. Diagnostic Coverage Notes

The invalid corpus now mixes two useful layers:

- failure-by-feature tests that assert at least the key diagnostic message
- cleaner failure tests for selected high-value diagnostics where Wio output must
  stay free from backend-only noise

Current representative message-asserted areas include:

- `Entry` contract failures
- missing std/native C++ header diagnostics
- import/module lookup failures
- initialization and `const` failures
- loop semantic failures
- backend note/line-mapping diagnostics

That is enough for the current stabilized surface to stay anchored while the
rest of the invalid corpus continues to grow.
