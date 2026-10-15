cmake_minimum_required(VERSION 3.20)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${WIO_OUTPUT_DIR}/${suffix}/project with spaces")
file(MAKE_DIRECTORY "${root}/wio" "${root}/native/include")
file(WRITE "${root}/wio.makewio" [=[
schemaVersion = 1
name = "LauncherProbe"
[wio]
entry = "wio/main.wio"
target = "exe"
sourceRoots = ["wio"]
includeDirs = ["native/include"]
nativeSources = []
backendArgs = []
[host]
enabled = false
[build]
config = "Debug"
buildDir = ".wio-build"
[outputs]
directory = ""
wioName = "probe"
[run]
workingDirectory = "."
]=])
file(WRITE "${root}/native/include/flags.h" [=[
#pragma once
namespace playground_flags
{
    inline bool IsRelease()
    {
#if defined(NDEBUG) && defined(__OPTIMIZE__)
        return true;
#else
        return false;
#endif
    }
}
]=])
file(WRITE "${root}/wio/main.wio" [=[
using cpp::header("flags.h");
[native, CppName(playground_flags::IsRelease)]
fn IsRelease() -> bool;
fn Entry(args: string[]) -> i32
{
    if (args.Count() != 5usize) return 21;
    if (IsRelease() != (args[1usize] == "Release")) return 22;
    if (args[2usize] != "two words" or args[3usize] != "--vm" or args[4usize] != "") return 23;
    return 0;
}
]=])
function(run)
    # ARGN list expansion drops a trailing empty item; add this test argument
    # explicitly so the real argv transport, rather than CMake, is exercised.
    execute_process(COMMAND ${ARGN} "" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        WORKING_DIRECTORY "${root}" TIMEOUT 360 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Launcher project command ${ARGN}: ${result}; ${output}\n${error}")
    endif()
    set(last_output "${output}" PARENT_SCOPE)
endfunction()
set(project --project "${root}/wio.makewio")
if(WIO_TEST_MODE STREQUAL "profiles")
    foreach(profile IN ITEMS Debug Release)
        string(TOLOWER "${profile}" lower)
        run("${WIO_LAUNCHER}" --native "--${lower}" ${project} -- "${profile}" "two words" --vm "")
        set(output "${root}/.wio-build/playground/native-${lower}/interop/probe${WIO_EXE_SUFFIX}")
        if(NOT EXISTS "${output}" OR NOT EXISTS "${output}.build-args")
            message(FATAL_ERROR "Missing isolated ${profile} output/signature")
        endif()
        file(READ "${output}.build-args" signature)
        if(profile STREQUAL "Release" AND NOT signature MATCHES "-O2")
            message(FATAL_ERROR "Release did not reach the C++ compiler")
        elseif(profile STREQUAL "Debug" AND NOT signature MATCHES "-O0")
            message(FATAL_ERROR "Debug did not reach the C++ compiler")
        endif()
    endforeach()
    run("${WIO_LAUNCHER}" --native --debug ${project} -- Debug "two words" --vm "")
    if(NOT last_output MATCHES "up to date")
        message(FATAL_ERROR "Returning to an unchanged profile did not reuse its own output: ${last_output}")
    endif()
    set(vm_configs "${WIO_CONFIG}")
    if(WIO_MULTI_CONFIG)
        set(vm_configs Debug Release)
    endif()
    foreach(vm_config IN LISTS vm_configs)
        run("${WIO_LAUNCHER}" --vm --config "${vm_config}" ${project} -- "${vm_config}" "two words" --vm "")
        string(TOLOWER "${vm_config}" lower)
        set(vm_output "${root}/.wio-build/playground/vm-${lower}/interop/vm/probe${WIO_EXE_SUFFIX}")
        set(vm_runner "${WIO_BUILD_DIR}/app/${vm_config}/wio-vm${WIO_EXE_SUFFIX}")
        file(READ "${vm_output}" packaged_header LIMIT 512 HEX)
        file(READ "${vm_runner}" runtime_header LIMIT 512 HEX)
        if(NOT packaged_header STREQUAL runtime_header)
            message(FATAL_ERROR "Packaged application does not embed the selected VM runtime image")
        endif()
    endforeach()
else()
    set(core "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}")
    set(common project run ${project} --config Debug --build-dir .wio-build/signature)
    run(${core} ${common} --backend-arg -O0 --backend-arg -UNDEBUG -- Debug "two words" --vm "")
    run(${core} ${common} --backend-arg=-O2 --backend-arg=-DNDEBUG -- Release "two words" --vm "")
    if(last_output MATCHES "up to date")
        message(FATAL_ERROR "Changed flags reused a stale executable")
    endif()
    run(${core} ${common} --backend-arg=-O2 --backend-arg=-DNDEBUG -- Release "two words" --vm "")
    if(NOT last_output MATCHES "up to date")
        message(FATAL_ERROR "Unchanged flags unexpectedly rebuilt: ${last_output}")
    endif()
    execute_process(COMMAND ${core} ${common} --backend-arg=-this-is-an-invalid-compiler-option
        RESULT_VARIABLE failed OUTPUT_VARIABLE failed_output ERROR_VARIABLE failed_error
        WORKING_DIRECTORY "${root}" TIMEOUT 60)
    if("${failed}" STREQUAL "0")
        message(FATAL_ERROR "Invalid backend argument unexpectedly succeeded")
    endif()
    file(READ "${root}/.wio-build/signature/interop/probe${WIO_EXE_SUFFIX}.build-args" invalidated)
    if(NOT invalidated STREQUAL "")
        message(FATAL_ERROR "Failed compilation retained a valid cache signature")
    endif()
    run(${core} ${common} --backend-arg=-O2 --backend-arg=-DNDEBUG -- Release "two words" --vm "")
    if(last_output MATCHES "up to date")
        message(FATAL_ERROR "Failed prior compilation reused a stale success signature")
    endif()
endif()
message(STATUS "Launcher/profile/backend arguments and runtime flags verified with isolated scratch project")
