string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
file(MAKE_DIRECTORY "${root}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
    --component WioVmRuntime --prefix "${root}/installed"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Session runtime-only install failed: ${output}\n${error}")
endif()
file(RENAME "${root}/installed" "${root}/relocated SDK 🚀")
set(prefix "${root}/relocated SDK 🚀")
foreach(header IN ITEMS "sdk/include/wio_module_session.h" "include/wio/vm/module_session.h")
    if(NOT EXISTS "${prefix}/${header}")
        message(FATAL_ERROR "Session runtime-only install is missing ${header}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/vm_sdk_session_library.wio" --no-builtin
    --target shared --emit-bytecode --bytecode-output "${root}/session.wiob"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Session library bytecode emission failed: ${output}\n${error}")
endif()
set(generator_args -G "${WIO_GENERATOR}")
if(WIO_GENERATOR_PLATFORM)
    list(APPEND generator_args -A "${WIO_GENERATOR_PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${WIO_ROOT}/tests/projects/vm_sdk_session_host" -B "${root}/host"
    ${generator_args} "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_BUILD_TYPE=${WIO_CONFIG}"
    "-DWIO_TEST_BYTECODE=${root}/session.wiob" "-DWIO_TEST_HOST_SOURCE=${WIO_ROOT}/tests/vm_sdk_session_artifact_host.cpp"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Installed-only session host configure failed: ${output}\n${error}")
endif()
set(build_args)
if(WIO_GENERATOR MATCHES "Visual Studio")
    set(build_args -- /m:1 /nodeReuse:false)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${root}/host" --config "${WIO_CONFIG}"
    --target vm_session_host ${build_args}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Installed-only session modules/host build failed: ${output}\n${error}")
endif()
file(RENAME "${root}/session.wiob" "${root}/session.offline")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT
    "${CMAKE_CTEST_COMMAND}" --test-dir "${root}/host" -C "${WIO_CONFIG}" --output-on-failure
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Deployed shared/static VM lifecycle sessions failed: ${output}\n${error}")
endif()
message(STATUS "Installed VM lifecycle session modules passed")
