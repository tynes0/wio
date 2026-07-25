set(WIO_RUNTIME_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(WIO_RUNTIME_HEADERS
    "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/format.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/fit.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/intrinsics.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/ref.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/io_errors.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_assert.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_console.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_convert.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_fs.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_hash.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_math.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_io.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_process.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_random.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_regex.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_text.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_time.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/std_type_traits.h"
    "${WIO_RUNTIME_INCLUDE_DIR}/type_reflection.h"
)

set(WIO_RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_assert.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_console.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_convert.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_fs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_hash.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_math.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_io.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_process.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_random.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_regex.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std_time.cpp"
)

set(WIO_BACKEND_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime/backend")
set(WIO_BACKEND_RUNTIME_LIB "${WIO_BACKEND_RUNTIME_DIR}/libwio_runtime.a")
set(WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT "${WIO_BACKEND_RUNTIME_DIR}/std_assert.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE "${WIO_BACKEND_RUNTIME_DIR}/std_console.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_CONVERT "${WIO_BACKEND_RUNTIME_DIR}/std_convert.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_FS "${WIO_BACKEND_RUNTIME_DIR}/std_fs.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_HASH "${WIO_BACKEND_RUNTIME_DIR}/std_hash.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_MATH "${WIO_BACKEND_RUNTIME_DIR}/std_math.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_IO "${WIO_BACKEND_RUNTIME_DIR}/std_io.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_PROCESS "${WIO_BACKEND_RUNTIME_DIR}/std_process.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_RANDOM "${WIO_BACKEND_RUNTIME_DIR}/std_random.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_REGEX "${WIO_BACKEND_RUNTIME_DIR}/std_regex.o")
set(WIO_BACKEND_RUNTIME_OBJ_STD_TIME "${WIO_BACKEND_RUNTIME_DIR}/std_time.o")

file(MAKE_DIRECTORY "${WIO_BACKEND_RUNTIME_DIR}")

set(WIO_BACKEND_RUNTIME_COMPILE_ARGS -std=c++20)
if(NOT WIN32)
    list(APPEND WIO_BACKEND_RUNTIME_COMPILE_ARGS -fPIC)
endif()

add_library(wio_runtime_headers INTERFACE)
target_include_directories(wio_runtime_headers INTERFACE "${WIO_RUNTIME_INCLUDE_DIR}")
target_sources(wio_runtime_headers INTERFACE ${WIO_RUNTIME_HEADERS})

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_HASH}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_hash.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_HASH}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_hash.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_hash.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
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
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_console.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_console.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/detail/io_detail/io_helpers.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_console.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONVERT}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_convert.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_CONVERT}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_convert.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_convert.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
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
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
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
        "${WIO_BACKEND_RUNTIME_OBJ_STD_IO}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_io.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_IO}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_io.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_io.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/io_errors.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_PROCESS}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_process.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_PROCESS}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_process.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_process.h"
        "${WIO_RUNTIME_INCLUDE_DIR}/exception.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_RANDOM}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_random.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_RANDOM}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_random.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_random.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_REGEX}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_regex.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_REGEX}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_regex.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_regex.h"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${WIO_BACKEND_RUNTIME_OBJ_STD_TIME}"
    COMMAND
        ${WIO_BACKEND_CXX_COMPILER}
        ${WIO_BACKEND_RUNTIME_COMPILE_ARGS}
        -I "${WIO_RUNTIME_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/src/std_time.cpp"
        -o "${WIO_BACKEND_RUNTIME_OBJ_STD_TIME}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/std_time.cpp"
        "${WIO_RUNTIME_INCLUDE_DIR}/std_time.h"
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
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONVERT}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_HASH}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_MATH}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_IO}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_PROCESS}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_RANDOM}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_REGEX}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_TIME}"
    DEPENDS
        "${WIO_BACKEND_RUNTIME_OBJ_STD_ASSERT}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONSOLE}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_CONVERT}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_FS}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_HASH}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_MATH}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_IO}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_PROCESS}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_RANDOM}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_REGEX}"
        "${WIO_BACKEND_RUNTIME_OBJ_STD_TIME}"
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
