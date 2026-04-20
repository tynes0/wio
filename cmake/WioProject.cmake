include_guard(GLOBAL)

function(_wio_find_powershell out_var)
    if(WIN32)
        find_program(_wio_powershell NAMES powershell pwsh REQUIRED)
    else()
        find_program(_wio_powershell NAMES pwsh powershell REQUIRED)
    endif()

    set(${out_var} "${_wio_powershell}" PARENT_SCOPE)
endfunction()

function(_wio_default_manifest out_var base_dir)
    foreach(_wio_candidate IN ITEMS "wio.makewio" "makewio" "wio.project.json")
        if(EXISTS "${base_dir}/${_wio_candidate}")
            set(${out_var} "${base_dir}/${_wio_candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "No Wio project manifest was found under '${base_dir}'. Expected one of: wio.makewio, makewio, wio.project.json.")
endfunction()

function(_wio_read_project_info out_var)
    set(options)
    set(oneValueArgs MANIFEST WIO_ROOT CONFIG WORKING_DIRECTORY)
    cmake_parse_arguments(WIO "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT WIO_MANIFEST)
        message(FATAL_ERROR "_wio_read_project_info requires MANIFEST.")
    endif()

    if(NOT WIO_WIO_ROOT)
        message(FATAL_ERROR "_wio_read_project_info requires WIO_ROOT.")
    endif()

    _wio_find_powershell(_wio_powershell)

    set(_wio_command
        "${_wio_powershell}"
        -ExecutionPolicy Bypass
        -File "${WIO_WIO_ROOT}/scripts/Invoke-WioProject.ps1"
        -Project "${WIO_MANIFEST}"
        -Describe
    )

    if(WIO_CONFIG)
        list(APPEND _wio_command -Config "${WIO_CONFIG}")
    endif()

    if(WIO_WORKING_DIRECTORY)
        set(_wio_working_directory "${WIO_WORKING_DIRECTORY}")
    else()
        set(_wio_working_directory "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    execute_process(
        COMMAND ${_wio_command}
        WORKING_DIRECTORY "${_wio_working_directory}"
        RESULT_VARIABLE _wio_result
        OUTPUT_VARIABLE _wio_output
        ERROR_VARIABLE _wio_error
    )

    if(NOT _wio_result EQUAL 0)
        string(STRIP "${_wio_error}" _wio_error)
        message(FATAL_ERROR "Failed to describe Wio project '${WIO_MANIFEST}'.\n${_wio_error}")
    endif()

    string(STRIP "${_wio_output}" _wio_output)
    if(_wio_output STREQUAL "")
        message(FATAL_ERROR "Describe mode returned no metadata for Wio project '${WIO_MANIFEST}'.")
    endif()

    set(${out_var} "${_wio_output}" PARENT_SCOPE)
endfunction()

function(_wio_ensure_sdk_targets wio_root runtime_static_library)
    if(NOT TARGET wio::sdk)
        add_library(wio::sdk INTERFACE IMPORTED GLOBAL)
        set_target_properties(wio::sdk PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${wio_root}/sdk/include"
        )
    endif()

    if(NOT TARGET wio::runtime_static)
        add_library(wio::runtime_static STATIC IMPORTED GLOBAL)
        set_target_properties(wio::runtime_static PROPERTIES
            IMPORTED_LOCATION "${runtime_static_library}"
        )
    endif()
endfunction()

function(wio_add_project_targets name)
    set(options)
    set(oneValueArgs MANIFEST WIO_ROOT CONFIG)
    cmake_parse_arguments(WIO "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT WIO_WIO_ROOT)
        if(DEFINED ENV{WIO_ROOT} AND NOT "$ENV{WIO_ROOT}" STREQUAL "")
            set(WIO_WIO_ROOT "$ENV{WIO_ROOT}")
        else()
            message(FATAL_ERROR "wio_add_project_targets requires WIO_ROOT or an environment variable WIO_ROOT.")
        endif()
    endif()

    if(NOT WIO_MANIFEST)
        _wio_default_manifest(WIO_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    get_filename_component(_wio_manifest "${WIO_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(_wio_root "${WIO_WIO_ROOT}" ABSOLUTE)
    _wio_find_powershell(_wio_powershell)

    if(NOT EXISTS "${_wio_root}/scripts/Invoke-WioProject.ps1")
        message(FATAL_ERROR "Invoke-WioProject.ps1 was not found under WIO_ROOT='${_wio_root}'.")
    endif()

    if(WIO_CONFIG)
        set(_wio_config "${WIO_CONFIG}")
        set(_wio_metadata_config "${WIO_CONFIG}")
    elseif(CMAKE_BUILD_TYPE)
        set(_wio_metadata_config "${CMAKE_BUILD_TYPE}")
    else()
        set(_wio_config "$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Debug>")
        set(_wio_metadata_config "Debug")
    endif()

    _wio_read_project_info(_wio_info
        MANIFEST "${_wio_manifest}"
        WIO_ROOT "${_wio_root}"
        CONFIG "${_wio_metadata_config}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    string(JSON _wio_target_kind GET "${_wio_info}" wio target)
    string(JSON _wio_output GET "${_wio_info}" wio output)
    string(JSON _wio_runtime_static GET "${_wio_info}" runtime staticLibrary)
    string(JSON _wio_host_enabled GET "${_wio_info}" host enabled)
    string(JSON _wio_host_output GET "${_wio_info}" host output)

    _wio_ensure_sdk_targets("${_wio_root}" "${_wio_runtime_static}")

    add_custom_target(${name}_build
        COMMAND ${CMAKE_COMMAND} -E env WIO_ROOT=${_wio_root}
                ${_wio_powershell} -ExecutionPolicy Bypass -File "${_wio_root}/scripts/Invoke-WioProject.ps1"
                -Project "${_wio_manifest}"
                -NoRun
                -Config "${_wio_config}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
    )

    add_custom_target(${name}_run
        COMMAND ${CMAKE_COMMAND} -E env WIO_ROOT=${_wio_root}
                ${_wio_powershell} -ExecutionPolicy Bypass -File "${_wio_root}/scripts/Invoke-WioProject.ps1"
                -Project "${_wio_manifest}"
                -Config "${_wio_config}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDS ${name}_build
        VERBATIM
    )

    set(_wio_imported_target "${name}::wio")
    if(TARGET ${_wio_imported_target})
        message(FATAL_ERROR "Wio imported target '${_wio_imported_target}' already exists.")
    endif()

    if(_wio_target_kind STREQUAL "static")
        add_library(${_wio_imported_target} STATIC IMPORTED GLOBAL)

        set(_wio_interface_links wio::sdk wio::runtime_static)
        if(NOT WIN32 AND CMAKE_DL_LIBS)
            list(APPEND _wio_interface_links "${CMAKE_DL_LIBS}")
        endif()

        set_target_properties(${_wio_imported_target} PROPERTIES
            IMPORTED_LOCATION "${_wio_output}"
            INTERFACE_LINK_LIBRARIES "${_wio_interface_links}"
            WIO_OUTPUT "${_wio_output}"
            WIO_TARGET_KIND "${_wio_target_kind}"
            WIO_MANIFEST "${_wio_manifest}"
            WIO_BUILD_TARGET "${name}_build"
        )
    else()
        add_library(${_wio_imported_target} INTERFACE IMPORTED GLOBAL)
        set_target_properties(${_wio_imported_target} PROPERTIES
            INTERFACE_LINK_LIBRARIES "wio::sdk"
            WIO_OUTPUT "${_wio_output}"
            WIO_TARGET_KIND "${_wio_target_kind}"
            WIO_MANIFEST "${_wio_manifest}"
            WIO_BUILD_TARGET "${name}_build"
        )
    endif()

    if(_wio_host_enabled STREQUAL "true")
        set(_wio_host_target "${name}::host")
        if(TARGET ${_wio_host_target})
            message(FATAL_ERROR "Wio imported host target '${_wio_host_target}' already exists.")
        endif()

        add_executable(${_wio_host_target} IMPORTED GLOBAL)
        set_target_properties(${_wio_host_target} PROPERTIES
            IMPORTED_LOCATION "${_wio_host_output}"
            WIO_OUTPUT "${_wio_host_output}"
            WIO_TARGET_KIND "host"
            WIO_MANIFEST "${_wio_manifest}"
            WIO_BUILD_TARGET "${name}_build"
        )
    endif()
endfunction()

function(wio_target_link_project consumer)
    if(NOT TARGET ${consumer})
        message(FATAL_ERROR "Consumer target '${consumer}' does not exist.")
    endif()

    if(ARGC EQUAL 2)
        set(_wio_visibility PRIVATE)
        set(_wio_project_name "${ARGV1}")
    elseif(ARGC EQUAL 3)
        set(_wio_visibility "${ARGV1}")
        set(_wio_project_name "${ARGV2}")
    else()
        message(FATAL_ERROR "Usage: wio_target_link_project(<consumer> [PRIVATE|PUBLIC|INTERFACE] <project_name>)")
    endif()

    if(NOT _wio_visibility MATCHES "^(PRIVATE|PUBLIC|INTERFACE)$")
        message(FATAL_ERROR "Unsupported visibility '${_wio_visibility}'. Expected PRIVATE, PUBLIC, or INTERFACE.")
    endif()

    set(_wio_imported_target "${_wio_project_name}::wio")
    if(NOT TARGET ${_wio_imported_target})
        message(FATAL_ERROR "Wio imported target '${_wio_imported_target}' does not exist. Call wio_add_project_targets(${_wio_project_name}) first.")
    endif()

    get_target_property(_wio_target_kind ${_wio_imported_target} WIO_TARGET_KIND)
    get_target_property(_wio_build_target ${_wio_imported_target} WIO_BUILD_TARGET)

    if(NOT _wio_target_kind STREQUAL "static")
        message(FATAL_ERROR "wio_target_link_project only supports static Wio outputs. '${_wio_project_name}' is '${_wio_target_kind}' and should be runtime-loaded instead.")
    endif()

    add_dependencies(${consumer} ${_wio_build_target})
    target_link_libraries(${consumer} ${_wio_visibility} ${_wio_imported_target})
endfunction()

function(wio_get_project_output out_var project_name)
    set(_wio_imported_target "${project_name}::wio")
    if(NOT TARGET ${_wio_imported_target})
        message(FATAL_ERROR "Wio imported target '${_wio_imported_target}' does not exist.")
    endif()

    get_target_property(_wio_output ${_wio_imported_target} WIO_OUTPUT)
    set(${out_var} "${_wio_output}" PARENT_SCOPE)
endfunction()

function(wio_get_host_output out_var project_name)
    set(_wio_host_target "${project_name}::host")
    if(NOT TARGET ${_wio_host_target})
        message(FATAL_ERROR "Wio host target '${_wio_host_target}' does not exist.")
    endif()

    get_target_property(_wio_output ${_wio_host_target} WIO_OUTPUT)
    set(${out_var} "${_wio_output}" PARENT_SCOPE)
endfunction()
