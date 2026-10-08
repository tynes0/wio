# Interop test registrations. Included in the app directory scope.

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
wio_test_labels(wio_test_sdk_invalid_api_contract interop traceability)

set_tests_properties(
    wio_test_sdk_invalid_api_contract
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
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
wio_test_labels(wio_test_example_native_app_run interop)

set_tests_properties(
    wio_test_example_native_app_run
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_success_test(
    wio_test_wio_tool_run_host_interop_dry_run
    "${CMAKE_SOURCE_DIR}/scripts/wio/run_host_interop.wio"
)
wio_test_labels(wio_test_wio_tool_run_host_interop_dry_run interop)

add_wio_output_test(
    wio_test_native_resource_owned_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_resource_owned_run.wio"
    "native-resource-owned-ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_resource_owned_run interop)

add_wio_output_test(
    wio_test_generic_native_default_instantiation_run
    "${CMAKE_SOURCE_DIR}/tests/native/generic_native_default_instantiation_run.wio"
    "Generic native defaults: ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_generic_native_default_instantiation_run interop)

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
wio_test_labels(wio_test_native_bridge_run interop)

add_wio_output_test(
    wio_test_native_cstring_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_cstring_bridge.wio"
    "Native cstring bridge: 3, 3, true"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_cstring_bridge_run interop)

add_wio_output_test(
    wio_test_native_opaque_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_opaque_bridge.wio"
    "Opaque bridge: true, true, true, false, 6, 6, true"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_opaque_bridge_run interop traceability)

add_wio_output_test(
    wio_test_native_ref_params_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_ref_params_bridge.wio"
    "Native refs: 12, 13, Hi!, true, 3, 10"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_ref_params_bridge_run interop)

add_wio_output_test(
    wio_test_native_any_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_any_bridge.wio"
    "native-any-ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_any_bridge_run interop)

add_wio_output_test(
    wio_test_native_box_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_box_bridge.wio"
    "native-box-ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_box_bridge_run interop traceability)

add_wio_output_test(
    wio_test_native_generic_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_generic_bridge.wio"
    "Generic native double: 42, 42"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_generic_bridge_run interop)

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
wio_test_labels(wio_test_native_object_component_bridge_run interop)

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
wio_test_labels(wio_test_native_pod_component_bridge_run interop)

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
wio_test_labels(wio_test_native_component_extension_run interop)

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
wio_test_labels(wio_test_native_generic_pod_component_bridge_run interop)

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
wio_test_labels(wio_test_native_generic_pod_array_component_bridge_run interop)

add_wio_output_test(
    wio_test_native_generic_specialized_component_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_generic_specialized_component_run.wio"
    "Native generic component specialization: ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_generic_specialized_component_run interop)

add_wio_output_test(
    wio_test_native_enum_flagset_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_enum_flagset_bridge.wio"
    "native-enum-flagset-ok"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_enum_flagset_bridge_run interop)

add_wio_output_test(
    wio_test_generic_native_predicate_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/generic_native_predicate_bridge.wio"
    "Generic native predicate: 14, 18"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_generic_native_predicate_bridge_run interop traceability)

add_wio_output_test(
    wio_test_default_parameter_native_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/default_parameter_native_bridge.wio"
    "Native default parameters: 12, 16"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_default_parameter_native_bridge_run interop traceability)

add_wio_output_test(
    wio_test_generic_native_forward_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/generic_native_forward_bridge.wio"
    "Generic native forward: 42, 42"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_generic_native_forward_bridge_run interop)

add_wio_output_test(
    wio_test_native_generic_pair_bridge_run
    "${CMAKE_SOURCE_DIR}/tests/native/native_generic_pair_bridge.wio"
    "Generic native pair: 42"
    --run
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_generic_pair_bridge_run interop)

add_wio_host_interop_test(
    wio_test_static_export_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/static_host.cpp"
    "Static export sum: 42"
)
wio_test_labels(wio_test_static_export_host_interop interop)

add_wio_host_interop_test(
    wio_test_unicode_text_export_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/unicode_text_export.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/unicode_text_export_host.cpp"
    "Text export: valid=true count=9"
)
wio_test_labels(wio_test_unicode_text_export_host_interop interop coherence)

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
wio_test_labels(wio_test_sdk_static_api_interop interop)

add_wio_shared_host_interop_test(
    wio_test_shared_export_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/shared_host.cpp"
    "Shared export sum: 42"
)
wio_test_labels(wio_test_shared_export_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_generic_export_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/generic_exported_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/generic_export_host.cpp"
    "Generic export instantiate: table=1 exports=3 i32=41 f32=.* bool=false"
)
wio_test_labels(wio_test_generic_export_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_module_lifecycle_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle_host.cpp"
    "Module lifecycle: table=1 caps=463 schema=0 exports=4 commands=2 events=2 v1 load=0 counter=48 add=51"
)
wio_test_labels(wio_test_module_lifecycle_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_module_easy_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/module_lifecycle.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_module_easy_host.cpp"
    "SDK easy: before=42 afterAdd=45 afterTick=51 exports=4 commands=2 events=2"
)
wio_test_labels(wio_test_sdk_module_easy_host_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_sdk_module_stale_bindings_interop
    "${CMAKE_SOURCE_DIR}/tests/native/module_stale_bindings.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_module_stale_bindings_host.cpp"
    "SDK module stale: before=5 afterAdd=12 stale=5 restarted=5"
)
wio_test_labels(wio_test_sdk_module_stale_bindings_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_sdk_exported_types_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_types_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_types_host.cpp"
    "SDK exported types: exports=14 types=2 objectFields=3 objectMethods=2 start=7 afterAdd=25 mixed=41 componentFields=2 hp=100 hpAfter=135 maxHp=150"
)
wio_test_labels(wio_test_sdk_exported_types_host_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_sdk_exported_ctor_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_ctor_types_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_ctor_host.cpp"
    "SDK ctor exported types: exports=11 types=2 enemy=15 afterDamage=11 point=4,9 ctorCount=1/1"
)
wio_test_labels(wio_test_sdk_exported_ctor_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_exported_inherited_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_inherited_types_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_inherited_host.cpp"
    "SDK inherited exported types: exports=13 types=1 fields=3 methods=5 hp=40 id=2 phase=3 score0=42 score1=47 enrage=5 heal=49 score2=59"
)
wio_test_labels(wio_test_sdk_exported_inherited_host_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_sdk_exported_complex_fields_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_complex_fields_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_exported_complex_fields_host.cpp"
    "SDK complex fields: fields=10 title=boss tags=2 hp=30 callback=21 mode=0 profile=77 position=11 fixedSum=24"
)
wio_test_labels(wio_test_sdk_exported_complex_fields_host_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_sdk_013_parity_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_013_parity_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_013_parity_host.cpp"
    "SDK 0.13 parity: version=0\\.[0-9]+\\.[0-9]+ text-codepoints=10 generic=std::Option<i32>"
)
wio_test_labels(wio_test_sdk_013_parity_host_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_option_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_option_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_option_host.cpp"
    "SDK 0.14 Option: selected=7 nested=9 empty-roundtrip=ok"
)
wio_test_labels(wio_test_sdk_014_option_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_result_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_result_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_result_host.cpp"
    "SDK 0.14 Result: value=84 error-code=701 unit=ok native-error=12345"
)
wio_test_labels(wio_test_sdk_014_result_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_nested_collections_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_nested_collections_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_nested_collections_host.cpp"
    "SDK 0.14 nested: choices=2 fixed-error=501 wide=18000000000 ordered-error=502"
)
wio_test_labels(wio_test_sdk_014_nested_collections_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_sequence_containers_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_sequence_containers_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_sequence_containers_host.cpp"
    "SDK 0.14 sequences: queue=2 tags=2 levels=4-9"
)
wio_test_labels(wio_test_sdk_014_sequence_containers_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_tuple_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_tuple_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_tuple_host.cpp"
    "SDK 0.14 tuple: selected=none error=730 revision=9000000000"
)
wio_test_labels(wio_test_sdk_014_tuple_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_span_range_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_span_range_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_span_range_host.cpp"
    "SDK 0.14 span: start=2 count=2 values=30-40"
)
wio_test_labels(wio_test_sdk_014_span_range_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_byte_buffer_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_byte_buffer_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_byte_buffer_host.cpp"
    "SDK 0.14 buffer: count=3 position=1 capacity=32 bytes=52-18-171"
)
wio_test_labels(wio_test_sdk_014_byte_buffer_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_014_const_generic_metadata_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_const_generic_metadata_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_014_const_generic_metadata_host.cpp"
    "SDK 0.14 const metadata: abi=11 block=4 label=sdk empty=ok"
)
wio_test_labels(wio_test_sdk_014_const_generic_metadata_host_interop interop)

add_wio_shared_host_interop_test(
    wio_test_sdk_015_attribute_metadata_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_015_attribute_metadata_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_015_attribute_metadata_host.cpp"
    "SDK 0.15 attributes: abi=11 type=Label field=Important method=Audited phase=pre"
)
wio_test_labels(wio_test_sdk_015_attribute_metadata_host_interop interop freeze)

add_wio_shared_host_interop_test(
    wio_test_sdk_016_application_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_application_host_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_application_host.cpp"
    "sdk-application-host-ok frames=3 exit=17 wrong-thread=1 stale=1"
)
wio_test_labels(wio_test_sdk_016_application_host_interop interop freeze)

add_wio_shared_host_interop_test(
    wio_test_sdk_016_async_task_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_async_task_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_async_task_host.cpp"
    "sdk-async-task-ok value=42 timeout=1 cancel=1 callbacks=2 stale=1"
)
wio_test_labels(wio_test_sdk_016_async_task_host_interop interop freeze)

add_wio_shared_host_interop_test(
    wio_test_sdk_016_fixed_application_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_fixed_application_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_fixed_application_host.cpp"
    "sdk-fixed-application-ok frames=6 ticks=3 exit=23"
)
wio_test_labels(wio_test_sdk_016_fixed_application_host_interop interop freeze)

add_wio_shared_host_interop_test(
    wio_test_sdk_017_application_schedule_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_017_application_schedule_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_017_application_schedule_host.cpp"
    "sdk-application-schedule-ok stages=4 fixed=20 main=Render"
)
wio_test_labels(wio_test_sdk_017_application_schedule_interop interop freeze)

add_wio_shared_host_interop_test(
    wio_test_sdk_016_application_rollback_interop
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_application_rollback_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_016_application_rollback_host.cpp"
    "rollback-ready-start[ \t\r\n]*rollback-failing-start[ \t\r\n]*rollback-ready-close[ \t\r\n]*rollback-application-close[ \t\r\n]*sdk-application-rollback-ok closed=1 restart=blocked"
)
wio_test_labels(wio_test_sdk_016_application_rollback_interop interop freeze)

add_wio_shared_reload_interop_test(
    wio_test_module_reload_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/module_reload.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/module_reload_host.cpp"
    "Module reload: table=1 caps=511 schema=1 exports=4 commands=2 events=2 v1 load=0 restore=0 before=42 after=42 tick=48 add=51"
)
wio_test_labels(wio_test_module_reload_host_interop interop)

add_wio_shared_reload_interop_test(
    wio_test_sdk_module_hot_reload_interop
    "${CMAKE_SOURCE_DIR}/tests/native/module_reload.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_module_hot_reload_host.cpp"
    "SDK reload: before=40 afterAdd=43 afterReload=43 afterTick=49 generation=2"
)
wio_test_labels(wio_test_sdk_module_hot_reload_interop interop traceability)

add_wio_shared_reload_interop_test(
    wio_test_sdk_hot_reload_stale_wrappers_interop
    "${CMAKE_SOURCE_DIR}/tests/native/exported_types_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/sdk_hot_reload_stale_wrappers_host.cpp"
    "SDK reload stale wrappers: start=7 afterAdd=12 hp=100 stale=6 fresh=17 freshHp=100 generation=2"
)
wio_test_labels(wio_test_sdk_hot_reload_stale_wrappers_interop interop traceability)

add_wio_shared_host_interop_test(
    wio_test_pack_export_host_interop
    "${CMAKE_SOURCE_DIR}/tests/native/pack_exported_library.wio"
    "${CMAKE_SOURCE_DIR}/tests/native/pack_export_host.cpp"
    "Pack export instantiate: exports=2 pair=2 triple=3"
)
wio_test_labels(wio_test_pack_export_host_interop interop traceability)

add_wio_emit_cpp_file_test(
    wio_test_native_pod_component_emits_alias
    "${CMAKE_SOURCE_DIR}/tests/native/native_pod_component_bridge.wio"
    "using [^\\n]* = vec2;"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_pod_component_emits_alias interop)

add_wio_emit_cpp_file_test(
    wio_test_native_pod_component_ref_dispatch_uses_direct_alias
    "${CMAKE_SOURCE_DIR}/tests/native/native_pod_component_bridge.wio"
    "native_pod::Translate\\([^;]*\\.read\\(\\), [^,]+, [^)]+\\);"
    --include-dir
    "${CMAKE_SOURCE_DIR}/tests/native"
)
wio_test_labels(wio_test_native_pod_component_ref_dispatch_uses_direct_alias interop)

add_wio_output_test(
    wio_test_std_console_native_types_run
    "${CMAKE_SOURCE_DIR}/tests/std_console_native_types_run.wio"
    "IO std-console-native-ok"
    --run
)
wio_test_labels(wio_test_std_console_native_types_run interop)

add_wio_emit_cpp_file_test(
    wio_test_exported_nullable_descriptor
    "${CMAKE_SOURCE_DIR}/tests/native/exported_nullable_descriptor_library.wio"
    "WIO_MODULE_TYPE_DESC_NULLABLE"
    --target
    shared
)
wio_test_labels(wio_test_exported_nullable_descriptor interop)

add_wio_output_test(
    wio_test_attribute_with_using_native_run
    "${CMAKE_SOURCE_DIR}/tests/attribute_with_using_native_run.wio"
    "attribute-with-using-native-ok"
    --run
)
wio_test_labels(wio_test_attribute_with_using_native_run interop freeze)

set_tests_properties(
    wio_test_attribute_with_using_native_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

# Real generated wire thunks, not mock registries: identical Wio program under
# the C++ backend and VM, with independent expected-result oracles.
add_test(
    NAME wio_vm_native_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-differential"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_differential_test.cmake"
)
wio_test_labels(wio_vm_native_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_layout_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-layout"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_layout_test.cmake"
)
wio_test_labels(wio_vm_native_pod_layout_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_layout_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_array_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_pod_array.wio"
        "-DWIO_EXPECTED_OUTPUT=native-pod-array-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-array"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_layout_test.cmake"
)
wio_test_labels(wio_vm_native_pod_array_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_array_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_pointer_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_pod_pointer.wio"
        "-DWIO_EXPECTED_OUTPUT=native-pod-pointer-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-pointer"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_layout_test.cmake"
)
wio_test_labels(wio_vm_native_pod_pointer_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_pointer_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_pointer_rejection
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_pointer_rejection_test.cmake"
)
wio_test_labels(wio_vm_native_pod_pointer_rejection interop vm native native-differential invalid release-gate)
set_tests_properties(wio_vm_native_pod_pointer_rejection PROPERTIES TIMEOUT 90)

add_test(
    NAME wio_vm_native_pod_template_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_pod_template.wio"
        "-DWIO_EXPECTED_OUTPUT=native-pod-template-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-template"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_layout_test.cmake"
)
wio_test_labels(wio_vm_native_pod_template_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_template_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_template_rejection
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_template_rejection_test.cmake"
)
wio_test_labels(wio_vm_native_pod_template_rejection interop vm native native-differential invalid release-gate)
set_tests_properties(wio_vm_native_pod_template_rejection PROPERTIES TIMEOUT 90)

add_test(
    NAME wio_vm_native_pod_default_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_pod_default.wio"
        "-DWIO_EXPECTED_OUTPUT=native-pod-default-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-default"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_layout_test.cmake"
)
wio_test_labels(wio_vm_native_pod_default_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_default_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_default_only
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_pod_default_only.wio"
        "-DWIO_EXPECTED_OUTPUT=^$"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-default-only"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_layout_test.cmake"
)
wio_test_labels(wio_vm_native_pod_default_only interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_default_only PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_pod_alias_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_pod_alias.wio"
        "-DWIO_EXPECTED_OUTPUT=native-pod-alias-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-pod-alias"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DWIO_ALIAS_CONTRACT_EXECUTABLE=$<TARGET_FILE:wio_vm_native_pod_alias_test>"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_pod_alias_test.cmake"
)
wio_test_labels(wio_vm_native_pod_alias_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_pod_alias_differential PROPERTIES TIMEOUT 420)

add_test(
    NAME wio_vm_native_sequence_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_sequence.wio"
        "-DWIO_EXPECTED_OUTPUT=native-sequence-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-sequence"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_sequence_test.cmake"
)
wio_test_labels(wio_vm_native_sequence_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_sequence_differential PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_dictionary_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_dictionary.wio"
        "-DWIO_EXPECTED_OUTPUT=native-dictionary-ok:"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-dictionary"
        "-DWIO_DICTIONARY_CONTRACT_EXECUTABLE=$<TARGET_FILE:wio_vm_native_dictionary_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_dictionary_test.cmake"
)
wio_test_labels(wio_vm_native_dictionary_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_dictionary_differential PROPERTIES TIMEOUT 900)

add_test(
    NAME wio_vm_native_any_differential
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_any.wio"
        "-DWIO_EXPECTED_OUTPUT=native-any-ok:17"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-any"
        "-DWIO_ANY_CONTRACT_EXECUTABLE=$<TARGET_FILE:wio_vm_native_any_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_any_test.cmake"
)
wio_test_labels(wio_vm_native_any_differential interop vm native native-differential differential release-gate)
set_tests_properties(wio_vm_native_any_differential PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_proxy
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-proxy"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_proxy interop vm native native-object release-gate)
set_tests_properties(wio_vm_native_object_proxy PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_materialization
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-materialization"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_AUTOMATIC_OBJECTS=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_materialization interop vm native native-object differential release-gate)
set_tests_properties(wio_vm_native_object_materialization PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_method_values
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-method-values"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_METHOD_VALUES=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_method_values interop vm native native-object differential release-gate)
set_tests_properties(wio_vm_native_object_method_values PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_method_references
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-method-references"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_METHOD_REFERENCES=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_method_references interop vm native native-object differential release-gate)
set_tests_properties(wio_vm_native_object_method_references PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_method_nullable
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-method-nullable"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_METHOD_NULLABLE=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_method_nullable interop vm native native-object differential release-gate)
set_tests_properties(wio_vm_native_object_method_nullable PROPERTIES TIMEOUT 600)
