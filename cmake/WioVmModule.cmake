include_guard(GLOBAL)

set(_WIO_VM_EMBED_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/EmbedWioVmModule.cmake")

# Bytecode must already exist or be produced by another target/custom command.
# Shared modules expose WioGetSdkModuleContract; static modules require a unique
# getter name, avoiding duplicate symbols when multiple modules share one host.
function(wio_add_vm_module target)
    cmake_parse_arguments(ARG "STATIC;SHARED" "BYTECODE;GETTER;SESSION_GETTER" "" ${ARGN})
    if(ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES OR NOT ARG_BYTECODE OR
       (ARG_STATIC AND ARG_SHARED))
        message(FATAL_ERROR "wio_add_vm_module requires BYTECODE and at most one of STATIC/SHARED")
    endif()
    if(NOT ARG_SESSION_GETTER)
        if(ARG_STATIC)
            set(ARG_SESSION_GETTER "${ARG_GETTER}SessionApi")
        else()
            set(ARG_SESSION_GETTER WioGetSdkModuleSessionApi)
        endif()
    endif()
    if(ARG_STATIC)
        set(kind STATIC)
        if(NOT ARG_GETTER)
            message(FATAL_ERROR "Static VM modules require a unique GETTER identifier")
        endif()
    else()
        set(kind SHARED)
        if(NOT ARG_GETTER)
            set(ARG_GETTER WioGetSdkModuleContract)
        endif()
    endif()
    if(NOT ARG_GETTER MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "VM module GETTER must be a C identifier")
    endif()
    if(NOT ARG_SESSION_GETTER MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" OR ARG_SESSION_GETTER STREQUAL ARG_GETTER)
        message(FATAL_ERROR "VM module SESSION_GETTER must be a distinct C identifier")
    endif()
    if(DEFINED WioVmRuntime_CXX_COMPILER_ID AND
       NOT CMAKE_CXX_COMPILER_ID STREQUAL WioVmRuntime_CXX_COMPILER_ID)
        message(FATAL_ERROR "VM module and installed runtime must use matching C++ toolchains")
    endif()
    get_filename_component(bytecode "${ARG_BYTECODE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${target}.vm-module.cpp")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${bytecode}" "-DOUTPUT=${output}"
            "-DGETTER=${ARG_GETTER}" "-DSESSION_GETTER=${ARG_SESSION_GETTER}" "-DKIND=${kind}"
            -P "${_WIO_VM_EMBED_SCRIPT}"
        DEPENDS "${bytecode}" "${_WIO_VM_EMBED_SCRIPT}"
        VERBATIM)
    add_library(${target} ${kind} "${output}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(TARGET Wio::wio_vm)
        target_link_libraries(${target} PRIVATE Wio::wio_vm)
    elseif(TARGET wio_vm)
        target_link_libraries(${target} PRIVATE wio_vm)
    else()
        message(FATAL_ERROR "Load WioVmRuntime or build the Wio runtime before declaring a VM module")
    endif()
    if(MSVC AND WioVmRuntime_MSVC_STATIC_RUNTIME)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endfunction()
