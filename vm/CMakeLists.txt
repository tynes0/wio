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
target_compile_features(wio_vm PUBLIC cxx_std_20)

target_include_directories(wio_vm PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
find_package(Threads REQUIRED)
target_link_libraries(wio_vm PUBLIC wio_bytecode_core wio_sdk PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
set_target_properties(wio_vm PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS wio_vm EXPORT WioVmRuntimeTargets ARCHIVE DESTINATION lib COMPONENT WioVmRuntime)
install(DIRECTORY include/wio/vm DESTINATION include/wio COMPONENT WioVmRuntime FILES_MATCHING PATTERN "*.h")

include(CMakePackageConfigHelpers)
configure_package_config_file("${CMAKE_SOURCE_DIR}/cmake/WioVmRuntimeConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/WioVmRuntimeConfig.cmake"
    INSTALL_DESTINATION lib/cmake/WioVmRuntime)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/WioVmRuntimeConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion)
install(EXPORT WioVmRuntimeTargets NAMESPACE Wio:: DESTINATION lib/cmake/WioVmRuntime COMPONENT WioVmRuntime)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/WioVmRuntimeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/WioVmRuntimeConfigVersion.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/WioVmModule.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/EmbedWioVmModule.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/StageWioVmNative.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/DeployWioVmNative.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/InstallWioVmNative.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/WioVmNativePaths.cmake"
    DESTINATION lib/cmake/WioVmRuntime COMPONENT WioVmRuntime)

