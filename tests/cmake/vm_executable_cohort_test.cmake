cmake_minimum_required(VERSION 3.20)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}")
set(project "${root}/Project Türkçe 🚀")
file(MAKE_DIRECTORY "${root}/cwd" "${project}/wio")
function(run expected)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${root}/cwd"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 240 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR "Native cohort command ${ARGN} returned ${result}, expected ${expected}: ${output}\n${error}")
    endif()
    set(output "${output}" PARENT_SCOPE)
    set(error "${error}" PARENT_SCOPE)
endfunction()
set(platform)
if(WIO_GENERATOR_PLATFORM)
    set(platform -A "${WIO_GENERATOR_PLATFORM}")
endif()
run(0 "${CMAKE_COMMAND}" -S "${WIO_ROOT}/tests/projects/vm_executable_cohort" -B "${root}/build"
    -G "${WIO_GENERATOR}" ${platform} "-DWIO_ROOT=${WIO_ROOT}")
run(0 "${CMAKE_COMMAND}" --build "${root}/build" --config "${WIO_CONFIG}" --parallel 1)
include("${root}/build/paths-${WIO_CONFIG}.cmake")
set(runner "${WIO_BUILD_DIR}/app/${WIO_CONFIG}/wio-vm${WIO_EXECUTABLE_SUFFIX}")
run(0 "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}"
    "${WIO_ROOT}/tests/vm_executable_cohort.wio" --no-builtin --emit-bytecode --bytecode-output "${root}/native.wiob")
set(app "${root}/Native Türkçe 🚀/app${WIO_EXECUTABLE_SUFFIX}")
run(0 "${runner}" --package "${root}/native.wiob" --output "${app}"
    --native-library "${producer}" --runtime-library "${helper}" --version-libraries)
run(0 "${runner}" --verify "${app}")
run(0 "${app}")
run(0 "${runner}" --deploy "${app}" --output "${root}/fresh/renamed${WIO_EXECUTABLE_SUFFIX}")
get_filename_component(helper_name "${helper}" NAME)
file(GLOB deployed_helper "${root}/fresh/wio-native/*/${helper_name}")
list(LENGTH deployed_helper count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR "Private runtime dependency was not copied with its original filename")
endif()
# Mutation must fail before loading native code, not just at dlopen.
file(APPEND "${deployed_helper}" "tampered")
run(1 "${runner}" --verify "${root}/fresh/renamed${WIO_EXECUTABLE_SUFFIX}")
if(NOT error MATCHES "checksum mismatch")
    message(FATAL_ERROR "Runtime dependency integrity was not checked: ${error}")
endif()
run(1 "${root}/fresh/renamed${WIO_EXECUTABLE_SUFFIX}")
if(NOT error MATCHES "checksum mismatch")
    message(FATAL_ERROR "Embedded execution bypassed runtime dependency verification: ${error}")
endif()

# Exercise the self-hosted project path too. Its generated bridge is a producer;
# the external registry has a private helper, but the helper is not a registry.
configure_file("${WIO_ROOT}/tests/vm_artifact_pure.wio" "${project}/wio/main.wio" COPYONLY)
include("${WIO_ROOT}/cmake/WioVmNativePaths.cmake")
wio_vm_native_quote(producer_json "${producer}")
wio_vm_native_quote(helper_json "${helper}")
file(WRITE "${project}/wio.makewio" "name = \"cohort_cli\"
[wio]
entry = \"wio/main.wio\"
target = \"exe\"
sourceRoots = [\"wio\"]
[host]
enabled = false
[vm]
nativeLibraries = [${producer_json}]
runtimeLibraries = [${helper_json}]
executableOutput = \"deploy/app${WIO_EXECUTABLE_SUFFIX}\"
[run]
args = [\"hello\"]
")
set(project_args --project "${project}" --backend vm --build-dir "${root}/cli-build")
run(0 "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}" project run ${project_args})
file(GLOB cli_helper "${project}/deploy/wio-native/*/${helper_name}")
list(LENGTH cli_helper count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR "Project CLI ignored vm.runtimeLibraries")
endif()
file(RENAME "${cli_helper}" "${cli_helper}.offline")
run(1 "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}"
    project run ${project_args} --no-build)
run(0 "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}" project run ${project_args})
if(NOT output MATCHES "needs repair")
    message(FATAL_ERROR "Project CLI did not repair a missing private dependency: ${output}")
endif()
run(0 "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}"
    project package ${project_args} --output-dir "${root}/package")
set(packaged_app "${root}/package/cohort_cli/bin/app${WIO_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${packaged_app}")
    message(FATAL_ERROR "Project package executable missing: ${packaged_app}")
endif()
# All original inputs can disappear after packaging.
file(RENAME "${root}/build/native" "${root}/build/native.offline")
file(RENAME "${root}/native.wiob" "${root}/native.offline")
file(RENAME "${project}/wio" "${project}/wio.offline")
run(0 "${app}")
run(0 "${packaged_app}" hello)
message(STATUS "Native executable cohort and project runtime-library policy passed")
