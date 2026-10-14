string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
file(MAKE_DIRECTORY "${root}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
    --component WioVmRuntime --prefix "${root}/installed"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "VM SDK runtime install failed: ${output}\n${error}")
endif()
# Package paths must not refer to the original build tree or installation.
file(RENAME "${root}/installed" "${root}/relocated runtime 🚀")
set(prefix "${root}/relocated runtime 🚀")
if(EXISTS "${prefix}/bin/wio${WIO_EXECUTABLE_SUFFIX}")
    message(FATAL_ERROR "VM SDK runtime unexpectedly installed the compiler")
endif()
foreach(required IN ITEMS "include/wio/vm/module_adapter.h" "sdk/include/wio_native_sdk.h"
    "lib/cmake/WioVmRuntime/WioVmRuntimeConfig.cmake" "lib/cmake/WioVmRuntime/WioVmModule.cmake")
    if(NOT EXISTS "${prefix}/${required}")
        message(FATAL_ERROR "Missing installed VM SDK surface: ${required}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/vm_sdk_module_library.wio" --no-builtin
    --target shared --emit-bytecode --bytecode-output "${root}/script.wiob"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "SDK library bytecode emission failed: ${output}\n${error}")
endif()
set(generator_args -G "${WIO_GENERATOR}")
string(REPEAT "x" 48 invalid_bytes)
file(WRITE "${root}/invalid.wiob" "${invalid_bytes}")
if(WIO_GENERATOR_PLATFORM)
    list(APPEND generator_args -A "${WIO_GENERATOR_PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${WIO_ROOT}/tests/projects/vm_sdk_module_host" -B "${root}/host"
    ${generator_args} "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_BUILD_TYPE=${WIO_CONFIG}"
    "-DWIO_TEST_BYTECODE=${root}/script.wiob" "-DWIO_TEST_HOST_SOURCE=${WIO_ROOT}/tests/vm_sdk_module_artifact_host.cpp"
    "-DWIO_TEST_INVALID_BYTECODE=${root}/invalid.wiob"
    "-DWIO_TEST_INVALID_HOST_SOURCE=${WIO_ROOT}/tests/vm_sdk_module_invalid_host.cpp"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Installed-only SDK host configure failed: ${output}\n${error}")
endif()
set(build_args)
if(WIO_GENERATOR MATCHES "Visual Studio")
    set(build_args -- /m:1 /nodeReuse:false)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${root}/host" --config "${WIO_CONFIG}"
    --target vm_sdk_host vm_invalid_host ${build_args}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Installed-only shared/static SDK host build failed: ${output}\n${error}")
endif()
# Executing DLL/static modules must not depend on the emitted bytecode file,
# WIO_ROOT, the Wio compiler or source files at deployment time.
file(RENAME "${root}/script.wiob" "${root}/script.offline")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT
    "${CMAKE_CTEST_COMMAND}" --test-dir "${root}/host" -C "${WIO_CONFIG}" --output-on-failure
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Deployed shared/static VM SDK modules failed: ${output}\n${error}")
endif()
message(STATUS "Installed compiler-independent shared/static VM SDK modules passed")
