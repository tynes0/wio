string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
file(MAKE_DIRECTORY "${root}/relocated" "${root}/pure")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
    --component WioVmRuntime --prefix "${root}/installed"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Runtime-only install failed: ${output}\n${error}")
endif()
foreach(required IN ITEMS "include/wio/vm/machine.h" "include/wio/bytecode/codec.h"
    "sdk/include/wio_native_abi.h" "lib/${WIO_VM_ARCHIVE}" "lib/${WIO_BYTECODE_ARCHIVE}")
    if(NOT EXISTS "${root}/installed/${required}")
        message(FATAL_ERROR "VM runtime install is missing ${required}")
    endif()
endforeach()
set(WIO_VM_RUNNER "${root}/installed/bin/wio-vm${WIO_EXECUTABLE_SUFFIX}")
if(EXISTS "${root}/installed/bin/wio${WIO_EXECUTABLE_SUFFIX}")
    message(FATAL_ERROR "Runtime-only installation unexpectedly contains the compiler")
endif()
execute_process(COMMAND "${WIO_VM_RUNNER}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "Usage: wio-vm")
    message(FATAL_ERROR "Bare standalone runner did not print help: ${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/vm_artifact_pure.wio"
    --emit-bytecode --bytecode-output "${root}/pure.wiob"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Pure bytecode failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${WIO_VM_RUNNER}" "${root}/pure.wiob" -- hello
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Runtime-only bytecode runner failed: ${output}\n${error}")
endif()
set(pure "${root}/pure/app${WIO_EXECUTABLE_SUFFIX}")
execute_process(COMMAND "${WIO_VM_RUNNER}" --package "${root}/pure.wiob" --output "${pure}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Pure VM packaging failed: ${output}\n${error}")
endif()
file(RENAME "${root}/pure.wiob" "${root}/pure.offline")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT "${pure}" hello
    WORKING_DIRECTORY "${root}/relocated" RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Standalone pure VM failed (${result}): ${error}")
endif()
execute_process(COMMAND "${pure}" wrong
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "7")
    message(FATAL_ERROR "Embedded VM lost the Wio exit code (${result}): ${error}")
endif()
# Preserve an existing output. Even an explicitly requested second package
# must not silently clobber the deployed artifact.
file(SHA256 "${pure}" before)
execute_process(COMMAND "${WIO_VM_RUNNER}" --package "${root}/pure.offline" --output "${pure}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
file(SHA256 "${pure}" after)
if("${result}" STREQUAL "0" OR NOT before STREQUAL after OR NOT error MATCHES "already exists")
    message(FATAL_ERROR "Existing VM executable was overwritten: ${output}\n${error}")
endif()
set(library "${root}/native bridge${WIO_SHARED_SUFFIX}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/vm_artifact_native.wio"
    --include-dir "${WIO_ROOT}/tests/native" --target shared --output "${library}"
    --native-bridge-only --bytecode-sidecar "${root}/native.wiob"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Native bridge failed: ${output}\n${error}")
endif()
set(deployed_library "${root}/native köprü 🌍${WIO_SHARED_SUFFIX}")
file(RENAME "${library}" "${deployed_library}")
set(library "${deployed_library}")
set(app "${root}/relocated/Türkçe native app${WIO_EXECUTABLE_SUFFIX}")
execute_process(COMMAND "${WIO_VM_RUNNER}" --package "${root}/native.wiob" --output "${app}" --native-library "${library}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Native VM packaging failed: ${output}\n${error}")
endif()
file(RENAME "${root}/native.wiob" "${root}/native.offline")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT "${app}" "two words" "Türkçe 🌍" --help
    WORKING_DIRECTORY "${root}/pure" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "vm-artifact-native-ok")
    message(FATAL_ERROR "Relocatable native VM failed (${result}): ${output}\n${error}")
endif()
file(RENAME "${root}/relocated/native köprü 🌍${WIO_SHARED_SUFFIX}" "${root}/relocated/bridge.offline")
execute_process(COMMAND "${app}" "two words" "Türkçe 🌍" --help
    WORKING_DIRECTORY "${root}/pure" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if("${result}" STREQUAL "0" OR NOT error MATCHES "native VM module does not exist")
    message(FATAL_ERROR "Missing adjacent native library was ignored: ${output}\n${error}")
endif()
file(RENAME "${root}/relocated/bridge.offline" "${root}/relocated/native köprü 🌍${WIO_SHARED_SUFFIX}")
message(STATUS "Standalone pure/native embedded executables passed without compiler or source files beside them")
