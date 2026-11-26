set(WIO_SELFHOST_CLI_SOURCE "${CMAKE_SOURCE_DIR}/cli/main.wio")

set(WIO_SELFHOST_PROJECT_NEW_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_new.wio")

set(WIO_SELFHOST_PROJECT_MANIFEST_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_manifest.wio")

set(WIO_SELFHOST_PROJECT_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_commands.wio")
set(WIO_SELFHOST_PROJECT_VM_SOURCE "${CMAKE_SOURCE_DIR}/cli/project_vm.wio")

set(WIO_SELFHOST_COMMON_SOURCE "${CMAKE_SOURCE_DIR}/cli/common.wio")

set(WIO_SELFHOST_GLOBAL_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/global_commands.wio")

set(WIO_SELFHOST_ENV_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/env_commands.wio")

set(WIO_SELFHOST_BIND_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/bind_commands.wio")

set(WIO_SELFHOST_PACKAGE_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/package_commands.wio")

set(WIO_SELFHOST_FILE_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/file_commands.wio")

set(WIO_SELFHOST_DEV_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/dev_commands.wio")

set(WIO_SELFHOST_PERF_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/perf_commands.wio")

set(WIO_SELFHOST_MIGRATE_COMMANDS_SOURCE "${CMAKE_SOURCE_DIR}/cli/migrate_commands.wio")

set(WIO_SELFHOST_ARGONAUT_SOURCE "${CMAKE_SOURCE_DIR}/cli/vendor/argonaut.wio")

add_executable(wio_app
    main.cpp
    vm_runner.cpp
    vm_runner.h
)

set_target_properties(wio_app PROPERTIES
    OUTPUT_NAME "wio"
)

target_link_libraries(wio_app
    PRIVATE
        wio_compiler
        wio_runtime_host
        wio_vm
)

# Recursive typed lowering has substantially larger frames in unoptimized
# Windows builds. The default 1 MiB stack fails on otherwise ordinary nested
# container/borrow expressions. Reserve on demand; this is not committed RAM.
if(WIN32)
    if(MSVC)
        target_link_options(wio_app PRIVATE /STACK:16777216)
    else()
        target_link_options(wio_app PRIVATE "-Wl,--stack,16777216")
    endif()
endif()

target_include_directories(wio_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/include
        ${CMAKE_SOURCE_DIR}/vm/include
)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" WIO_APP_DEFAULT_ROOT_DIR)

target_compile_definitions(wio_app
    PRIVATE
        WIO_DEFAULT_ROOT_DIR="${WIO_APP_DEFAULT_ROOT_DIR}"
        WIO_VERSION="${PROJECT_VERSION}"
)

install(TARGETS wio_app
    RUNTIME DESTINATION bin
)

if(EXISTS "${WIO_SELFHOST_CLI_SOURCE}" AND EXISTS "${WIO_SELFHOST_ARGONAUT_SOURCE}")
    set(WIO_SELFHOST_CLI_DEPENDENCIES
        "${WIO_SELFHOST_CLI_SOURCE}"
        "${WIO_SELFHOST_PROJECT_NEW_SOURCE}"
        "${WIO_SELFHOST_PROJECT_MANIFEST_SOURCE}"
        "${WIO_SELFHOST_PROJECT_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_PROJECT_VM_SOURCE}"
        "${WIO_SELFHOST_COMMON_SOURCE}"
        "${WIO_SELFHOST_GLOBAL_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_ENV_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_BIND_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_PACKAGE_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_FILE_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_DEV_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_PERF_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_MIGRATE_COMMANDS_SOURCE}"
        "${WIO_SELFHOST_ARGONAUT_SOURCE}"
    )
    if(CMAKE_CONFIGURATION_TYPES)
        set(WIO_SELFHOST_CLI_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/wio-selfhost${CMAKE_EXECUTABLE_SUFFIX}"
        )
    else()
        set(WIO_SELFHOST_CLI_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/wio-selfhost${CMAKE_EXECUTABLE_SUFFIX}"
        )
    endif()

    add_custom_command(
        OUTPUT "${WIO_SELFHOST_CLI_OUTPUT}"
        COMMAND
            ${CMAKE_COMMAND} -E env
            "WIO_ROOT=${CMAKE_SOURCE_DIR}"
            $<TARGET_FILE:wio_app>
            "${WIO_SELFHOST_CLI_SOURCE}"
            --target exe
            --output "${WIO_SELFHOST_CLI_OUTPUT}"
            --module-dir "${CMAKE_SOURCE_DIR}/cli"
            --module-dir "${CMAKE_SOURCE_DIR}/cli/vendor"
        DEPENDS
            wio_app
            "${WIO_BACKEND_RUNTIME_LIBRARY}"
            ${WIO_SELFHOST_CLI_DEPENDENCIES}
        COMMENT "Building the Argonaut-Wio self-hosted CLI companion..."
        VERBATIM
    )

    add_custom_target(wio_selfhost_cli ALL
        DEPENDS "${WIO_SELFHOST_CLI_OUTPUT}"
    )
    add_dependencies(wio_selfhost_cli wio_vm_runner)
    set_target_properties(wio_selfhost_cli PROPERTIES FOLDER "app")

    install(
        PROGRAMS "${WIO_SELFHOST_CLI_OUTPUT}"
        TYPE BIN
    )
endif()

include("${CMAKE_SOURCE_DIR}/cmake/WioBenchmarkTool.cmake")

set(WIO_PLAYGROUND_DIR "${CMAKE_SOURCE_DIR}/playground")

set(WIO_PLAYGROUND_MANIFEST "${WIO_PLAYGROUND_DIR}/wio.makewio")

if(EXISTS "${WIO_PLAYGROUND_MANIFEST}")
    file(GLOB_RECURSE WIO_PLAYGROUND_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${WIO_PLAYGROUND_DIR}/*"
    )
    list(FILTER WIO_PLAYGROUND_SOURCES EXCLUDE REGEX "[/\\\\](build|build-[^/\\\\]*|\\.wio-build)[/\\\\]")
    list(FILTER WIO_PLAYGROUND_SOURCES EXCLUDE REGEX "\\.wio\\.cpp$")
    list(FILTER WIO_PLAYGROUND_SOURCES EXCLUDE REGEX "\\.(exe|dll|lib|a|so|dylib|obj|o|pdb)$")
    set_source_files_properties(${WIO_PLAYGROUND_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" WIO_PLAYGROUND_SOURCE_ROOT)
    file(TO_CMAKE_PATH "${WIO_PLAYGROUND_MANIFEST}" WIO_PLAYGROUND_MANIFEST_PATH)

    add_executable(wio_playground
        "${CMAKE_SOURCE_DIR}/tools/wio_playground_launcher.cpp"
        ${WIO_PLAYGROUND_SOURCES}
    )
    add_dependencies(wio_playground wio_app wio_runtime_backend)
    if(TARGET wio_selfhost_cli)
        add_dependencies(wio_playground wio_selfhost_cli)
    endif()
    target_link_libraries(wio_playground PRIVATE wio_runtime_host)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" WIO_PLAYGROUND_BUILD_ROOT)
    if(CMAKE_CONFIGURATION_TYPES)
        set(WIO_PLAYGROUND_MULTI_CONFIG 1)
    else()
        set(WIO_PLAYGROUND_MULTI_CONFIG 0)
    endif()
    target_compile_definitions(wio_playground PRIVATE
        WIO_PLAYGROUND_COMPILER_PATH="$<TARGET_FILE:wio_app>"
        WIO_PLAYGROUND_MANIFEST_PATH="${WIO_PLAYGROUND_MANIFEST_PATH}"
        WIO_PLAYGROUND_SOURCE_ROOT="${WIO_PLAYGROUND_SOURCE_ROOT}"
        WIO_PLAYGROUND_BUILD_CONFIG="$<CONFIG>"
        WIO_PLAYGROUND_BUILD_ROOT="${WIO_PLAYGROUND_BUILD_ROOT}"
        WIO_PLAYGROUND_CMAKE_PATH="${CMAKE_COMMAND}"
        WIO_PLAYGROUND_MULTI_CONFIG=${WIO_PLAYGROUND_MULTI_CONFIG}
    )
    source_group(TREE "${WIO_PLAYGROUND_DIR}" PREFIX "Project" FILES ${WIO_PLAYGROUND_SOURCES})
    source_group("Launcher" FILES "${CMAKE_SOURCE_DIR}/tools/wio_playground_launcher.cpp")
    set_target_properties(wio_playground PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${WIO_PLAYGROUND_DIR}"
    )
endif()

if(BUILD_TESTING)
    include("${CMAKE_SOURCE_DIR}/tests/cmake/WioTestHelpers.cmake")
    include("${CMAKE_SOURCE_DIR}/tests/cmake/LanguageTests.cmake")
    include("${CMAKE_SOURCE_DIR}/tests/cmake/InvalidTests.cmake")
    include("${CMAKE_SOURCE_DIR}/tests/cmake/RuntimeTests.cmake")
    include("${CMAKE_SOURCE_DIR}/tests/cmake/InteropTests.cmake")
    include("${CMAKE_SOURCE_DIR}/tests/cmake/CliTests.cmake")
    wio_finalize_test_labels()
    get_property(wio_registered_tests DIRECTORY PROPERTY TESTS)

    if(wio_registered_tests)
        set_property(
            TEST ${wio_registered_tests}
            APPEND PROPERTY ENVIRONMENT
                "WIO_ROOT=${CMAKE_SOURCE_DIR}"
                "WIO_HOME=${CMAKE_SOURCE_DIR}"
        )
    endif()
    include("${CMAKE_SOURCE_DIR}/tests/cmake/WioTestTargets.cmake")
endif()
