include_guard(GLOBAL)

function(wio_enable_solution_folders)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake/Generated")
endfunction()

function(_wio_collect_cpp_sources output_variable)
    set(collected_sources)
    foreach(source_root IN LISTS ARGN)
        file(GLOB_RECURSE root_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false
            "${source_root}/*.c"
            "${source_root}/*.cc"
            "${source_root}/*.cpp"
            "${source_root}/*.cxx"
            "${source_root}/*.h"
            "${source_root}/*.hh"
            "${source_root}/*.hpp"
            "${source_root}/*.inl"
            "${source_root}/*.ipp"
        )
        list(APPEND collected_sources ${root_sources})
    endforeach()

    list(FILTER collected_sources EXCLUDE REGEX "[/\\\\](build|build-[^/\\\\]*|\\.wio-build)[/\\\\]")
    list(FILTER collected_sources EXCLUDE REGEX "\\.wio\\.cpp$")
    list(REMOVE_DUPLICATES collected_sources)
    list(SORT collected_sources)
    set(${output_variable} ${collected_sources} PARENT_SCOPE)
endfunction()

function(_wio_add_source_catalog target tree_root)
    set(catalog_sources ${ARGN})
    if(NOT catalog_sources)
        return()
    endif()

    add_custom_target(${target} SOURCES ${catalog_sources})
    source_group(TREE "${tree_root}" FILES ${catalog_sources})
endfunction()

