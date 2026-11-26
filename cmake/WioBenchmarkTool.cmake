include_guard(GLOBAL)

set(WIO_BENCH_SOURCE "${CMAKE_SOURCE_DIR}/tools/wio-bench/main.wio")
set(WIO_BENCH_DIR "${CMAKE_BINARY_DIR}/tools/wio-bench/$<CONFIG>")
set(WIO_BENCH_EXECUTABLE "${WIO_BENCH_DIR}/wio-bench${CMAKE_EXECUTABLE_SUFFIX}")
file(GLOB_RECURSE WIO_BENCH_STD_DEPENDENCIES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/std/*.wio")
file(GLOB_RECURSE WIO_BENCH_HEADER_DEPENDENCIES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/runtime/include/*.h")
set(WIO_BENCH_LINK_ARGS)
if(WIN32)
    list(APPEND WIO_BENCH_LINK_ARGS -static -lws2_32 -lshell32 -lpsapi -Wa,-mbig-obj)
else()
    list(APPEND WIO_BENCH_LINK_ARGS -pthread)
endif()

add_custom_command(
    OUTPUT "${WIO_BENCH_EXECUTABLE}"
    BYPRODUCTS "${WIO_BENCH_DIR}/main.wio" "${WIO_BENCH_DIR}/main.wio.cpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WIO_BENCH_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WIO_BENCH_SOURCE}" "${WIO_BENCH_DIR}/main.wio"
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        $<TARGET_FILE:wio_app> "${WIO_BENCH_DIR}/main.wio" --emit-cpp
    COMMAND ${WIO_BACKEND_CXX_COMPILER} -std=c++20 -O2 -ffp-contract=off
        "${WIO_BENCH_DIR}/main.wio.cpp"
        "-I${CMAKE_SOURCE_DIR}/runtime/include" "-I${CMAKE_SOURCE_DIR}/sdk/include"
        "${WIO_BACKEND_RUNTIME_LIBRARY}" ${WIO_BENCH_LINK_ARGS} -o "${WIO_BENCH_EXECUTABLE}"
    DEPENDS wio_app wio_runtime_backend "${WIO_BENCH_SOURCE}" ${WIO_BENCH_STD_DEPENDENCIES} ${WIO_BENCH_HEADER_DEPENDENCIES}
        "${CMAKE_SOURCE_DIR}/runtime/include/std_benchmark.h"
        "${CMAKE_SOURCE_DIR}/runtime/include/benchmark_memory.h"
    COMMENT "Building Wio-written benchmark runner..."
    VERBATIM
)
add_custom_target(wio_bench DEPENDS "${WIO_BENCH_EXECUTABLE}"
    SOURCES "${WIO_BENCH_SOURCE}" "${CMAKE_SOURCE_DIR}/tools/wio-bench/wio.makewio")
set_target_properties(wio_bench PROPERTIES FOLDER "tools")
install(PROGRAMS "${WIO_BENCH_EXECUTABLE}" TYPE BIN OPTIONAL)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/wio-bench/" DESTINATION tools/wio-bench
    PATTERN ".wio-build" EXCLUDE PATTERN "__pycache__" EXCLUDE PATTERN "*.wio.cpp" EXCLUDE)
