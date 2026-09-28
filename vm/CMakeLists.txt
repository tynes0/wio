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
endif()
