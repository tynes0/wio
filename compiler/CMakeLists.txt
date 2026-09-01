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

add_library(wio_compiler STATIC 
    ${WIO_SOURCES}
    ${WIO_HEADERS}
)

add_library(wio-compiler ALIAS wio_compiler)

target_include_directories(wio_compiler PUBLIC include)

add_dependencies(wio_compiler wio_runtime_backend)

target_sources(wio_compiler
PRIVATE
    include/wio/ast/ast_nodes.def
    include/wio/ast/visitor_overloads.def
)

target_link_libraries(wio_compiler
PUBLIC
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

    add_executable(wio_typed_wir_data_match_test
        "${CMAKE_SOURCE_DIR}/tests/typed_wir_data_match_test.cpp"
    )
    target_link_libraries(wio_typed_wir_data_match_test PRIVATE wio_compiler)
    add_test(
        NAME wio_typed_wir_data_match
        COMMAND wio_typed_wir_data_match_test
    )

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
endif()
