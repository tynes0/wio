cmake_minimum_required(VERSION 3.20)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
file(MAKE_DIRECTORY "${root}/cwd")
function(run expected)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${root}/cwd"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 360 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR "Platform qualification ${ARGN} returned ${result}, expected ${expected}: ${output}\n${error}")
    endif()
    set(output "${output}" PARENT_SCOPE)
endfunction()
set(prefix "${root}/runtime original")
run(0 "${CMAKE_COMMAND}" --install "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
    --component WioVmRuntime --prefix "${prefix}")
set(relocated "${root}/relocated SDK Türkçe 🚀")
file(RENAME "${prefix}" "${relocated}")
if(EXISTS "${relocated}/bin/wio${WIO_EXECUTABLE_SUFFIX}")
    message(FATAL_ERROR "Runtime-only prefix contains the compiler")
endif()
set(compiler "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}")
run(0 ${compiler} "${WIO_ROOT}/tests/vm_sdk_native_dependencies_library.wio" --no-builtin
    --target shared --emit-bytecode --bytecode-output "${root}/library.wiob")
set(platform)
if(WIO_GENERATOR_PLATFORM)
    set(platform -A "${WIO_GENERATOR_PLATFORM}")
endif()
set(generator -G "${WIO_GENERATOR}" ${platform})
run(0 "${CMAKE_COMMAND}" -S "${WIO_ROOT}/tests/projects/vm_sdk_native_dependencies_host" -B "${root}/consumer"
    ${generator} "-DCMAKE_BUILD_TYPE=${WIO_CONFIG}" "-DCMAKE_PREFIX_PATH=${relocated}"
    "-DWIO_TEST_BYTECODE=${root}/library.wiob" "-DWIO_TEST_FIXTURES=${WIO_ROOT}/tests/native"
    "-DWIO_TEST_HOST_SOURCE=${WIO_ROOT}/tests/vm_sdk_native_dependencies_host.cpp" -DWIO_TEST_INSTALL_WORKFLOW=ON)
run(0 "${CMAKE_COMMAND}" --build "${root}/consumer" --target native_host --config "${WIO_CONFIG}" --parallel 1)
include("${root}/consumer/paths-${WIO_CONFIG}.cmake")
foreach(item IN ITEMS shared static bad)
    file(READ "${test_${item}_manifest}" document)
    string(JSON bundle_${item} GET "${document}" bundle)
endforeach()
# Unix installation must honor DESTDIR without embedding that staging root.
if(WIN32)
    set(staged "${root}/stage")
    run(0 "${CMAKE_COMMAND}" --install "${root}/consumer" --config "${WIO_CONFIG}"
        --component VmFixture --prefix "${staged}")
else()
    run(0 "${CMAKE_COMMAND}" -E env "DESTDIR=${root}/destdir" "${CMAKE_COMMAND}"
        --install "${root}/consumer" --config "${WIO_CONFIG}" --component VmFixture --prefix /opt/wio-qualification)
    set(staged "${root}/destdir/opt/wio-qualification")
endif()
set(distribution "${root}/relocated app Türkçe 🚀")
file(RENAME "${staged}" "${distribution}")
foreach(item IN ITEMS shared bad host)
    get_filename_component(name "${test_${item}}" NAME)
    set(final_${item} "${distribution}/bin/${name}")
endforeach()
# All producer build directories and external bytecode disappear before running.
file(RENAME "${root}/consumer/original-producers" "${root}/consumer/original-producers.offline")
file(RENAME "${root}/library.wiob" "${root}/library.offline")
run(0 "${final_host}" "${final_shared}" "${distribution}/bin/${bundle_shared}/${test_producer_name}"
    "${distribution}/bin/${bundle_static}/${test_producer_name}" "${final_bad}"
    "${distribution}/bin/${bundle_bad}/${test_producer_name}")
if(NOT output MATCHES "vm-sdk-native-dependencies-ok")
    message(FATAL_ERROR "Relocated native session lifecycle was not qualified")
endif()

# Independently exercise project command selection against the relocated prefix.
set(project "${root}/Project Türkçe 🚀")
file(MAKE_DIRECTORY "${project}")
file(COPY "${WIO_ROOT}/tests/projects/vm_library_cli/wio" "${WIO_ROOT}/tests/projects/vm_library_cli/native"
    "${WIO_ROOT}/tests/projects/vm_library_cli/host" DESTINATION "${project}")
include("${WIO_ROOT}/cmake/WioVmNativePaths.cmake")
wio_vm_native_quote(prefix_json "${relocated}")
wio_vm_native_quote(cmake_json "${CMAKE_COMMAND}")
wio_vm_native_quote(generator_json "${WIO_GENERATOR}")
wio_vm_native_quote(compiler_json "${WIO_CXX_COMPILER}")
foreach(kind IN ITEMS shared static)
    if(kind STREQUAL "static")
        set(compile_args "\"-DWIO_TEST_STATIC\"")
    else()
        set(compile_args "")
    endif()
    if(WIO_MINGW)
        set(link_args "\"-municode\"")
    else()
        set(link_args "")
    endif()
    file(WRITE "${project}/wio.makewio" "name = \"platform_cli\"
[wio]
entry = \"wio/module.wio\"
target = \"${kind}\"
sourceRoots = [\"wio\"]
includeDirs = [\"native\"]
[host]
enabled = true
sources = [\"host/main.cpp\"]
compilerArgs = [${compile_args}]
linkerArgs = [${link_args}]
[vm]
runtimePrefix = ${prefix_json}
cmake = ${cmake_json}
generator = ${generator_json}
generatorPlatform = \"${WIO_GENERATOR_PLATFORM}\"
cxxCompiler = ${compiler_json}
getter = \"WioCliStaticContract\"
[run]
args = [\"manifest-arg\"]
")
    set(args --project "${project}" --backend vm --config "${WIO_CONFIG}"
        --build-dir "${root}/build ${kind} Türkçe 🚀")
    run(0 ${compiler} project run ${args} --arg "two words")
    if(NOT output MATCHES "vm-library-cli-ok")
        message(FATAL_ERROR "${kind} project host failed its value/argument contract")
    endif()
    run(0 ${compiler} project package ${args} --no-build --output-dir "${root}/packages-${kind}")
    file(READ "${root}/packages-${kind}/platform_cli/wio-package.json" document)
    string(JSON entry GET "${document}" entrypoint)
    set(package_${kind} "${root}/packages-${kind}/platform_cli")
    set(entry_${kind} "${entry}")
endforeach()
file(RENAME "${project}/wio" "${project}/wio.offline")
file(RENAME "${relocated}" "${relocated}.offline")
foreach(kind IN ITEMS shared static)
    if(WIN32)
        run(0 "${package_${kind}}/run.cmd" manifest-arg "two words")
    else()
        run(0 "${package_${kind}}/run.sh" manifest-arg "two words")
    endif()
    if(NOT output MATCHES "vm-library-cli-ok")
        message(FATAL_ERROR "${kind} package depended on the original SDK/source installation")
    endif()
endforeach()
message(STATUS "Platform install/DESTDIR, relocation, native lifecycle, shared/static CLI and source-independent packages passed")
