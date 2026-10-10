file(GLOB_RECURSE WIO_VM_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

file(GLOB_RECURSE WIO_VM_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "src/*.h"
)

add_library(wio_vm STATIC
    ${WIO_VM_SOURCES}
    ${WIO_VM_HEADERS}
)

add_library(wio-vm ALIAS wio_vm)

target_include_directories(wio_vm PUBLIC include)
target_link_libraries(wio_vm PUBLIC wio_compiler wio_sdk PRIVATE ${CMAKE_DL_LIBS})

target_compile_options(wio_vm PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(BUILD_TESTING)
    add_executable(wio_vm_native_projected_object_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_projected_object_borrow_test.cpp")
    target_link_libraries(wio_vm_native_projected_object_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_projected_any_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_projected_any_borrow_test.cpp")
    target_link_libraries(wio_vm_native_projected_any_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_dictionary_intrinsic_test "${CMAKE_SOURCE_DIR}/tests/vm_native_dictionary_intrinsic_test.cpp")
    target_include_directories(wio_vm_native_dictionary_intrinsic_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_dictionary_intrinsic_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_projected_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_projected_borrow_test.cpp")
    target_link_libraries(wio_vm_native_projected_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_dictionary_projection_test "${CMAKE_SOURCE_DIR}/tests/vm_native_dictionary_projection_test.cpp")
    target_link_libraries(wio_vm_native_dictionary_projection_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_sequence_pod_projection_test "${CMAKE_SOURCE_DIR}/tests/vm_native_sequence_pod_projection_test.cpp")
    target_link_libraries(wio_vm_native_sequence_pod_projection_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_sequence_projection_test "${CMAKE_SOURCE_DIR}/tests/vm_native_sequence_projection_test.cpp")
    target_include_directories(wio_vm_native_sequence_projection_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    target_link_libraries(wio_vm_native_sequence_projection_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_container_shape_test "${CMAKE_SOURCE_DIR}/tests/vm_native_container_shape_test.cpp")
    target_include_directories(wio_vm_native_container_shape_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_container_shape_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_nested_any_test "${CMAKE_SOURCE_DIR}/tests/vm_native_nested_any_test.cpp")
    target_include_directories(wio_vm_native_nested_any_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include")
    target_link_libraries(wio_vm_native_nested_any_test PRIVATE wio_vm)

    add_executable(wio_vm_native_free_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_free_borrow_test.cpp")
    target_link_libraries(wio_vm_native_free_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_handle_containers_test "${CMAKE_SOURCE_DIR}/tests/vm_native_handle_containers_test.cpp")
    target_include_directories(wio_vm_native_handle_containers_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_handle_containers_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_any_generic_test "${CMAKE_SOURCE_DIR}/tests/vm_native_any_generic_test.cpp")
    target_include_directories(wio_vm_native_any_generic_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_any_generic_test PRIVATE wio_vm)

    add_executable(wio_vm_native_object_proxy_test "${CMAKE_SOURCE_DIR}/tests/vm_native_object_proxy_test.cpp")
    target_include_directories(wio_vm_native_object_proxy_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include")
    target_link_libraries(wio_vm_native_object_proxy_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_object_host_test "${CMAKE_SOURCE_DIR}/tests/vm_native_object_host_test.cpp")
    target_include_directories(wio_vm_native_object_host_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include")
    target_link_libraries(wio_vm_native_object_host_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_object_host COMMAND wio_vm_native_object_host_test)
    set_tests_properties(wio_vm_native_object_host PROPERTIES TIMEOUT 45)
    set_property(TEST wio_vm_native_object_host APPEND PROPERTY LABELS "vm;native;native-object;release-gate")

    add_executable(wio_vm_native_any_test "${CMAKE_SOURCE_DIR}/tests/vm_native_any_test.cpp")
    target_include_directories(wio_vm_native_any_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include")
    target_link_libraries(wio_vm_native_any_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_any COMMAND wio_vm_native_any_test)
    set_property(TEST wio_vm_native_any APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_dictionary_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_dictionary_test.cpp"
    )
    target_include_directories(wio_vm_native_dictionary_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include"
    )
    target_link_libraries(wio_vm_native_dictionary_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_dictionary COMMAND wio_vm_native_dictionary_test)
    set_property(TEST wio_vm_native_dictionary APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_sequence_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_sequence_test.cpp"
    )
    target_include_directories(wio_vm_native_sequence_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include"
    )
    target_link_libraries(wio_vm_native_sequence_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_sequence COMMAND wio_vm_native_sequence_test)
    set_property(TEST wio_vm_native_sequence APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_pod_alias_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_alias_test.cpp"
    )
    target_include_directories(wio_vm_native_pod_alias_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include"
    )
    target_link_libraries(wio_vm_native_pod_alias_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_pod_alias COMMAND wio_vm_native_pod_alias_test)
    set_property(TEST wio_vm_native_pod_alias APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_library(wio_vm_native_pod_default_module SHARED
        "${CMAKE_SOURCE_DIR}/tests/native/vm_pod_default_module.cpp"
    )
    target_include_directories(wio_vm_native_pod_default_module PRIVATE "${CMAKE_SOURCE_DIR}/sdk/include")
    add_executable(wio_vm_native_pod_default_lifetime_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_default_lifetime_test.cpp"
    )
    target_link_libraries(wio_vm_native_pod_default_lifetime_test PRIVATE wio_vm)
    target_compile_definitions(wio_vm_native_pod_default_lifetime_test PRIVATE
        WIO_TEST_POD_DEFAULT_LIBRARY="$<TARGET_FILE:wio_vm_native_pod_default_module>"
    )
    add_dependencies(wio_vm_native_pod_default_lifetime_test wio_vm_native_pod_default_module)
    add_test(NAME wio_vm_native_pod_default_lifetime COMMAND wio_vm_native_pod_default_lifetime_test)
    set_property(TEST wio_vm_native_pod_default_lifetime APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_pod_default_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_default_test.cpp"
    )
    target_include_directories(wio_vm_native_pod_default_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include"
    )
    target_link_libraries(wio_vm_native_pod_default_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_pod_default COMMAND wio_vm_native_pod_default_test)
    set_property(TEST wio_vm_native_pod_default APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_pod_template_contract_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_template_contract_test.cpp"
    )
    target_include_directories(wio_vm_native_pod_template_contract_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_pod_template_contract_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_pod_template_contract COMMAND wio_vm_native_pod_template_contract_test)
    set_property(TEST wio_vm_native_pod_template_contract APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_library(wio_vm_native_pod_pointer_module SHARED
        "${CMAKE_SOURCE_DIR}/tests/native/vm_pod_pointer_lifetime_module.cpp"
    )
    target_include_directories(wio_vm_native_pod_pointer_module PRIVATE "${CMAKE_SOURCE_DIR}/sdk/include")
    add_executable(wio_vm_native_pod_pointer_lifetime_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_pointer_lifetime_test.cpp"
    )
    target_link_libraries(wio_vm_native_pod_pointer_lifetime_test PRIVATE wio_vm)
    target_compile_definitions(wio_vm_native_pod_pointer_lifetime_test PRIVATE
        WIO_TEST_POD_POINTER_LIBRARY="$<TARGET_FILE:wio_vm_native_pod_pointer_module>"
    )
    add_dependencies(wio_vm_native_pod_pointer_lifetime_test wio_vm_native_pod_pointer_module)
    add_test(NAME wio_vm_native_pod_pointer_lifetime COMMAND wio_vm_native_pod_pointer_lifetime_test)
    set_property(TEST wio_vm_native_pod_pointer_lifetime APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_pod_pointer_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_pointer_test.cpp"
    )
    target_include_directories(wio_vm_native_pod_pointer_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_pod_pointer_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_pod_pointer COMMAND wio_vm_native_pod_pointer_test)
    set_property(TEST wio_vm_native_pod_pointer APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_pod_array_codec_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_array_codec_test.cpp"
    )
    target_include_directories(wio_vm_native_pod_array_codec_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_pod_array_codec_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_pod_array_codec COMMAND wio_vm_native_pod_array_codec_test)
    set_property(TEST wio_vm_native_pod_array_codec APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_pod_layout_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_pod_layout_test.cpp"
    )
    target_include_directories(wio_vm_native_pod_layout_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_pod_layout_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_pod_layout COMMAND wio_vm_native_pod_layout_test)
    set_property(TEST wio_vm_native_pod_layout APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_native_value_contract_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_value_contract_test.cpp"
    )
    target_include_directories(wio_vm_native_value_contract_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(wio_vm_native_value_contract_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_value_contract COMMAND wio_vm_native_value_contract_test)
    set_property(TEST wio_vm_native_value_contract APPEND PROPERTY LABELS "vm;native;native-differential;release-gate")

    add_executable(wio_vm_scalar_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_scalar_execution_test.cpp"
    )
    target_link_libraries(wio_vm_scalar_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_scalar_execution COMMAND wio_vm_scalar_execution_test)

    add_executable(wio_vm_unicode_container_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_unicode_container_execution_test.cpp"
    )
    target_link_libraries(wio_vm_unicode_container_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_unicode_container_execution COMMAND wio_vm_unicode_container_execution_test)

    add_executable(wio_vm_object_lifecycle_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_object_lifecycle_execution_test.cpp"
    )
    target_link_libraries(wio_vm_object_lifecycle_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_object_lifecycle_execution COMMAND wio_vm_object_lifecycle_execution_test)

    add_executable(wio_vm_dictionary_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_dictionary_execution_test.cpp"
    )
    target_link_libraries(wio_vm_dictionary_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_dictionary_execution COMMAND wio_vm_dictionary_execution_test)

    add_executable(wio_vm_array_intrinsic_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_array_intrinsic_execution_test.cpp"
    )
    target_link_libraries(wio_vm_array_intrinsic_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_array_intrinsic_execution COMMAND wio_vm_array_intrinsic_execution_test)

    add_executable(wio_vm_string_intrinsic_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_string_intrinsic_execution_test.cpp"
    )
    target_link_libraries(wio_vm_string_intrinsic_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_string_intrinsic_execution COMMAND wio_vm_string_intrinsic_execution_test)

    add_executable(wio_vm_text_intrinsic_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_text_intrinsic_execution_test.cpp"
    )
    target_link_libraries(wio_vm_text_intrinsic_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_text_intrinsic_execution COMMAND wio_vm_text_intrinsic_execution_test)

    add_executable(wio_vm_callable_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_callable_execution_test.cpp"
    )
    target_link_libraries(wio_vm_callable_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_callable_execution COMMAND wio_vm_callable_execution_test)

    add_executable(wio_vm_dynamic_dispatch_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_dynamic_dispatch_execution_test.cpp"
    )
    target_link_libraries(wio_vm_dynamic_dispatch_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_dynamic_dispatch_execution COMMAND wio_vm_dynamic_dispatch_execution_test)

    add_executable(wio_vm_coroutine_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_coroutine_execution_test.cpp"
    )
    target_link_libraries(wio_vm_coroutine_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_coroutine_execution COMMAND wio_vm_coroutine_execution_test)

    add_executable(wio_vm_value_model_execution_test
        "${CMAKE_SOURCE_DIR}/tests/vm_value_model_execution_test.cpp"
    )
    target_link_libraries(wio_vm_value_model_execution_test PRIVATE wio_vm)
    add_test(NAME wio_vm_value_model_execution COMMAND wio_vm_value_model_execution_test)

    add_executable(wio_vm_async_services_test
        "${CMAKE_SOURCE_DIR}/tests/vm_async_services_test.cpp"
    )
    target_link_libraries(wio_vm_async_services_test PRIVATE wio_vm)
    add_test(NAME wio_vm_async_services COMMAND wio_vm_async_services_test)
    set_property(TEST wio_vm_async_services APPEND PROPERTY LABELS freeze)

    add_executable(wio_vm_debug_diagnostics_test
        "${CMAKE_SOURCE_DIR}/tests/vm_debug_diagnostics_test.cpp"
    )
    target_link_libraries(wio_vm_debug_diagnostics_test PRIVATE wio_vm)
    add_test(NAME wio_vm_debug_diagnostics COMMAND wio_vm_debug_diagnostics_test)

    add_executable(wio_vm_exceptional_cleanup_test
        "${CMAKE_SOURCE_DIR}/tests/vm_exceptional_cleanup_test.cpp"
    )
    target_link_libraries(wio_vm_exceptional_cleanup_test PRIVATE wio_vm)
    add_test(NAME wio_vm_exceptional_cleanup COMMAND wio_vm_exceptional_cleanup_test)

    add_executable(wio_vm_native_bridge_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_bridge_test.cpp"
    )
    target_link_libraries(wio_vm_native_bridge_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_bridge COMMAND wio_vm_native_bridge_test)

    add_executable(wio_vm_native_async_bridge_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_async_bridge_test.cpp"
    )
    target_link_libraries(wio_vm_native_async_bridge_test PRIVATE wio_vm)
    target_compile_definitions(wio_vm_native_async_bridge_test PRIVATE
        WIO_TEST_CXX_COMPILER="${WIO_BACKEND_CXX_COMPILER}"
        WIO_TEST_RUNTIME_INCLUDE="${CMAKE_SOURCE_DIR}/runtime/include"
        WIO_TEST_SDK_INCLUDE="${CMAKE_SOURCE_DIR}/sdk/include"
    )
    add_test(NAME wio_vm_native_async_bridge COMMAND wio_vm_native_async_bridge_test)
    set_property(TEST wio_vm_native_async_bridge APPEND PROPERTY LABELS freeze)

    add_library(wio_vm_native_discovery_module SHARED
        "${CMAKE_SOURCE_DIR}/tests/native/vm_discovery_module.cpp"
    )
    target_include_directories(wio_vm_native_discovery_module PRIVATE "${CMAKE_SOURCE_DIR}/sdk/include")

    foreach(mode IN ITEMS 1 2 3)
        add_library(wio_vm_native_invalid_module_${mode} SHARED
            "${CMAKE_SOURCE_DIR}/tests/native/vm_discovery_invalid_module.cpp"
        )
        target_include_directories(wio_vm_native_invalid_module_${mode} PRIVATE "${CMAKE_SOURCE_DIR}/sdk/include")
        target_compile_definitions(wio_vm_native_invalid_module_${mode} PRIVATE WIO_TEST_MODULE_MODE=${mode})
    endforeach()

    add_executable(wio_vm_native_library_discovery_test
        "${CMAKE_SOURCE_DIR}/tests/vm_native_library_discovery_test.cpp"
    )
    target_link_libraries(wio_vm_native_library_discovery_test PRIVATE wio_vm)
    target_compile_definitions(wio_vm_native_library_discovery_test PRIVATE
        WIO_TEST_NATIVE_LIBRARY="$<TARGET_FILE:wio_vm_native_discovery_module>"
        WIO_TEST_MISSING_REGISTRY_LIBRARY="$<TARGET_FILE:wio_vm_native_invalid_module_1>"
        WIO_TEST_INVALID_ABI_LIBRARY="$<TARGET_FILE:wio_vm_native_invalid_module_2>"
        WIO_TEST_INVALID_SIDECAR_LIBRARY="$<TARGET_FILE:wio_vm_native_invalid_module_3>"
    )
    add_dependencies(wio_vm_native_library_discovery_test wio_vm_native_discovery_module
        wio_vm_native_invalid_module_1 wio_vm_native_invalid_module_2 wio_vm_native_invalid_module_3)
    add_test(NAME wio_vm_native_library_discovery COMMAND wio_vm_native_library_discovery_test)
endif()