function(wio_add_solution_catalogs)
    file(GLOB_RECURSE wio_cmake_module_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/cmake/*.cmake"
    )
    file(GLOB_RECURSE wio_cmake_project_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/app/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/compiler/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/examples/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/libs/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/runtime/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/sdk/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/vm/CMakeLists.txt"
    )
    set(wio_cmake_files
        "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
        ${wio_cmake_module_files}
        ${wio_cmake_project_files}
    )
    _wio_add_source_catalog(wio_cmake_files "${CMAKE_SOURCE_DIR}" ${wio_cmake_files})

    file(GLOB_RECURSE wio_standard_library_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/std/*.wio"
    )
    _wio_add_source_catalog(wio_standard_library "${CMAKE_SOURCE_DIR}/std" ${wio_standard_library_files})

    file(GLOB_RECURSE wio_cli_source_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/cli/*.wio"
    )
    _wio_add_source_catalog(wio_cli_sources "${CMAKE_SOURCE_DIR}/cli" ${wio_cli_source_files})

    file(GLOB_RECURSE wio_language_test_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/tests/*.wio"
    )
    _wio_add_source_catalog(wio_language_tests "${CMAKE_SOURCE_DIR}/tests" ${wio_language_test_files})

    file(GLOB_RECURSE wio_example_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/examples/*"
    )
    list(FILTER wio_example_files EXCLUDE REGEX "[/\\\\](build|build-[^/\\\\]*|\\.wio-build)[/\\\\]")
    _wio_add_source_catalog(wio_examples "${CMAKE_SOURCE_DIR}/examples" ${wio_example_files})

    _wio_collect_cpp_sources(wio_runtime_source_files "${CMAKE_SOURCE_DIR}/runtime")
    list(FILTER wio_runtime_source_files EXCLUDE REGEX "[/\\\\]third_party[/\\\\]")
    _wio_add_source_catalog(wio_runtime_sources "${CMAKE_SOURCE_DIR}/runtime" ${wio_runtime_source_files})

    _wio_collect_cpp_sources(wio_sdk_source_files "${CMAKE_SOURCE_DIR}/sdk")
    _wio_add_source_catalog(wio_sdk_sources "${CMAKE_SOURCE_DIR}/sdk" ${wio_sdk_source_files})

    _wio_collect_cpp_sources(wio_dependency_source_files
        "${CMAKE_SOURCE_DIR}/libs"
        "${CMAKE_SOURCE_DIR}/runtime/src/third_party"
    )
    _wio_add_source_catalog(wio_dependency_sources "${CMAKE_SOURCE_DIR}" ${wio_dependency_source_files})

    _wio_collect_cpp_sources(wio_native_test_source_files "${CMAKE_SOURCE_DIR}/tests")
    _wio_add_source_catalog(wio_native_test_sources "${CMAKE_SOURCE_DIR}/tests" ${wio_native_test_source_files})

endfunction()

function(_wio_collect_directory_targets directory output_variable)
    get_property(local_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(child_directories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    set(all_targets ${local_targets})

    foreach(child_directory IN LISTS child_directories)
        _wio_collect_directory_targets("${child_directory}" child_targets)
        list(APPEND all_targets ${child_targets})
    endforeach()

    set(${output_variable} ${all_targets} PARENT_SCOPE)
endfunction()

function(_wio_test_folder target output_variable)
    get_target_property(target_source_dir ${target} SOURCE_DIR)
    file(RELATIVE_PATH relative_source_dir "${CMAKE_SOURCE_DIR}" "${target_source_dir}")

    if(relative_source_dir MATCHES "^vm($|/)")
        set(folder "Tests/VM")
    elseif(relative_source_dir MATCHES "^runtime($|/)")
        set(folder "Tests/Runtime")
    elseif(relative_source_dir MATCHES "^sdk($|/)")
        set(folder "Tests/SDK")
    elseif(relative_source_dir MATCHES "^libs($|/)")
        set(folder "Tests/Libraries")
    elseif(relative_source_dir MATCHES "^compiler($|/)")
        set(folder "Tests/Compiler")
    else()
        set(folder "Tests/Integration")
    endif()

    set(${output_variable} "${folder}" PARENT_SCOPE)
endfunction()

function(wio_apply_solution_layout)
    _wio_collect_directory_targets("${CMAKE_SOURCE_DIR}" all_targets)

    foreach(target IN LISTS all_targets)
        if(target STREQUAL "wio_cmake_files")
            set(folder "CMake/Files")
        elseif(target MATCHES "^(Continuous|Experimental|Nightly)")
            set(folder "CMake/CTest")
        elseif(target STREQUAL "wio_examples")
            set(folder "Examples")
        elseif(target STREQUAL "wio_language_tests")
            set(folder "Tests/Wio Sources")
        elseif(target STREQUAL "wio_native_test_sources")
            set(folder "Tests/Native Sources")
        elseif(target STREQUAL "wio_standard_library")
            set(folder "Wio/Standard Library")
        elseif(target STREQUAL "wio_cli_sources" OR target STREQUAL "wio_app" OR
               target STREQUAL "wio_selfhost_cli")
            set(folder "Wio/CLI")
        elseif(target STREQUAL "wio_runtime_sources" OR target MATCHES "^wio_runtime")
            set(folder "Wio/Runtime")
        elseif(target STREQUAL "wio_sdk_sources" OR target STREQUAL "wio_sdk")
            set(folder "Wio/SDK")
        elseif(target STREQUAL "wio_dependency_sources" OR target STREQUAL "dtlog" OR
               target STREQUAL "argonaut" OR target STREQUAL "coco" OR target STREQUAL "frenum")
            set(folder "Dependencies")
        elseif(target STREQUAL "wio_playground")
            set(folder "Playground")
        elseif(target STREQUAL "wio_dist")
            set(folder "Distribution")
        elseif(target MATCHES "^wio_tests($|_)|_test$|_stress$|_fuzzer$|^argonaut_parser_smoke$|^wio_sdk_invalid_api_host$|^wio_fuzz$")
            _wio_test_folder(${target} folder)
        elseif(target STREQUAL "wio_compiler")
            set(folder "Wio/Compiler")
        elseif(target STREQUAL "wio_vm")
            set(folder "Wio/VM")
        else()
            get_target_property(target_source_dir ${target} SOURCE_DIR)
            file(RELATIVE_PATH relative_source_dir "${CMAKE_SOURCE_DIR}" "${target_source_dir}")
            if(relative_source_dir MATCHES "^compiler($|/)")
                set(folder "Wio/Compiler")
            elseif(relative_source_dir MATCHES "^runtime($|/)")
                set(folder "Wio/Runtime")
            elseif(relative_source_dir MATCHES "^sdk($|/)")
                set(folder "Wio/SDK")
            elseif(relative_source_dir MATCHES "^vm($|/)")
                set(folder "Wio/VM")
            elseif(relative_source_dir MATCHES "^libs($|/)")
                set(folder "Dependencies")
            elseif(relative_source_dir MATCHES "^app($|/)")
                set(folder "Wio/CLI")
            else()
                set(folder "Tools")
            endif()
        endif()

        set_property(TARGET ${target} PROPERTY FOLDER "${folder}")
    endforeach()
endfunction()
