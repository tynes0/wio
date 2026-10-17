file(GLOB_RECURSE WIO_SOURCES CONFIGURE_DEPENDS
    "src/*.h"
    "src/*.inl"
    "src/*.cpp"
)

file(GLOB_RECURSE WIO_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.inl"
)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include" WIO_RUNTIME_INCLUDE_DIR)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include" WIO_SDK_INCLUDE_DIR)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../std" WIO_STD_SOURCE_DIR)
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" WIO_DEFAULT_ROOT_DIR)
file(TO_CMAKE_PATH "${WIO_BACKEND_CXX_COMPILER}" WIO_BACKEND_CXX_COMPILER_CPP)
file(TO_CMAKE_PATH "${WIO_BACKEND_AR}" WIO_BACKEND_AR_CPP)
file(TO_CMAKE_PATH "${WIO_BACKEND_RUNTIME_LIBRARY}" WIO_BACKEND_RUNTIME_LIBRARY_CPP)

list(REMOVE_ITEM WIO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wio_compiler/bytecode/codec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wio_compiler/bytecode/module.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wio_compiler/bytecode/format.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wio_compiler/bytecode/verifier.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wio_compiler/bytecode/disassembler.cpp")

add_library(wio_compiler STATIC 
    ${WIO_SOURCES}
    ${WIO_HEADERS}
)

add_library(wio-compiler ALIAS wio_compiler)

if(BUILD_TESTING)
    add_executable(wio_string_init_plan_test "${CMAKE_SOURCE_DIR}/tests/string_init_plan_test.cpp")
    target_link_libraries(wio_string_init_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_string_init_plan COMMAND wio_string_init_plan_test)
    set_tests_properties(wio_n03_string_init_plan PROPERTIES LABELS "native;formatting;ownership;memory" TIMEOUT 60)

    add_executable(wio_inline_string_init_test "${CMAKE_SOURCE_DIR}/tests/inline_string_init_test.cpp")
    target_include_directories(wio_inline_string_init_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_n03_inline_string_init COMMAND wio_inline_string_init_test)
    set_tests_properties(wio_n03_inline_string_init PROPERTIES LABELS "native;formatting;ownership;memory" TIMEOUT 60)

    add_executable(wio_string_transfer_plan_test "${CMAKE_SOURCE_DIR}/tests/string_transfer_plan_test.cpp")
    target_link_libraries(wio_string_transfer_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_string_transfer_plan COMMAND wio_string_transfer_plan_test)
    set_tests_properties(wio_n03_string_transfer_plan PROPERTIES LABELS "native;formatting;ownership;memory" TIMEOUT 60)

    add_executable(wio_integer_hot_path_test "${CMAKE_SOURCE_DIR}/tests/integer_hot_path_test.cpp")
    target_include_directories(wio_integer_hot_path_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_n03_integer_hot_path COMMAND wio_integer_hot_path_test)
    set_tests_properties(wio_n03_integer_hot_path PROPERTIES LABELS "native;formatting;locale;memory" TIMEOUT 60)

    add_executable(wio_receiver_plan_test "${CMAKE_SOURCE_DIR}/tests/receiver_plan_test.cpp")
    target_link_libraries(wio_receiver_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_receiver_plan COMMAND wio_receiver_plan_test)
    set_tests_properties(wio_n03_receiver_plan PROPERTIES LABELS "native;formatting;ownership;memory" TIMEOUT 60)

    add_executable(wio_formatted_size_test "${CMAKE_SOURCE_DIR}/tests/formatted_size_test.cpp")
    target_include_directories(wio_formatted_size_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_n03_formatted_size COMMAND wio_formatted_size_test)
    set_tests_properties(wio_n03_formatted_size PROPERTIES LABELS "native;formatting;memory" TIMEOUT 60)
    add_executable(wio_scalar_field_plan_test "${CMAKE_SOURCE_DIR}/tests/scalar_field_plan_test.cpp")
    target_link_libraries(wio_scalar_field_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_scalar_field_plan COMMAND wio_scalar_field_plan_test)
    set_tests_properties(wio_n03_scalar_field_plan PROPERTIES LABELS "native;formatting;ownership;memory" TIMEOUT 60)
    add_executable(wio_native_string_borrow_plan_test "${CMAKE_SOURCE_DIR}/tests/native_string_borrow_plan_test.cpp")
    target_link_libraries(wio_native_string_borrow_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_native_string_borrow_plan COMMAND wio_native_string_borrow_plan_test)
    set_tests_properties(wio_n03_native_string_borrow_plan PROPERTIES LABELS "native;formatting;memory;ownership;interop" TIMEOUT 60)
    add_executable(wio_formatting_call_lowerer_test "${CMAKE_SOURCE_DIR}/tests/formatting_call_lowerer_test.cpp")
    target_link_libraries(wio_formatting_call_lowerer_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_formatting_call_plan COMMAND wio_formatting_call_lowerer_test)
    set_tests_properties(wio_n03_formatting_call_plan PROPERTIES LABELS "wir;native;vm;formatting;ownership" TIMEOUT 60)

    add_executable(wio_numeric_append_optimizer_test "${CMAKE_SOURCE_DIR}/tests/numeric_append_optimizer_test.cpp")
    target_link_libraries(wio_numeric_append_optimizer_test PRIVATE wio_compiler)
    add_test(NAME wio_n03_numeric_builder_plan COMMAND wio_numeric_append_optimizer_test)
    set_tests_properties(wio_n03_numeric_builder_plan PROPERTIES LABELS "wir;native;vm;formatting;optimization;ownership" TIMEOUT 60)

    add_executable(wio_numeric_builder_test "${CMAKE_SOURCE_DIR}/tests/numeric_builder_test.cpp")
    target_include_directories(wio_numeric_builder_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_n03_numeric_builder_unit COMMAND wio_numeric_builder_test)
    set_tests_properties(wio_n03_numeric_builder_unit PROPERTIES LABELS "native;formatting;float;memory;optimization" TIMEOUT 60)

    add_executable(wio_floating_value_format_test "${CMAKE_SOURCE_DIR}/tests/floating_value_format_test.cpp")
    target_include_directories(wio_floating_value_format_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_n03_value_format COMMAND wio_floating_value_format_test)
    set_tests_properties(wio_n03_value_format PROPERTIES LABELS "native;formatting;float;locale;optimization" TIMEOUT 60)
    add_executable(wio_wir_cpp_concat_plan_test "${CMAKE_SOURCE_DIR}/tests/wir_cpp_concat_plan_test.cpp")
    target_link_libraries(wio_wir_cpp_concat_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n02_concat_plan COMMAND wio_wir_cpp_concat_plan_test)
    set_tests_properties(wio_n02_concat_plan PROPERTIES LABELS "native;string;text;optimization;ownership")
    add_executable(wio_concat_runtime_test "${CMAKE_SOURCE_DIR}/tests/concat_runtime_test.cpp")
    target_include_directories(wio_concat_runtime_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_n02_concat_runtime COMMAND wio_concat_runtime_test)
    set_tests_properties(wio_n02_concat_runtime PROPERTIES LABELS "native;string;text;optimization;memory;exception" TIMEOUT 30)
    add_executable(wio_wir_cpp_append_reuse_plan_test "${CMAKE_SOURCE_DIR}/tests/wir_cpp_append_reuse_plan_test.cpp")
    target_link_libraries(wio_wir_cpp_append_reuse_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_n02_append_plan COMMAND wio_wir_cpp_append_reuse_plan_test)
    set_tests_properties(wio_n02_append_plan PROPERTIES LABELS "native;string;optimization;ownership")
    add_executable(wio_wir_cpp_string_fast_paths_test "${CMAKE_SOURCE_DIR}/tests/wir_cpp_string_fast_paths_test.cpp")
    target_link_libraries(wio_wir_cpp_string_fast_paths_test PRIVATE wio_compiler)
    target_include_directories(wio_wir_cpp_string_fast_paths_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_wir_cpp_string_fast_plan COMMAND wio_wir_cpp_string_fast_paths_test)
    set_tests_properties(wio_wir_cpp_string_fast_plan PROPERTIES LABELS "native;string;optimization;ownership;locale")
    add_executable(wio_integer_format_test "${CMAKE_SOURCE_DIR}/tests/integer_format_test.cpp")
    target_include_directories(wio_integer_format_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_integer_format COMMAND wio_integer_format_test)
    set_tests_properties(wio_integer_format PROPERTIES LABELS "runtime;native;formatting;optimization;locale")
    add_executable(wio_wir_cpp_managed_storage_test "${CMAKE_SOURCE_DIR}/tests/wir_cpp_managed_storage_test.cpp")
    target_link_libraries(wio_wir_cpp_managed_storage_test PRIVATE wio_compiler)
    target_include_directories(wio_wir_cpp_managed_storage_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_wir_cpp_managed_plan COMMAND wio_wir_cpp_managed_storage_test)
    set_tests_properties(wio_wir_cpp_managed_plan PROPERTIES LABELS "native;codegen;optimization;storage;ownership")
    add_executable(wio_wir_cpp_storage_plan_test "${CMAKE_SOURCE_DIR}/tests/wir_cpp_storage_plan_test.cpp")
    target_link_libraries(wio_wir_cpp_storage_plan_test PRIVATE wio_compiler)
    add_test(NAME wio_wir_cpp_storage_plan COMMAND wio_wir_cpp_storage_plan_test)
    set_tests_properties(wio_wir_cpp_storage_plan PROPERTIES LABELS "native;codegen;optimization;storage")
    add_executable(wio_float_parse_portability_test "${CMAKE_SOURCE_DIR}/tests/float_parse_portability_test.cpp")
    target_include_directories(wio_float_parse_portability_test PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
    add_test(NAME wio_float_parse_portability COMMAND wio_float_parse_portability_test)
    set_tests_properties(wio_float_parse_portability PROPERTIES LABELS "runtime;platform;conversion;backend-closure")
    add_executable(wio_backend_workspace_test
        "${CMAKE_SOURCE_DIR}/tests/backend_workspace_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/wio_compiler/common/filesystem/backend_workspace.cpp")
    target_include_directories(wio_backend_workspace_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    add_test(NAME wio_backend_workspace COMMAND wio_backend_workspace_test "${CMAKE_BINARY_DIR}/backend-workspace")
    set_tests_properties(wio_backend_workspace PROPERTIES LABELS "native;platform;unicode;publication;backend-closure")
endif()

target_include_directories(wio_compiler PUBLIC include)

add_dependencies(wio_compiler wio_runtime_backend)

target_sources(wio_compiler
PRIVATE
    include/wio/ast/ast_nodes.def
    include/wio/ast/visitor_overloads.def
)

target_link_libraries(wio_compiler
PUBLIC
    wio_bytecode_core
    dtlog
    frenum
    coco
PRIVATE
    argonaut
)

target_compile_definitions(wio_compiler
    PRIVATE
        WIO_BUILD
        WIO_VERSION="${PROJECT_VERSION}"
        WIO_RUNTIME_INCLUDE_DIR="${WIO_RUNTIME_INCLUDE_DIR}"
        WIO_SDK_INCLUDE_DIR="${WIO_SDK_INCLUDE_DIR}"
        WIO_STD_SOURCE_DIR="${WIO_STD_SOURCE_DIR}"
        WIO_DEFAULT_ROOT_DIR="${WIO_DEFAULT_ROOT_DIR}"
        WIO_BACKEND_CXX_COMPILER="${WIO_BACKEND_CXX_COMPILER_CPP}"
        WIO_BACKEND_AR="${WIO_BACKEND_AR_CPP}"
        WIO_RUNTIME_LIBRARY_FILE="${WIO_BACKEND_RUNTIME_LIBRARY_CPP}"
        _CRT_SECURE_NO_WARNINGS
)

# Warning options
target_compile_options(wio_compiler PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(BUILD_TESTING)
    add_executable(wio_compiler_helper_modules_test
        "${CMAKE_SOURCE_DIR}/tests/compiler_helper_modules_test.cpp"
    )
    target_link_libraries(wio_compiler_helper_modules_test PRIVATE wio_compiler)
    add_test(
        NAME wio_compiler_helper_modules
        COMMAND wio_compiler_helper_modules_test
    )

    add_executable(wio_const_evaluation_limiter_test
        "${CMAKE_SOURCE_DIR}/tests/const_evaluation_limiter_test.cpp"
    )
    target_include_directories(wio_const_evaluation_limiter_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(wio_const_evaluation_limiter_test PRIVATE wio_compiler)
    add_test(
        NAME wio_const_evaluation_limiter
        COMMAND wio_const_evaluation_limiter_test
    )

    add_executable(wio_typed_wir_core_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_core_test.cpp"
    )
    target_link_libraries(wio_typed_wir_core_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_core
        COMMAND wio_typed_wir_core_test
    )

    add_executable(wio_lowered_wir_pipeline_test
        "${CMAKE_SOURCE_DIR}/tests/lowered_wir_pipeline_test.cpp"
    )
    target_link_libraries(wio_lowered_wir_pipeline_test PRIVATE wio_compiler)
    add_test(
        NAME wio_lowered_wir_pipeline
        COMMAND wio_lowered_wir_pipeline_test
    )
    set_property(TEST wio_lowered_wir_pipeline APPEND PROPERTY LABELS freeze)

    add_executable(wio_typed_wir_control_flow_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_control_flow_test.cpp"
    )
    target_link_libraries(wio_typed_wir_control_flow_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_control_flow
        COMMAND wio_typed_wir_control_flow_test
    )

    add_executable(wio_typed_wir_loop_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_loop_test.cpp"
    )
    target_link_libraries(wio_typed_wir_loop_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_loop
        COMMAND wio_typed_wir_loop_test
    )

    add_executable(wio_typed_wir_cfor_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_cfor_test.cpp"
    )
    target_link_libraries(wio_typed_wir_cfor_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_cfor
        COMMAND wio_typed_wir_cfor_test
    )

    add_executable(wio_typed_wir_short_circuit_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_short_circuit_test.cpp"
    )
    target_link_libraries(wio_typed_wir_short_circuit_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_short_circuit
        COMMAND wio_typed_wir_short_circuit_test
    )

    add_executable(wio_typed_wir_literals_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_literals_test.cpp"
    )
    target_link_libraries(wio_typed_wir_literals_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_literals
        COMMAND wio_typed_wir_literals_test
    )

    add_executable(wio_typed_wir_conversion_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_conversion_test.cpp"
    )
    target_link_libraries(wio_typed_wir_conversion_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_conversion
        COMMAND wio_typed_wir_conversion_test
    )

    add_executable(wio_typed_wir_conditional_effects_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_conditional_effects_test.cpp"
    )
    target_link_libraries(wio_typed_wir_conditional_effects_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_conditional_effects
        COMMAND wio_typed_wir_conditional_effects_test
    )

    add_executable(wio_typed_wir_match_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_match_test.cpp"
    )
    target_link_libraries(wio_typed_wir_match_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_match
        COMMAND wio_typed_wir_match_test
    )
    set_property(TEST wio_typed_wir_match APPEND PROPERTY LABELS freeze)

    add_executable(wio_typed_wir_data_match_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_data_match_test.cpp"
    )
    target_link_libraries(wio_typed_wir_data_match_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_data_match
        COMMAND wio_typed_wir_data_match_test
    )
    set_property(TEST wio_typed_wir_data_match APPEND PROPERTY LABELS freeze)

    add_executable(wio_typed_wir_nominal_types_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_nominal_types_test.cpp"
    )
    target_link_libraries(wio_typed_wir_nominal_types_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_nominal_types
        COMMAND wio_typed_wir_nominal_types_test
    )

    add_executable(wio_typed_wir_arrays_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_arrays_test.cpp"
    )
    target_link_libraries(wio_typed_wir_arrays_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_arrays
        COMMAND wio_typed_wir_arrays_test
    )

    add_executable(wio_typed_wir_places_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_places_test.cpp"
    )
    target_link_libraries(wio_typed_wir_places_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_places
        COMMAND wio_typed_wir_places_test
    )

    add_executable(wio_typed_wir_lifecycle_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_lifecycle_test.cpp"
    )
    target_link_libraries(wio_typed_wir_lifecycle_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_lifecycle
        COMMAND wio_typed_wir_lifecycle_test
    )

    add_executable(wio_typed_wir_object_interface_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_object_interface_test.cpp"
    )
    target_link_libraries(wio_typed_wir_object_interface_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_object_interface
        COMMAND wio_typed_wir_object_interface_test
    )

    add_executable(wio_typed_wir_callable_model_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_callable_model_test.cpp"
    )
    target_link_libraries(wio_typed_wir_callable_model_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_callable_model
        COMMAND wio_typed_wir_callable_model_test
    )

    add_executable(wio_typed_wir_value_container_surface_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_value_container_surface_test.cpp"
    )
    target_include_directories(wio_typed_wir_value_container_surface_test PRIVATE
        "${CMAKE_SOURCE_DIR}/runtime/include"
    )
    target_link_libraries(wio_typed_wir_value_container_surface_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_value_container_surface
        COMMAND wio_typed_wir_value_container_surface_test
    )

    add_executable(wio_typed_wir_ownership_cleanup_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_ownership_cleanup_test.cpp"
    )
    target_link_libraries(wio_typed_wir_ownership_cleanup_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_ownership_cleanup
        COMMAND wio_typed_wir_ownership_cleanup_test
    )

    add_executable(wio_typed_wir_native_abi_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_native_abi_test.cpp"
    )
    target_link_libraries(wio_typed_wir_native_abi_test PRIVATE wio_compiler wio_sdk)
    add_test(
        NAME wio_typed_wir_native_abi
        COMMAND wio_typed_wir_native_abi_test
    )

    add_executable(wio_typed_wir_module_sdk_model_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_module_sdk_model_test.cpp"
    )
    target_link_libraries(wio_typed_wir_module_sdk_model_test PRIVATE wio_compiler wio_sdk)
    add_test(
        NAME wio_typed_wir_module_sdk_model
        COMMAND wio_typed_wir_module_sdk_model_test
    )

    add_executable(wio_typed_wir_async_lowering_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_async_lowering_test.cpp"
    )
    target_link_libraries(wio_typed_wir_async_lowering_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_async_lowering
        COMMAND wio_typed_wir_async_lowering_test
    )
    set_property(TEST wio_typed_wir_async_lowering APPEND PROPERTY LABELS freeze)

    add_executable(wio_typed_wir_application_metadata_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_application_metadata_test.cpp"
    )
    target_link_libraries(wio_typed_wir_application_metadata_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_application_metadata
        COMMAND wio_typed_wir_application_metadata_test
    )
    set_property(TEST wio_typed_wir_application_metadata APPEND PROPERTY LABELS freeze)

    add_executable(wio_typed_wir_language_surface_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_language_surface_test.cpp"
    )
    target_link_libraries(wio_typed_wir_language_surface_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_language_surface
        COMMAND wio_typed_wir_language_surface_test
    )

    add_executable(wio_wir_canonical_optimizer_test
        "${CMAKE_SOURCE_DIR}/tests/wir_canonical_optimizer_test.cpp"
    )
    target_link_libraries(wio_wir_canonical_optimizer_test PRIVATE wio_compiler)
    add_test(
        NAME wio_wir_canonical_optimizer
        COMMAND wio_wir_canonical_optimizer_test
    )

    add_executable(wio_bytecode_format_test
        "${CMAKE_SOURCE_DIR}/tests/bytecode_format_test.cpp"
    )
    target_link_libraries(wio_bytecode_format_test PRIVATE wio_compiler)
    add_test(
        NAME wio_bytecode_format
        COMMAND wio_bytecode_format_test
    )

    add_executable(wio_wir_cpp_backend_test
        "${CMAKE_SOURCE_DIR}/tests/wir_cpp_backend_test.cpp"
    )
    add_executable(wio_wir_native_bridge_plan_test
        "${CMAKE_SOURCE_DIR}/tests/wir_native_bridge_plan_test.cpp")
    target_link_libraries(wio_wir_native_bridge_plan_test PRIVATE wio_compiler)
    target_compile_definitions(wio_wir_native_bridge_plan_test PRIVATE
        WIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME wio_wir_native_bridge_plan COMMAND wio_wir_native_bridge_plan_test)
    set_tests_properties(wio_wir_native_bridge_plan PROPERTIES LABELS "wir;cpp;native;pruning;release-gate")
    add_executable(wio_wir_any_array_identity_test
        "${CMAKE_SOURCE_DIR}/tests/wir_any_array_identity_test.cpp")
    target_link_libraries(wio_wir_any_array_identity_test PRIVATE wio_compiler)
    target_compile_definitions(wio_wir_any_array_identity_test PRIVATE
        WIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME wio_wir_any_array_identity COMMAND wio_wir_any_array_identity_test)
    set_tests_properties(wio_wir_any_array_identity PROPERTIES LABELS "wir;bytecode;native;any;array;generic;release-gate")
    add_executable(wio_wir_inferred_array_calls_test "${CMAKE_SOURCE_DIR}/tests/wir_inferred_array_calls_test.cpp")
    target_link_libraries(wio_wir_inferred_array_calls_test PRIVATE wio_compiler)
    target_compile_definitions(wio_wir_inferred_array_calls_test PRIVATE WIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME wio_wir_inferred_array_copy_verifier COMMAND wio_wir_inferred_array_calls_test)
    set_tests_properties(wio_wir_inferred_array_copy_verifier PROPERTIES LABELS "wir;bytecode;array;generic;release-gate")
    target_link_libraries(wio_wir_cpp_backend_test PRIVATE wio_compiler)
    target_compile_definitions(wio_wir_cpp_backend_test PRIVATE
        WIO_TEST_CXX_COMPILER="${WIO_BACKEND_CXX_COMPILER_CPP}"
        WIO_TEST_CXX_COMPILER_ID="GNU"
        WIO_TEST_RUNTIME_INCLUDE="${WIO_RUNTIME_INCLUDE_DIR}"
        WIO_TEST_RUNTIME_LIBRARY="${WIO_BACKEND_RUNTIME_LIBRARY_CPP}"
        WIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    add_test(NAME wio_wir_cpp_generics COMMAND wio_wir_cpp_backend_test --generics-only)
    add_test(NAME wio_wir_cpp_objects COMMAND wio_wir_cpp_backend_test --objects-only)
    add_test(NAME wio_wir_cpp_async COMMAND wio_wir_cpp_backend_test --async-only)
    set_property(TEST wio_wir_cpp_async APPEND PROPERTY LABELS freeze)
    add_test(NAME wio_wir_cpp_native COMMAND wio_wir_cpp_backend_test --native-only)
    add_test(NAME wio_wir_cpp_sdk COMMAND wio_wir_cpp_backend_test --sdk-only)
    add_test(NAME wio_wir_cpp_application COMMAND wio_wir_cpp_backend_test --application-only)
    set_property(TEST wio_wir_cpp_application APPEND PROPERTY LABELS freeze)
    add_test(NAME wio_wir_cpp_reflection COMMAND wio_wir_cpp_backend_test --reflection-only)
    set_tests_properties(wio_wir_cpp_reflection PROPERTIES TIMEOUT 120)
    set_tests_properties(wio_wir_cpp_application PROPERTIES TIMEOUT 120)
    set_tests_properties(wio_wir_cpp_sdk PROPERTIES TIMEOUT 120)
    set_tests_properties(wio_wir_cpp_native PROPERTIES TIMEOUT 90)
    set_tests_properties(wio_wir_cpp_async PROPERTIES TIMEOUT 90)
    add_test(
        NAME wio_wir_cpp_backend
        COMMAND wio_wir_cpp_backend_test
    )

    add_executable(wio_function_type_cpp_string_test
        "${CMAKE_SOURCE_DIR}/tests/function_type_cpp_string_test.cpp"
    )
    target_link_libraries(wio_function_type_cpp_string_test PRIVATE wio_compiler)
    add_test(
        NAME wio_function_type_cpp_string
        COMMAND wio_function_type_cpp_string_test
    )

    add_executable(wio_filesystem_policy_test
        "${CMAKE_SOURCE_DIR}/tests/filesystem_policy_test.cpp"
    )
    target_link_libraries(wio_filesystem_policy_test PRIVATE wio_compiler)
    add_test(
        NAME wio_filesystem_policy
        COMMAND wio_filesystem_policy_test
    )
endif()

if(WIO_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "WIO_BUILD_FUZZERS requires Clang with libFuzzer support.")
    endif()

    add_executable(wio_frontend_fuzzer
        "${CMAKE_SOURCE_DIR}/tests/fuzz/wio_frontend_fuzzer.cpp"
    )
    target_compile_options(wio_compiler PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_libraries(wio_frontend_fuzzer PRIVATE wio_compiler)
    target_compile_options(wio_frontend_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(wio_frontend_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )

    add_executable(wio_bytecode_loader_fuzzer
        "${CMAKE_SOURCE_DIR}/tests/fuzz/bytecode_loader_fuzzer.cpp"
    )
    target_link_libraries(wio_bytecode_loader_fuzzer PRIVATE wio_compiler)
    target_compile_options(wio_bytecode_loader_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(wio_bytecode_loader_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
endif()
