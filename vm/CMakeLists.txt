file(GLOB_RECURSE WIO_VM_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

file(GLOB_RECURSE WIO_VM_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
)

add_library(wio_vm STATIC
    ${WIO_VM_SOURCES}
    ${WIO_VM_HEADERS}
)

add_library(wio-vm ALIAS wio_vm)

target_include_directories(wio_vm PUBLIC include)
target_link_libraries(wio_vm PUBLIC wio_compiler)

target_compile_options(wio_vm PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(BUILD_TESTING)
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
endif()
