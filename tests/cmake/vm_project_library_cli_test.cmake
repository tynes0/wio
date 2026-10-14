cmake_minimum_required(VERSION 3.20)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
set(project "${root}/VM library Türkçe 🚀")
file(MAKE_DIRECTORY "${project}/run cwd")
file(COPY "${WIO_ROOT}/tests/projects/vm_library_cli/wio" "${WIO_ROOT}/tests/projects/vm_library_cli/native"
    "${WIO_ROOT}/tests/projects/vm_library_cli/host" DESTINATION "${project}")
set(runtime "${root}/runtime SDK 🚀")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}"
    --component WioVmRuntime --prefix "${runtime}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "VM CLI runtime install failed: ${output}\n${error}")
endif()
if(WIN32)
    set(static_suffix .lib)
else()
    set(static_suffix .a)
endif()
function(manifest kind host)
    if(kind STREQUAL "exe")
        set(entry "wio/entry.wio")
    else()
        set(entry "wio/module.wio")
    endif()
    if(kind STREQUAL "static")
        set(module_suffix "${static_suffix}")
        set(host_args "-DWIO_TEST_STATIC")
    else()
        set(module_suffix "${WIO_SHARED_SUFFIX}")
        set(host_args "")
    endif()
    if(WIO_MINGW)
        set(link_args "\"-municode\"")
    else()
        set(link_args "")
    endif()
    # JSON strings are also valid scalar strings in makewio.
    include("${WIO_ROOT}/cmake/WioVmNativePaths.cmake")
    wio_vm_native_quote(runtime_json "${runtime}")
    wio_vm_native_quote(generator_json "${WIO_GENERATOR}")
    wio_vm_native_quote(cmake_json "${CMAKE_COMMAND}")
    wio_vm_native_quote(compiler_json "${WIO_CXX_COMPILER}")
    file(WRITE "${project}/wio.makewio" "name = \"VM Library CLI\"
[wio]
entry = \"${entry}\"
target = \"${kind}\"
sourceRoots = [\"wio\"]
includeDirs = [\"native\"]
[host]
enabled = ${host}
sources = [\"host/main.cpp\"]
compilerArgs = [\"${host_args}\"]
linkerArgs = [${link_args}]
[vm]
runtimePrefix = ${runtime_json}
cmake = ${cmake_json}
generator = ${generator_json}
cxxCompiler = ${compiler_json}
generatorPlatform = \"${WIO_GENERATOR_PLATFORM}\"
getter = \"WioCliStaticContract\"
moduleOutput = \"deploy/${kind}/module${module_suffix}\"
hostOutput = \"deploy/${kind}/host${WIO_EXECUTABLE_SUFFIX}\"
[run]
args = [\"manifest-arg\"]
workingDirectory = \"run cwd\"
")
endfunction()
function(cli expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${project}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 300 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR "VM library CLI ${ARGN} returned ${result}, expected ${expected}: ${output}\n${error}")
    endif()
    set(output "${output}" PARENT_SCOPE)
    set(error "${error}" PARENT_SCOPE)
endfunction()
foreach(kind_argument IN ITEMS "library" "invalid")
    set(sidecar_arguments)
    if(kind_argument STREQUAL "invalid")
        set(sidecar_arguments --bytecode-sidecar "${root}/rejected.wiob")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}"
        "${project}/wio/module.wio" --target shared --bytecode-sidecar-kind "${kind_argument}" ${sidecar_arguments}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if("${result}" STREQUAL "0" OR NOT "${output}${error}" MATCHES "bytecode-sidecar-kind requires")
        message(FATAL_ERROR "Invalid bytecode contract option was not rejected: ${output}\n${error}")
    endif()
endforeach()
foreach(kind IN ITEMS shared static)
    manifest("${kind}" true)
    set(build "${root}/build-${kind}")
    cli(0 project describe --build-dir "${build}")
    string(JSON described GET "${output}" vm runtimePrefix)
    if(NOT described STREQUAL runtime)
        message(FATAL_ERROR "VM library manifest did not describe its runtime prefix")
    endif()
    cli(0 project build --backend vm --build-dir "${build}" --config "${WIO_CONFIG}")
    cli(0 project run --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --arg "two words")
    if(NOT output MATCHES "vm-library-cli-ok")
        message(FATAL_ERROR "VM ${kind} host did not execute exports/native calls: ${output}")
    endif()
    set(metadata "${build}/vm-module-project/build/artifacts-${WIO_CONFIG}.json")
    file(READ "${metadata}" record)
    string(JSON module GET "${record}" module)
    string(JSON host GET "${record}" host)
    string(JSON bundle GET "${record}" native bundle)
    string(JSON producer GET "${record}" native files 0 name)
    get_filename_component(directory "${module}" DIRECTORY)
    file(SHA256 "${module}" module_hash)
    file(TIMESTAMP "${module}" module_time UTC)
    cli(0 project run --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build --arg "two words")
    file(RENAME "${directory}/${bundle}/${producer}" "${directory}/${bundle}/${producer}.offline")
    cli(1 project run --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build --arg "two words")
    if(NOT "${output}${error}" MATCHES "Missing deployed VM native dependency")
        message(FATAL_ERROR "No-build did not diagnose missing VM library dependencies")
    endif()
    cli(0 project build --backend vm --build-dir "${build}" --config "${WIO_CONFIG}")
    file(SHA256 "${module}" repaired_hash)
    file(TIMESTAMP "${module}" repaired_time UTC)
    if(NOT repaired_hash STREQUAL module_hash OR NOT repaired_time STREQUAL module_time)
        message(FATAL_ERROR "Repair relinked/changed an up-to-date VM module")
    endif()
    # Source and build-time bytecode are unnecessary for no-build run/package.
    file(RENAME "${project}/wio/module.wio" "${project}/wio/module.offline")
    file(RENAME "${build}/interop/vm_library_cli.wiob" "${build}/interop/bytecode.offline")
    cli(0 project run --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build --arg "two words")
    cli(0 project package --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build --output-dir "${root}/packages-${kind}")
    set(package "${root}/packages-${kind}/vm_library_cli")
    file(READ "${package}/wio-package.json" package_json)
    string(JSON entry GET "${package_json}" entrypoint)
    if(NOT EXISTS "${package}/${entry}")
        message(FATAL_ERROR "VM library package omitted its final host")
    endif()
    set(relocated "${root}/relocated-${kind} 🚀")
    file(RENAME "${package}" "${relocated}")
    get_filename_component(module_name "${module}" NAME)
    if(kind STREQUAL "shared")
        set(run_args "${relocated}/lib/${module_name}" manifest-arg "two words")
    else()
        set(run_args manifest-arg "two words")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT "${relocated}/${entry}" ${run_args}
        WORKING_DIRECTORY "${project}/run cwd" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "vm-library-cli-ok")
        message(FATAL_ERROR "Relocated CLI VM ${kind} package failed: ${output}\n${error}")
    endif()
    # Verification detects a changed image, not merely file existence.
    file(RENAME "${host}" "${host}.original")
    file(WRITE "${host}" "not the recorded VM host")
    cli(1 project run --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build --arg "two words")
    if(NOT "${output}${error}" MATCHES "does not match its recorded image")
        message(FATAL_ERROR "Changed VM host passed no-build verification")
    endif()
    file(RENAME "${host}" "${host}.changed")
    file(RENAME "${host}.original" "${host}")
    file(RENAME "${project}/wio/module.offline" "${project}/wio/module.wio")
endforeach()

# Bare static/shared libraries build and package without inventing an Entry.
foreach(kind IN ITEMS shared static)
    manifest("${kind}" false)
    set(build "${root}/bare-${kind}")
    cli(0 project build --backend vm --build-dir "${build}" --config "${WIO_CONFIG}")
    cli(1 project run --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build)
    if(NOT "${output}${error}" MATCHES "library is not executable")
        message(FATAL_ERROR "Bare VM library run did not reject explicitly")
    endif()
    cli(0 project package --backend vm --build-dir "${build}" --config "${WIO_CONFIG}" --no-build
        --output-dir "${root}/bare-packages-${kind}")
    file(READ "${root}/bare-packages-${kind}/vm_library_cli/wio-package.json" document)
    string(JSON entry_type TYPE "${document}" entrypoint)
    if(NOT entry_type STREQUAL "NULL")
        message(FATAL_ERROR "Bare library package invented an executable entry")
    endif()
    if(kind STREQUAL "static")
        file(READ "${build}/vm-module-project/build/artifacts-${WIO_CONFIG}.json" record)
        string(JSON bundle GET "${record}" native bundle)
        string(JSON archive GET "${record}" module)
        get_filename_component(archive_name "${archive}" NAME)
        set(package "${root}/bare-packages-static/vm_library_cli")
        set(generator_args -G "${WIO_GENERATOR}")
        if(WIO_GENERATOR_PLATFORM)
            list(APPEND generator_args -A "${WIO_GENERATOR_PLATFORM}")
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -S "${WIO_ROOT}/tests/projects/vm_library_cli/consumer"
            -B "${root}/consumer" ${generator_args} "-DCMAKE_CXX_COMPILER=${WIO_CXX_COMPILER}"
            "-DCMAKE_BUILD_TYPE=${WIO_CONFIG}" "-DCMAKE_PREFIX_PATH=${runtime}"
            "-DWIO_ARCHIVE=${package}/lib/${archive_name}"
            "-DWIO_NATIVE_MANIFEST=${package}/lib/${bundle}/wio-native-manifest.json"
            "-DWIO_HOST_SOURCE=${project}/host/main.cpp"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45)
        if(NOT "${result}" STREQUAL "0")
            message(FATAL_ERROR "Independent static CLI consumer configure failed: ${output}\n${error}")
        endif()
        set(build_arguments)
        if(WIO_GENERATOR MATCHES "Visual Studio")
            set(build_arguments -- /m:1 /nodeReuse:false)
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" --build "${root}/consumer" --config "${WIO_CONFIG}"
            --target archive_consumer ${build_arguments}
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
        if(NOT "${result}" STREQUAL "0")
            message(FATAL_ERROR "Independent static CLI consumer build failed: ${output}\n${error}")
        endif()
        include("${root}/consumer/consumer-${WIO_CONFIG}.cmake")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=WIO_ROOT "${consumer}" manifest-arg "two words"
            WORKING_DIRECTORY "${project}/run cwd" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
        if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "vm-library-cli-ok")
            message(FATAL_ERROR "Independent static CLI consumer run failed: ${output}\n${error}")
        endif()
    endif()
endforeach()
manifest(exe false)
cli(0 project run --backend vm --build-dir "${root}/exe-compatibility" --config "${WIO_CONFIG}")
message(STATUS "VM project shared/static/host CLI workflow passed")
