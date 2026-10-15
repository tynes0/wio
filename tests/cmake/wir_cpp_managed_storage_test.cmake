cmake_minimum_required(VERSION 3.20)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
file(MAKE_DIRECTORY "${root}")
configure_file("${WIO_ROOT}/tests/wir_cpp_managed_storage.wio" "${root}/main.wio" COPYONLY)
function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        WORKING_DIRECTORY "${WIO_ROOT}" TIMEOUT 240 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Managed storage command ${ARGN} failed (${result}): ${output}\n${error}")
    endif()
endfunction()
set(compiler "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}")
run(${compiler} "${root}/main.wio" --emit-cpp --include-dir "${WIO_ROOT}/tests/native"
    --output "${root}/native${WIO_EXECUTABLE_SUFFIX}")
set(flags -std=c++20)
set(libraries "${WIO_RUNTIME}")
if(WIN32)
    list(APPEND flags -static -Wa,-mbig-obj)
    list(APPEND libraries -lws2_32 -lshell32)
else()
    list(APPEND libraries -pthread)
endif()
foreach(optimization IN ITEMS -O0 -O2)
    run("${WIO_BACKEND_CXX}" ${flags} ${optimization} "${root}/main.wio.cpp"
        "-I${WIO_ROOT}/runtime/include" "-I${WIO_ROOT}/sdk/include" "-I${WIO_ROOT}/tests/native"
        ${libraries} -o "${root}/native${WIO_EXECUTABLE_SUFFIX}")
    run("${root}/native${WIO_EXECUTABLE_SUFFIX}")
endforeach()
run(${compiler} "${root}/main.wio" --target shared --native-bridge-only
    --include-dir "${WIO_ROOT}/tests/native" --output "${root}/bridge${WIO_SHARED_SUFFIX}"
    --bytecode-sidecar "${root}/main.wiob")
run("${WIO_EXECUTABLE}" --vm-run-bytecode "${root}/main.wiob"
    --native-library "${root}/bridge${WIO_SHARED_SUFFIX}")
message(STATUS "O0/O2 string cells, zero-allocation short echo, ref/capture/Result snapshots and VM parity passed")
