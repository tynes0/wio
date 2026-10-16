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

add_test(
    NAME wio_vm_native_object_method_nullable_references
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-method-nullable-references"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_METHOD_NULLABLE_REFERENCES=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_method_nullable_references interop vm native native-object differential release-gate)
set_tests_properties(wio_vm_native_object_method_nullable_references PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_method_borrowed_results
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-method-borrowed-results"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_METHOD_BORROWED_RESULTS=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_method_borrowed_results interop vm native native-object differential release-gate)
set_tests_properties(wio_vm_native_object_method_borrowed_results PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_concrete_generics
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-concrete-generics"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_CONCRETE_GENERICS=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_concrete_generics interop vm native native-object generic differential release-gate)
set_tests_properties(wio_vm_native_object_concrete_generics PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_hierarchy
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-hierarchy"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_HIERARCHY=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_hierarchy interop vm native native-object inheritance interface differential release-gate)
set_tests_properties(wio_vm_native_object_hierarchy PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_polymorphic
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-polymorphic"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_POLYMORPHIC=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_polymorphic interop vm native native-object inheritance interface differential release-gate)
set_tests_properties(wio_vm_native_object_polymorphic PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_polymorphic_references
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-polymorphic-references"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -DWIO_OBJECT_POLYMORPHIC_REFERENCES=ON
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_proxy_test.cmake"
)
wio_test_labels(wio_vm_native_object_polymorphic_references interop vm native native-object inheritance interface borrowed-result differential release-gate)
set_tests_properties(wio_vm_native_object_polymorphic_references PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_object_any
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_object_any.wio"
        "-DWIO_EXPECTED_OUTPUT=native-object-any-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-object-any"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_object_any_test.cmake"
)
wio_test_labels(wio_vm_native_object_any interop vm native native-object any generic differential release-gate)
set_tests_properties(wio_vm_native_object_any PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_any_generic
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_any_generic.wio"
        "-DWIO_EXPECTED_OUTPUT=native-any-generic-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-any-generic"
        "-DWIO_GENERIC_ANY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_any_generic_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_any_generic_test.cmake"
)
wio_test_labels(wio_vm_native_any_generic interop vm native any generic container differential release-gate)
set_tests_properties(wio_vm_native_any_generic PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_any_nullable
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_any_nullable.wio"
        "-DWIO_EXPECTED_OUTPUT=native-nullable-any-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-any-nullable"
        "-DWIO_OBJECT_PROXY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_object_proxy_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_any_nullable_test.cmake"
)
wio_test_labels(wio_vm_native_any_nullable interop vm native any nullable generic differential release-gate)
set_tests_properties(wio_vm_native_any_nullable PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_handle_containers
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_handle_containers.wio"
        "-DWIO_EXPECTED_OUTPUT=native-handle-containers-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-handle-containers"
        "-DWIO_CONTAINER_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_handle_containers_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_handle_containers_test.cmake"
)
wio_test_labels(wio_vm_native_handle_containers interop vm native any native-object container generic nullable differential release-gate)
set_tests_properties(wio_vm_native_handle_containers PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_free_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_free_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-free-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-free-borrow"
        "-DWIO_BORROW_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_free_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_free_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_free_borrow interop vm native borrowed-result nullable container differential release-gate)
set_tests_properties(wio_vm_native_free_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_nested_any
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_nested_any.wio"
        "-DWIO_EXPECTED_OUTPUT=native-nested-any-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-nested-any"
        "-DWIO_NESTED_ANY_EXECUTABLE=$<TARGET_FILE:wio_vm_native_nested_any_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_nested_any_test.cmake"
)
wio_test_labels(wio_vm_native_nested_any interop vm native any container differential release-gate)
set_tests_properties(wio_vm_native_nested_any PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_container_shape
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_container_shape.wio"
        "-DWIO_EXPECTED_OUTPUT=native-container-shape-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-container-shape"
        "-DWIO_SHAPE_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_container_shape_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_container_shape_test.cmake"
)
wio_test_labels(wio_vm_native_container_shape interop vm native container borrow cleanup differential release-gate)
set_tests_properties(wio_vm_native_container_shape PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_sequence_projection
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_sequence_projection.wio"
        "-DWIO_EXPECTED_OUTPUT=native-sequence-projection-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-sequence-projection"
        "-DWIO_PROJECTION_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_sequence_projection_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_sequence_projection_test.cmake"
)
wio_test_labels(wio_vm_native_sequence_projection interop vm native container projection borrowed-result differential release-gate)
set_tests_properties(wio_vm_native_sequence_projection PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_sequence_pod_projection
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_sequence_pod_projection.wio"
        "-DWIO_EXPECTED_OUTPUT=native-sequence-pod-projection-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-sequence-pod-projection"
        "-DWIO_PROJECTION_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_sequence_pod_projection_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_sequence_pod_projection_test.cmake"
)
wio_test_labels(wio_vm_native_sequence_pod_projection interop vm native container pod projection differential release-gate)
set_tests_properties(wio_vm_native_sequence_pod_projection PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_dictionary_projection
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_dictionary_projection.wio"
        "-DWIO_EXPECTED_OUTPUT=native-dictionary-projection-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-dictionary-projection"
        "-DWIO_PROJECTION_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_dictionary_projection_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_dictionary_projection_test.cmake"
)
wio_test_labels(wio_vm_native_dictionary_projection interop vm native container dictionary projection differential release-gate)
set_tests_properties(wio_vm_native_dictionary_projection PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_projected_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_projected_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-projected-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-projected-borrow"
        "-DWIO_PROJECTION_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_projected_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_projected_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_projected_borrow interop vm native container pod projection borrowed-result differential release-gate)
set_tests_properties(wio_vm_native_projected_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_dictionary_intrinsic
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_dictionary_intrinsic.wio"
        "-DWIO_EXPECTED_OUTPUT=native-dictionary-intrinsic-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-dictionary-intrinsic"
        "-DWIO_DICTIONARY_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_dictionary_intrinsic_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_dictionary_intrinsic_test.cmake"
)
wio_test_labels(wio_vm_native_dictionary_intrinsic interop vm native container dictionary projection borrowed-result cleanup differential release-gate)
set_tests_properties(wio_vm_native_dictionary_intrinsic PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_projected_any_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_projected_any_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-projected-any-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-projected-any-borrow"
        "-DWIO_ANY_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_projected_any_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_projected_any_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_projected_any_borrow interop vm native any container projection borrowed-result cleanup differential release-gate)
set_tests_properties(wio_vm_native_projected_any_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_projected_object_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_projected_object_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-projected-object-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-projected-object-borrow"
        "-DWIO_OBJECT_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_projected_object_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_projected_object_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_projected_object_borrow interop vm native object generic nullable projection borrowed-result cleanup differential release-gate)
set_tests_properties(wio_vm_native_projected_object_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_pod
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_pod.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-pod-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-pod"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_pod_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_pod_test.cmake"
)
wio_test_labels(wio_vm_native_callback_pod interop vm native callback pod lifetime differential release-gate)
set_tests_properties(wio_vm_native_callback_pod PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_object
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_object.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-object-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-object"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_object_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_object_test.cmake"
)
wio_test_labels(wio_vm_native_callback_object interop vm native callback object nullable lifetime cleanup differential release-gate)
set_tests_properties(wio_vm_native_callback_object PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_container
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_container.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-container-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-container"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_container_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_container_test.cmake"
)
wio_test_labels(wio_vm_native_callback_container interop vm native callback container any ownership cleanup differential release-gate)
set_tests_properties(wio_vm_native_callback_container PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_hosted_callback
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_hosted_callback.wio"
        "-DWIO_EXPECTED_OUTPUT=hosted-callback-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-hosted-callback"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_hosted_callback_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_hosted_callback_test.cmake"
)
wio_test_labels(wio_vm_native_hosted_callback interop vm native callback object ownership differential release-gate)
set_tests_properties(wio_vm_native_hosted_callback PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-borrow"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_callback_borrow interop vm native callback borrow lifetime cleanup differential release-gate)
set_tests_properties(wio_vm_native_callback_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_string_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_string_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-string-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-string-borrow"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_string_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_string_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_callback_string_borrow interop vm native callback borrow string unicode ownership differential release-gate)
set_tests_properties(wio_vm_native_callback_string_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_value_borrow
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_value_borrow.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-value-borrow-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-value-borrow"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_value_borrow_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_value_borrow_test.cmake"
)
wio_test_labels(wio_vm_native_callback_value_borrow interop vm native callback borrow container object ownership cleanup differential release-gate)
set_tests_properties(wio_vm_native_callback_value_borrow PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_fixed_array
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_fixed_array.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-fixed-array-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-fixed-array"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_fixed_array_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_fixed_array_test.cmake"
)
wio_test_labels(wio_vm_native_callback_fixed_array interop vm native callback array borrow ownership cleanup differential release-gate)
set_tests_properties(wio_vm_native_callback_fixed_array PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_callback_callable_results
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_callback_callable_results.wio"
        "-DWIO_EXPECTED_OUTPUT=native-callback-callable-results-ok:30"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-callback-callable-results"
        "-DWIO_CALLBACK_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_callback_callable_results_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_callback_callable_results_test.cmake"
)
wio_test_labels(wio_vm_native_callback_callable_results interop vm native callback callable borrow lifetime cleanup differential release-gate)
set_tests_properties(wio_vm_native_callback_callable_results PROPERTIES TIMEOUT 600)

add_test(
    NAME wio_vm_native_ownership_alias
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_ownership_alias.wio"
        "-DWIO_EXPECTED_OUTPUT=native-ownership-alias-ok:40"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-ownership-alias"
        "-DWIO_OWNERSHIP_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_ownership_alias_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_ownership_alias_test.cmake"
)
wio_test_labels(wio_vm_native_ownership_alias interop vm native ownership consume box alias differential release-gate)
set_tests_properties(wio_vm_native_ownership_alias PROPERTIES TIMEOUT 600)

add_test(NAME wio_vm_native_bridge_pruning
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-bridge-pruning"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_bridge_pruning_test.cmake")
wio_test_labels(wio_vm_native_bridge_pruning interop vm native pruning any callback lifecycle async differential release-gate)
set_tests_properties(wio_vm_native_bridge_pruning PROPERTIES TIMEOUT 600)

add_test(NAME wio_vm_native_bridge_project
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-bridge-project"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_bridge_project_test.cmake")
wio_test_labels(wio_vm_native_bridge_project interop cli project vm native pruning incremental release-gate)
set_tests_properties(wio_vm_native_bridge_project PROPERTIES TIMEOUT 480)

add_test(NAME wio_vm_native_any_array_identity
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_any_array_identity.wio"
        "-DWIO_EXPECTED_OUTPUT=native-any-array-identity-ok"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-any-array-identity"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_bridge_pruning_test.cmake")
wio_test_labels(wio_vm_native_any_array_identity interop vm native any array generic differential release-gate)
set_tests_properties(wio_vm_native_any_array_identity PROPERTIES TIMEOUT 600)

add_test(NAME wio_wir_inferred_array_calls
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/wir_inferred_array_calls.wio"
        "-DWIO_EXPECTED_OUTPUT=inferred-array-calls-ok"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-inferred-array-calls"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_bridge_pruning_test.cmake")
wio_test_labels(wio_wir_inferred_array_calls interop wir vm native array generic differential release-gate)
set_tests_properties(wio_wir_inferred_array_calls PROPERTIES TIMEOUT 600)

add_test(NAME wio_vm_native_task_inputs
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_native_task_inputs.wio"
        "-DWIO_EXPECTED_OUTPUT=native-task-inputs-ok"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-task-inputs"
        "-DWIO_TASK_INPUT_HOST_EXECUTABLE=$<TARGET_FILE:wio_vm_native_task_inputs_host_test>"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_task_inputs_test.cmake")
wio_test_labels(wio_vm_native_task_inputs interop vm native async task std ownership lifetime differential release-gate)
set_tests_properties(wio_vm_native_task_inputs PROPERTIES TIMEOUT 600)

add_test(NAME wio_vm_std_async_task_inputs
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_std_async_task_inputs.wio"
        "-DWIO_EXPECTED_OUTPUT=std-async-task-inputs-ok"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-std-async-task-inputs"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_bridge_pruning_test.cmake")
wio_test_labels(wio_vm_std_async_task_inputs interop vm native async task std differential release-gate)
set_tests_properties(wio_vm_std_async_task_inputs PROPERTIES TIMEOUT 600)

add_test(NAME wio_vm_executable_artifact
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_VM_RUNNER=$<TARGET_FILE:wio_vm_runner>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_VM_ARCHIVE=$<TARGET_FILE_NAME:wio_vm>"
        "-DWIO_BYTECODE_ARCHIVE=$<TARGET_FILE_NAME:wio_bytecode_core>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-executable-artifact"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_executable_artifact_test.cmake")
wio_test_labels(wio_vm_executable_artifact interop vm artifact executable deployment native async release-gate)
set_tests_properties(wio_vm_executable_artifact PROPERTIES TIMEOUT 240)

add_test(NAME wio_vm_project_artifact
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_EXTRA_REGISTRY=$<TARGET_FILE:wio_vm_artifact_extra_registry>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-project-artifact"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_project_artifact_test.cmake")
wio_test_labels(wio_vm_project_artifact interop vm artifact project cli native async incremental deployment release-gate)
set_tests_properties(wio_vm_project_artifact PROPERTIES TIMEOUT 900)

add_test(NAME wio_vm_sdk_module_artifacts
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-sdk-module-artifacts"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_sdk_module_artifacts_test.cmake")
wio_test_labels(wio_vm_sdk_module_artifacts interop vm sdk module shared static deployment ownership release-gate)
set_tests_properties(wio_vm_sdk_module_artifacts PROPERTIES TIMEOUT 360)

add_test(NAME wio_vm_sdk_session_artifacts
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-sdk-session-artifacts"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_sdk_session_artifacts_test.cmake")
wio_test_labels(wio_vm_sdk_session_artifacts interop vm sdk session lifecycle shared static deployment ownership thread release-gate)
set_tests_properties(wio_vm_sdk_session_artifacts PROPERTIES TIMEOUT 360)

add_test(NAME wio_vm_sdk_async_artifacts
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-sdk-async-artifacts"
        "-DWIO_LIBRARY_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_sdk_async_library.wio"
        "-DWIO_HOST_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_sdk_async_artifact_host.cpp"
        "-DWIO_HOST_PROJECT=${CMAKE_SOURCE_DIR}/tests/projects/vm_sdk_async_host"
        "-DWIO_HOST_TARGET=vm_async_host"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_sdk_session_artifacts_test.cmake")
wio_test_labels(wio_vm_sdk_async_artifacts interop vm sdk async task shared static deployment ownership thread release-gate)
set_tests_properties(wio_vm_sdk_async_artifacts PROPERTIES TIMEOUT 360)

add_test(NAME wio_vm_sdk_task_inputs_artifacts
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-sdk-task-inputs-artifacts"
        "-DWIO_LIBRARY_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_sdk_task_inputs_library.wio"
        "-DWIO_BAD_LIBRARY_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_sdk_task_inputs_unsupported_library.wio"
        "-DWIO_HOST_SOURCE=${CMAKE_SOURCE_DIR}/tests/vm_sdk_task_inputs_artifact_host.cpp"
        "-DWIO_HOST_PROJECT=${CMAKE_SOURCE_DIR}/tests/projects/vm_sdk_task_inputs_host"
        "-DWIO_HOST_TARGET=vm_task_inputs_host"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_sdk_task_inputs_artifacts_test.cmake")
wio_test_labels(wio_vm_sdk_task_inputs_artifacts interop vm sdk async task task-input shared static deployment ownership thread release-gate)
set_tests_properties(wio_vm_sdk_task_inputs_artifacts PROPERTIES TIMEOUT 360)

add_test(NAME wio_vm_sdk_native_dependencies
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-sdk-native-dependencies"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_sdk_native_dependencies_test.cmake")
wio_test_labels(wio_vm_sdk_native_dependencies interop vm sdk native deployment shared static ownership lifecycle release-gate)
set_tests_properties(wio_vm_sdk_native_dependencies PROPERTIES TIMEOUT 360)

add_test(NAME wio_vm_native_bundle_policy
    COMMAND ${CMAKE_COMMAND} "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-bundle-policy"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_bundle_policy_test.cmake")
wio_test_labels(wio_vm_native_bundle_policy vm native deployment integrity release-gate)
set_tests_properties(wio_vm_native_bundle_policy PROPERTIES TIMEOUT 30)

add_test(NAME wio_vm_sdk_native_install
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-sdk-native-install"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_sdk_native_install_test.cmake")
wio_test_labels(wio_vm_sdk_native_install interop vm sdk native deployment install repair shared static ownership release-gate)
set_tests_properties(wio_vm_sdk_native_install PROPERTIES TIMEOUT 480)

add_test(NAME wio_vm_project_library_cli
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DWIO_MINGW=${MINGW}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-project-library-cli"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_project_library_cli_test.cmake")
wio_test_labels(wio_vm_project_library_cli interop vm project cli shared static host artifact deployment release-gate)
set_tests_properties(wio_vm_project_library_cli PROPERTIES TIMEOUT 1200)

add_test(NAME wio_vm_project_record_policy
    COMMAND ${CMAKE_COMMAND} "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-project-record-policy"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_project_record_policy_test.cmake")
wio_test_labels(wio_vm_project_record_policy vm project cli artifact integrity release-gate)
set_tests_properties(wio_vm_project_record_policy PROPERTIES TIMEOUT 30)

add_test(NAME wio_vm_executable_cohort
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-executable-cohort"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_executable_cohort_test.cmake")
wio_test_labels(wio_vm_executable_cohort interop vm native project cli artifact integrity deployment release-gate)
set_tests_properties(wio_vm_executable_cohort PROPERTIES TIMEOUT 600)

add_test(NAME wio_backend_unicode_outputs
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_VM_RUNNER=$<TARGET_FILE:wio_vm_runner>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/backend-unicode-outputs"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/backend_unicode_outputs_test.cmake")
wio_test_labels(wio_backend_unicode_outputs native platform artifact unicode publication release-gate backend-closure)
set_tests_properties(wio_backend_unicode_outputs PROPERTIES TIMEOUT 600)

add_test(NAME wio_vm_platform_qualification
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        "-DWIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DWIO_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWIO_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DWIO_MINGW=${MINGW}"
        "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-platform-qualification"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_platform_qualification_test.cmake")
wio_test_labels(wio_vm_platform_qualification vm native platform cli install deployment ownership release-gate backend-closure)
set_tests_properties(wio_vm_platform_qualification PROPERTIES TIMEOUT 1200)

add_test(NAME wio_wir_cpp_scalar_storage
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_PLAN_TEST=$<TARGET_FILE:wio_wir_cpp_storage_plan_test>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-scalar-storage"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_scalar_storage_test.cmake")
wio_test_labels(wio_wir_cpp_scalar_storage native codegen optimization storage ownership)
set_tests_properties(wio_wir_cpp_scalar_storage PROPERTIES TIMEOUT 600)

add_test(NAME wio_std_builders_consistency
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/std-builders-consistency"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/std_builders_consistency_test.cmake")
wio_test_labels(wio_std_builders_consistency std native vm builders result optimization)
set_tests_properties(wio_std_builders_consistency PROPERTIES TIMEOUT 900)

add_test(NAME wio_std_builders_style COMMAND ${CMAKE_COMMAND} "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/cmake/std_builders_style_test.cmake")
wio_test_labels(wio_std_builders_style std builders style result)

add_test(NAME wio_wir_cpp_managed_storage
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-managed-storage"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_managed_storage_test.cmake")
wio_test_labels(wio_wir_cpp_managed_storage native vm codegen optimization storage ownership)
set_tests_properties(wio_wir_cpp_managed_storage PROPERTIES TIMEOUT 900)

add_test(NAME wio_wir_integer_interpolation
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-integer-interpolation"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_integer_interpolation_test.cmake")
wio_test_labels(wio_wir_integer_interpolation native vm formatting optimization unicode)
set_tests_properties(wio_wir_integer_interpolation PROPERTIES TIMEOUT 900)

add_test(NAME wio_wir_cpp_string_fast_paths
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_SHARED_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-string-fast-paths"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_string_fast_paths_test.cmake")
wio_test_labels(wio_wir_cpp_string_fast_paths native vm string optimization ownership)
set_tests_properties(wio_wir_cpp_string_fast_paths PROPERTIES TIMEOUT 900)

add_test(NAME wio_native_cost_contract
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/native-cost-contract"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/native_cost_contract_test.cmake")
wio_test_labels(wio_native_cost_contract native performance diagnostic allocation)
set_tests_properties(wio_native_cost_contract PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)

add_test(NAME wio_native_cost_extended_contract
    COMMAND ${CMAKE_COMMAND} "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_BACKEND_CXX=${WIO_BACKEND_CXX_COMPILER}" "-DWIO_RUNTIME=${WIO_BACKEND_RUNTIME_LIBRARY}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}" "-DWIO_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/native-cost-extended-contract" -DWIO_SUITE=extended
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/native_cost_contract_test.cmake")
wio_test_labels(wio_native_cost_extended_contract native performance diagnostic allocation async unicode ownership)
set_tests_properties(wio_native_cost_extended_contract PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)

# PowerShell is optional for compiler users; this is a diagnostic harness test,
# not a platform runtime dependency or a performance-threshold CI gate.
find_program(WIO_MEASUREMENT_PWSH NAMES pwsh)
if(WIO_MEASUREMENT_PWSH)
    add_test(NAME wio_measurement_contract
        COMMAND "${WIO_MEASUREMENT_PWSH}" -NoProfile -File
            "${CMAKE_SOURCE_DIR}/tests/performance/measurement_test.ps1")
    wio_test_labels(wio_measurement_contract performance diagnostic measurement)
    set_tests_properties(wio_measurement_contract PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(WIO_BENCHMARK_TEST_RUNNER "" CACHE FILEPATH "Explicit prebuilt wio-bench for its contract test (empty bootstraps from Wio)")
    set(WIO_BENCHMARK_TEST_RUNNER_ARGS)
    if(WIO_BENCHMARK_TEST_RUNNER)
        list(APPEND WIO_BENCHMARK_TEST_RUNNER_ARGS --runner "${WIO_BENCHMARK_TEST_RUNNER}")
    endif()
    add_test(NAME wio_benchmark_contract
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/benchmark/verify.py"
            --wio "$<TARGET_FILE:wio_app>" --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}"
            --cxx "${WIO_BACKEND_CXX_COMPILER}" --vm-host "$<TARGET_FILE:wio_vm_runner>"
            --vm-config "$<CONFIG>" --output "${CMAKE_BINARY_DIR}/wio-benchmark-contract"
            ${WIO_BENCHMARK_TEST_RUNNER_ARGS})
    wio_test_labels(wio_benchmark_contract native vm benchmark performance diagnostic memory wio)
    set_tests_properties(wio_benchmark_contract PROPERTIES TIMEOUT 1800 RUN_SERIAL TRUE)
    add_test(NAME wio_n02_concat_semantics
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/performance/concat_reuse/verify.py"
            --wio "$<TARGET_FILE:wio_app>" --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}"
            --cxx "${WIO_BACKEND_CXX_COMPILER}" --output "${CMAKE_BINARY_DIR}/n02-concat-semantics")
    wio_test_labels(wio_n02_concat_semantics native vm string text optimization ownership)
    set_tests_properties(wio_n02_concat_semantics PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
    add_test(NAME wio_n02_append_reuse
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/performance/append_reuse/verify.py"
            --wio "$<TARGET_FILE:wio_app>" --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}"
            --cxx "${WIO_BACKEND_CXX_COMPILER}" --output "${CMAKE_BINARY_DIR}/n02-append-semantics")
    wio_test_labels(wio_n02_append_reuse native vm string optimization ownership)
    set_tests_properties(wio_n02_append_reuse PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
    add_test(NAME wio_native_cost_python_contract
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/performance/native_costs/test_measurement.py"
            --cxx "${WIO_BACKEND_CXX_COMPILER}" --wio "$<TARGET_FILE:wio_app>"
            --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}")
    wio_test_labels(wio_native_cost_python_contract performance diagnostic measurement language wio)
    set_tests_properties(wio_native_cost_python_contract PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)

    add_test(NAME wio_native_memory_attribution
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/performance/native_costs/memory/test_memory.py"
            --output "${CMAKE_BINARY_DIR}/native-memory-attribution"
            --cxx "${WIO_BACKEND_CXX_COMPILER}" --wio "$<TARGET_FILE:wio_app>"
            --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}")
    wio_test_labels(wio_native_memory_attribution native performance diagnostic allocation ownership memory wio)
    set_tests_properties(wio_native_memory_attribution PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)

    # This attribution profiler needs GCC/gcov, not Clang's different coverage
    # format. Keep it separate from portable timing/allocation controllers.
    execute_process(COMMAND "${WIO_BACKEND_CXX_COMPILER}" --version
        OUTPUT_VARIABLE WIO_COST_CXX_VERSION ERROR_QUIET)
    if(WIO_COST_CXX_VERSION MATCHES "(g\\+\\+|GCC|Free Software Foundation)")
        add_test(NAME wio_native_copy_attribution
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/performance/native_costs/copies/test_copies.py"
                --output "${CMAKE_BINARY_DIR}/native-copy-attribution"
                --cxx "${WIO_BACKEND_CXX_COMPILER}" --wio "$<TARGET_FILE:wio_app>"
                --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}")
        wio_test_labels(wio_native_copy_attribution native performance diagnostic copies wio)
        set_tests_properties(wio_native_copy_attribution PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    endif()
    find_program(WIO_COST_GCOV NAMES gcov)
    if(WIO_COST_GCOV AND WIO_COST_CXX_VERSION MATCHES "(g\\+\\+|GCC|Free Software Foundation)")
        add_test(NAME wio_native_ownership_attribution
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/performance/native_costs/ownership/test_ownership.py"
                --output "${CMAKE_BINARY_DIR}/native-ownership-attribution"
                --cxx "${WIO_BACKEND_CXX_COMPILER}" --gcov "${WIO_COST_GCOV}"
                --wio "$<TARGET_FILE:wio_app>" --runtime "${WIO_BACKEND_RUNTIME_LIBRARY}")
        wio_test_labels(wio_native_ownership_attribution native performance diagnostic ownership wio)
        set_tests_properties(wio_native_ownership_attribution PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
    endif()
endif()
