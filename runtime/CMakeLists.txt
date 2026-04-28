set(WIO_RUNTIME_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(WIO_RUNTIME_HEADERS
    "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/fit.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/intrinsics.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/ref.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_assert.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_console.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_fs.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_math.h"
)

set(WIO_RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_assert.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_console.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_fs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_math.cpp"
)

set(WIO_BACKEND_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime/backend")
set(WIO_BACKEND_RUNTIME_LIB "${WIO_BACKEND_RUNTIME_DIR}/libwio_runtime.a")
set(WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT "${WIO_BACKEND_RUNTIME_DIR}/std_assert.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE "${WIO_BACKEND_RUNTIME_DIR}/std_console.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_FS "${WIO_BACKEND_RUNTIME_DIR}/std_fs.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_MATH "${WIO_BACKEND_RUNTIME_DIR}/std_math.o")

file(MAKE_DIRECTORY "${WIO_BACKEND_RUNTIME_DIR}")

add_library(wio_runtime_headers INTERFACE)
target_include_directories(wio_runtime_headers INTERFACE "${WIO_RUNTIME_INCLUDE_DIR}")
target_sources(wio_runtime_headers INTERFACE ${WIO_RUNTIME_HEADERS})

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        -std=c++20
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_assert.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_assert.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_assert.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        -std=c++20
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_console.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_console.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/detail/io_helpers.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_console.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        -std=c++20
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_fs.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_fs.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_fs.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_MATH}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        -std=c++20
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_math.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_MATH}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_math.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_math.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_LIB}"
    COMMAND
        ${WIO_BACKEND_AR}
        rcs
        "${WIO_BACKEND_RUNTIME_LIB}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_MATH}"
    DEPENDS
        "${WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_MATH}"
    VERBATIM
)

add_custom_target(wio_runtime_backend ALL
    DEPENDS "${WIO_BACKEND_RUNTIME_LIB}"
)

set(WIO_BACKEND_RUNTIME_LIBRARY "${WIO_BACKEND_RUNTIME_LIB}" CACHE INTERNAL "Backend-compatible Wio runtime library")

install(DIRECTORY "${WIO_RUNTIME_INCLUDE_DIR}/"
    DESTINATION runtime/include
)

install(FILES "${WIO_BACKEND_RUNTIME_LIB}"
    DESTINATION runtime/lib
)
