file(GLOB_RECURSE WIO_SOURCES CONFIGURE_DEPENDS
    "src/*.h"
    "src/*.cpp"
)

file(GLOB_RECURSE WIO_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.inl"
)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/runtime" WIO_RUNTIME_INCLUDE_DIR)
set(WIO_BACKEND_CXX_COMPILER "g++" CACHE STRING "C++ compiler used for generated Wio output.")

add_library(wio_compiler STATIC 
    ${WIO_SOURCES}
    ${WIO_HEADERS}
)

add_library(wio-compiler ALIAS wio_compiler)

target_include_directories(wio_compiler PUBLIC include)

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
        WIO_RUNTIME_INCLUDE_DIR="${WIO_RUNTIME_INCLUDE_DIR}"
        WIO_BACKEND_CXX_COMPILER="${WIO_BACKEND_CXX_COMPILER}"
)

# Warning options
target_compile_options(wio_compiler PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