target_compile_options(wio_vm PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(BUILD_TESTING)
    add_executable(wio_vm_native_task_inputs_host_test "${CMAKE_SOURCE_DIR}/tests/vm_native_task_inputs_host_test.cpp")
    target_link_libraries(wio_vm_native_task_inputs_host_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_native_task_source_codec_test "${CMAKE_SOURCE_DIR}/tests/native_task_source_codec_test.cpp")
    target_include_directories(wio_native_task_source_codec_test PRIVATE
        "${CMAKE_SOURCE_DIR}/runtime/include" "${CMAKE_SOURCE_DIR}/sdk/include")
    target_link_libraries(wio_native_task_source_codec_test PRIVATE wio_vm)
    add_test(NAME wio_native_task_source_codec COMMAND wio_native_task_source_codec_test)
    set_tests_properties(wio_native_task_source_codec PROPERTIES
        TIMEOUT 30 LABELS "vm;native;async;task;ownership;release-gate")
    add_executable(wio_vm_native_ownership_contract_test "${CMAKE_SOURCE_DIR}/tests/vm_native_ownership_contract_test.cpp")
    target_include_directories(wio_vm_native_ownership_contract_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/runtime/include")
    target_link_libraries(wio_vm_native_ownership_contract_test PRIVATE wio_vm)
    add_test(NAME wio_vm_native_ownership_contract COMMAND wio_vm_native_ownership_contract_test)
    set_tests_properties(wio_vm_native_ownership_contract PROPERTIES LABELS "vm;native;ownership;alias;release-gate")
    add_executable(wio_vm_native_ownership_alias_test "${CMAKE_SOURCE_DIR}/tests/vm_native_ownership_alias_test.cpp")
    target_link_libraries(wio_vm_native_ownership_alias_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_vm_native_callback_callable_results_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_callable_results_test.cpp")
    target_link_libraries(wio_vm_native_callback_callable_results_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_vm_native_callback_fixed_array_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_fixed_array_test.cpp")
    target_link_libraries(wio_vm_native_callback_fixed_array_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_vm_native_callback_value_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_value_borrow_test.cpp")
    target_link_libraries(wio_vm_native_callback_value_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_vm_native_callback_string_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_string_borrow_test.cpp")
    target_link_libraries(wio_vm_native_callback_string_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_vm_native_callback_borrow_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_borrow_test.cpp")
    target_link_libraries(wio_vm_native_callback_borrow_test PRIVATE wio_vm ${CMAKE_DL_LIBS})
    add_executable(wio_vm_native_hosted_callback_test "${CMAKE_SOURCE_DIR}/tests/vm_native_hosted_callback_test.cpp")
    target_link_libraries(wio_vm_native_hosted_callback_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_callback_container_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_container_test.cpp")
    target_link_libraries(wio_vm_native_callback_container_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_callback_object_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_object_test.cpp")
    target_link_libraries(wio_vm_native_callback_object_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

    add_executable(wio_vm_native_callback_pod_test "${CMAKE_SOURCE_DIR}/tests/vm_native_callback_pod_test.cpp")
    target_link_libraries(wio_vm_native_callback_pod_test PRIVATE wio_vm ${CMAKE_DL_LIBS})

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

# Compiler-pipeline tests need the frontend explicitly; the published runtime
# and standalone runner must not regain that dependency transitively.
get_property(wio_vm_local_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(wio_vm_local_target IN LISTS wio_vm_local_targets)
    if(wio_vm_local_target MATCHES "_test$")
        target_link_libraries(${wio_vm_local_target} PRIVATE wio_compiler)
    endif()
endforeach()

add_executable(wio_vm_runner
    runner/main.cpp
    runner/artifact.cpp
    "${CMAKE_SOURCE_DIR}/app/vm_runner.cpp")
target_include_directories(wio_vm_runner PRIVATE "${CMAKE_SOURCE_DIR}/app" "${CMAKE_SOURCE_DIR}/runtime/include")
target_link_libraries(wio_vm_runner PRIVATE wio_vm wio_runtime_host)
set_target_properties(wio_vm_runner PROPERTIES
    OUTPUT_NAME "wio-vm" FOLDER "runtime"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app/$<CONFIG>")
install(TARGETS wio_vm_runner RUNTIME DESTINATION bin COMPONENT WioVmRuntime)

if(BUILD_TESTING)
    add_library(wio_vm_artifact_extra_registry SHARED "${CMAKE_SOURCE_DIR}/tests/native/vm_artifact_extra_registry.cpp")
    target_link_libraries(wio_vm_artifact_extra_registry PRIVATE wio_sdk)
    add_executable(wio_vm_artifact_generation_test
        "${CMAKE_SOURCE_DIR}/tests/vm_artifact_generation_test.cpp"
        runner/artifact.cpp)
    target_include_directories(wio_vm_artifact_generation_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runner")
    target_link_libraries(wio_vm_artifact_generation_test PRIVATE wio_bytecode_core)
    add_test(NAME wio_vm_artifact_generation COMMAND wio_vm_artifact_generation_test "${CMAKE_BINARY_DIR}/vm-artifact-generation")
    set_tests_properties(wio_vm_artifact_generation PROPERTIES LABELS "vm;artifact;deployment;ownership;release-gate")
    add_executable(wio_vm_artifact_codec_test
        "${CMAKE_SOURCE_DIR}/tests/vm_artifact_codec_test.cpp"
        runner/artifact.cpp)
    target_include_directories(wio_vm_artifact_codec_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runner")
    target_link_libraries(wio_vm_artifact_codec_test PRIVATE wio_bytecode_core)
    add_test(NAME wio_vm_artifact_codec COMMAND wio_vm_artifact_codec_test "${CMAKE_BINARY_DIR}/vm-artifact-codec")
    set_tests_properties(wio_vm_artifact_codec PROPERTIES LABELS "vm;artifact;codec;deployment;release-gate")
    add_executable(wio_vm_sdk_module_adapter_test "${CMAKE_SOURCE_DIR}/tests/vm_sdk_module_adapter_test.cpp")
    target_link_libraries(wio_vm_sdk_module_adapter_test PRIVATE wio_vm)
    add_test(NAME wio_vm_sdk_module_adapter COMMAND wio_vm_sdk_module_adapter_test)
    set_tests_properties(wio_vm_sdk_module_adapter PROPERTIES
        TIMEOUT 30 LABELS "vm;sdk;module;ownership;thread;release-gate")
    add_executable(wio_vm_sdk_session_lifecycle_test "${CMAKE_SOURCE_DIR}/tests/vm_sdk_session_lifecycle_test.cpp")
    target_link_libraries(wio_vm_sdk_session_lifecycle_test PRIVATE wio_vm)
    add_test(NAME wio_vm_sdk_session_lifecycle COMMAND wio_vm_sdk_session_lifecycle_test)
    set_tests_properties(wio_vm_sdk_session_lifecycle PROPERTIES
        TIMEOUT 30 LABELS "vm;sdk;session;lifecycle;ownership;thread;release-gate")
    add_executable(wio_vm_task_source_shutdown_test "${CMAKE_SOURCE_DIR}/tests/vm_task_source_shutdown_test.cpp")
    target_link_libraries(wio_vm_task_source_shutdown_test PRIVATE wio_vm)
    add_test(NAME wio_vm_task_source_shutdown COMMAND wio_vm_task_source_shutdown_test)
    set_tests_properties(wio_vm_task_source_shutdown PROPERTIES
        TIMEOUT 90 LABELS "vm;sdk;async;task;shutdown;ownership;thread;release-gate")
    add_executable(wio_vm_task_source_import_test "${CMAKE_SOURCE_DIR}/tests/vm_task_source_import_test.cpp")
    target_link_libraries(wio_vm_task_source_import_test PRIVATE wio_vm)
    add_test(NAME wio_vm_task_source_import COMMAND wio_vm_task_source_import_test)
    set_tests_properties(wio_vm_task_source_import PROPERTIES
        TIMEOUT 30 LABELS "vm;sdk;async;task;task-input;ownership;thread;release-gate")
    add_executable(wio_vm_module_native_paths_test "${CMAKE_SOURCE_DIR}/tests/vm_module_native_paths_test.cpp")
    target_link_libraries(wio_vm_module_native_paths_test PRIVATE wio_vm)
    add_test(NAME wio_vm_module_native_paths COMMAND wio_vm_module_native_paths_test)
    set_tests_properties(wio_vm_module_native_paths PROPERTIES
        TIMEOUT 30 LABELS "vm;sdk;native;deployment;ownership;release-gate")
endif()

# Fail configuration if an accidental transitive dependency pulls the compiler
# back into the published runtime. Inspect targets, not generated project names.
function(wio_assert_runtime_only target)
    get_target_property(aliased "${target}" ALIASED_TARGET)
    if(aliased)
        set(target "${aliased}")
    endif()
    get_property(visited GLOBAL PROPERTY WIO_RUNTIME_DEPENDENCY_VISITED)
    if(target IN_LIST visited)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY WIO_RUNTIME_DEPENDENCY_VISITED "${target}")
    if(target STREQUAL "wio_compiler")
        message(FATAL_ERROR "Standalone VM runtime must not link wio_compiler")
    endif()
    foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(dependencies "${target}" "${property}")
        foreach(dependency IN LISTS dependencies)
            # Static libraries expose private dependencies using LINK_ONLY.
            if(dependency MATCHES "^\\$<LINK_ONLY:(.+)>$")
                set(dependency "${CMAKE_MATCH_1}")
            endif()
            if(TARGET "${dependency}")
                wio_assert_runtime_only("${dependency}")
            endif()
        endforeach()
    endforeach()
endfunction()
wio_assert_runtime_only(wio_vm_runner)
if(TARGET wio_vm_sdk_module_adapter_test)
    wio_assert_runtime_only(wio_vm_sdk_module_adapter_test)
endif()
if(TARGET wio_vm_sdk_session_lifecycle_test)
    wio_assert_runtime_only(wio_vm_sdk_session_lifecycle_test)
endif()
if(TARGET wio_vm_task_source_shutdown_test)
    wio_assert_runtime_only(wio_vm_task_source_shutdown_test)
endif()
if(TARGET wio_vm_task_source_import_test)
    wio_assert_runtime_only(wio_vm_task_source_import_test)
endif()
if(TARGET wio_vm_module_native_paths_test)
    wio_assert_runtime_only(wio_vm_module_native_paths_test)
endif()
