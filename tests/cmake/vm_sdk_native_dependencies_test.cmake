string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
file(MAKE_DIRECTORY "${root}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
    --component WioVmRuntime --prefix "${root}/installed"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Native deployment runtime install failed: ${output}\n${error}")
endif()
file(RENAME "${root}/installed" "${root}/relocated SDK 🚀")
set(prefix "${root}/relocated SDK 🚀")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_ROOT}/tests/vm_sdk_native_dependencies_library.wio" --no-builtin
    --target shared --emit-bytecode --bytecode-output "${root}/native.wiob"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Native dependency library emission failed: ${output}\n${error}")
endif()
set(generator_args -G "${WIO_GENERATOR}")
if(WIO_GENERATOR_PLATFORM)
    list(APPEND generator_args -A "${WIO_GENERATOR_PLATFORM}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${WIO_ROOT}/tests/projects/vm_sdk_native_dependencies_host"
    -B "${root}/host" ${generator_args} "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_BUILD_TYPE=${WIO_CONFIG}"
    "-DWIO_TEST_BYTECODE=${root}/native.wiob" "-DWIO_TEST_FIXTURES=${WIO_ROOT}/tests/native"
    "-DWIO_TEST_HOST_SOURCE=${WIO_ROOT}/tests/vm_sdk_native_dependencies_host.cpp"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Installed native dependency host configure failed: ${output}\n${error}")
endif()
set(build_args)
if(WIO_GENERATOR MATCHES "Visual Studio")
    set(build_args -- /m:1 /nodeReuse:false)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${root}/host" --config "${WIO_CONFIG}" --target native_host ${build_args}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Installed native dependency modules build failed: ${output}\n${error}")
endif()
include("${root}/host/paths-${WIO_CONFIG}.cmake")
set(deployed "${root}/deployed app 🚀")
file(MAKE_DIRECTORY "${deployed}")
foreach(item IN ITEMS shared bad host)
    get_filename_component(name "${test_${item}}" NAME)
    configure_file("${test_${item}}" "${deployed}/${name}" COPYONLY)
    set(deployed_${item} "${deployed}/${name}")
endforeach()
foreach(item IN ITEMS shared static bad)
    file(READ "${test_${item}_manifest}" document)
    string(JSON bundle GET "${document}" bundle)
    if(item STREQUAL "static")
        get_filename_component(parent "${test_host}" DIRECTORY)
    else()
        get_filename_component(parent "${test_${item}}" DIRECTORY)
    endif()
    file(COPY "${parent}/${bundle}" DESTINATION "${deployed}")
    # Preserve the module-specific prefix as well as the hash directory.
    get_filename_component(prefix_name "${bundle}" DIRECTORY)
    file(MAKE_DIRECTORY "${deployed}/${prefix_name}")
    get_filename_component(digest "${bundle}" NAME)
    file(RENAME "${deployed}/${digest}" "${deployed}/${bundle}")
    set(deployed_${item}_producer "${deployed}/${bundle}/${test_producer_name}")
endforeach()
# Rename build-time producer output and bytecode: the relocated process must
# not resolve a provider from the source build tree or rely on its .wiob.
file(RENAME "${root}/host/original-producers" "${root}/host/producers.offline")
file(RENAME "${root}/native.wiob" "${root}/native.offline")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT "${deployed_host}"
    "${deployed_shared}" "${deployed_shared_producer}" "${deployed_static_producer}"
    "${deployed_bad}" "${deployed_bad_producer}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Relocated native dependency host failed: ${output}\n${error}")
endif()
message(STATUS "Installed native dependency host passed: ${output}")
