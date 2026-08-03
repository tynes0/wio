if(NOT DEFINED WIO_EXE OR WIO_EXE STREQUAL "")
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()
if(NOT DEFINED WIO_BUILD_DIR OR WIO_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_BUILD_DIR was not provided.")
endif()
if(NOT DEFINED WIO_SCRATCH_DIR OR WIO_SCRATCH_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_SCRATCH_DIR was not provided.")
endif()
if(NOT DEFINED WIO_CONFIG OR WIO_CONFIG STREQUAL "")
    set(WIO_CONFIG Debug)
endif()

cmake_path(ABSOLUTE_PATH CMAKE_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE repository_root)
cmake_path(ABSOLUTE_PATH WIO_SCRATCH_DIR NORMALIZE OUTPUT_VARIABLE scratch_root)
string(TOLOWER "${repository_root}" repository_root_lower)

set(package_output_dir "${scratch_root}/package")
set(install_root "${scratch_root}/installed")
set(external_root "${scratch_root}/external")
set(fake_home "${scratch_root}/home")
if(WIN32)
    set(installed_wio "${install_root}/bin/wio.exe")
else()
    set(installed_wio "${install_root}/bin/wio")
endif()

set(reuse_install false)
if(DEFINED WIO_REUSE_INSTALL AND WIO_REUSE_INSTALL AND EXISTS "${installed_wio}")
    set(reuse_install true)
endif()

if(NOT reuse_install)
    file(REMOVE_RECURSE "${scratch_root}")
    file(MAKE_DIRECTORY "${package_output_dir}" "${fake_home}")

    set(package_command
        "${WIO_EXE}" package
        --build-dir "${WIO_BUILD_DIR}"
        --config "${WIO_CONFIG}"
        --output-dir "${package_output_dir}"
        --no-zip
        --no-visual-installer
        --clean
    )
    if(DEFINED WIO_PORTABLE_BACKEND_ROOT AND NOT WIO_PORTABLE_BACKEND_ROOT STREQUAL "")
        set(package_command
            "${CMAKE_COMMAND}" -E env
            "WIO_PORTABLE_BACKEND_ROOT=${WIO_PORTABLE_BACKEND_ROOT}"
            ${package_command}
        )
    endif()

    execute_process(
        COMMAND ${package_command}
        WORKING_DIRECTORY "${repository_root}"
        RESULT_VARIABLE package_result
        OUTPUT_VARIABLE package_stdout
        ERROR_VARIABLE package_stderr
    )
    if(NOT package_result EQUAL 0)
        message(FATAL_ERROR
            "Installed-package qualification could not stage the release package.\n"
            "${package_stdout}${package_stderr}"
        )
    endif()

    file(GLOB package_candidates LIST_DIRECTORIES true "${package_output_dir}/wio-*")
    set(package_roots)
    foreach(candidate IN LISTS package_candidates)
        if(IS_DIRECTORY "${candidate}")
            list(APPEND package_roots "${candidate}")
        endif()
    endforeach()
    list(LENGTH package_roots package_count)
    if(NOT package_count EQUAL 1)
        message(FATAL_ERROR "Expected exactly one package root, found ${package_count}.")
    endif()
    list(GET package_roots 0 package_root)

    if(WIN32)
        execute_process(
            COMMAND
                powershell -NoProfile -ExecutionPolicy Bypass
                -File "${package_root}/Install-Wio.ps1"
                -InstallRoot "${install_root}"
                -NoPrompt -Force -SkipEnvironmentSetup
            WORKING_DIRECTORY "${package_root}"
            RESULT_VARIABLE install_result
            OUTPUT_VARIABLE install_stdout
            ERROR_VARIABLE install_stderr
        )
    else()
        execute_process(
            COMMAND
                "${CMAKE_COMMAND}" -E env "HOME=${fake_home}"
                sh "${package_root}/install-wio.sh"
                --install-root "${install_root}"
                --skip-path
            WORKING_DIRECTORY "${package_root}"
            RESULT_VARIABLE install_result
            OUTPUT_VARIABLE install_stdout
            ERROR_VARIABLE install_stderr
        )
    endif()

    if(NOT install_result EQUAL 0)
        message(FATAL_ERROR
            "Release package installation failed.\n${install_stdout}${install_stderr}"
        )
    endif()
    if("${install_root}" STREQUAL "${package_root}")
        message(FATAL_ERROR "Qualification must use an installed copy, not the staged package root.")
    endif()
endif()

if(NOT EXISTS "${installed_wio}")
    message(FATAL_ERROR "Installed wio executable was not found: ${installed_wio}")
endif()

file(REMOVE_RECURSE "${external_root}")
file(MAKE_DIRECTORY "${external_root}" "${fake_home}")

function(run_installed label working_directory)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env
            "WIO_ROOT=${install_root}"
            "HOME=${fake_home}"
            "USERPROFILE=${fake_home}"
            "${installed_wio}" ${ARGN}
        WORKING_DIRECTORY "${working_directory}"
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_stdout
        ERROR_VARIABLE command_stderr
    )
    set(command_output "${command_stdout}${command_stderr}")

    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with code ${command_result}.\n${command_output}"
        )
    endif()

    string(REPLACE "\\" "/" command_output_normalized "${command_output}")
    string(TOLOWER "${command_output_normalized}" command_output_lower)
    foreach(repository_subtree IN ITEMS std runtime sdk compiler app)
        set(forbidden_path "${repository_root_lower}/${repository_subtree}/")
        string(FIND "${command_output_lower}" "${forbidden_path}" repository_path_index)
        if(NOT repository_path_index EQUAL -1)
            message(FATAL_ERROR
                "${label} leaked repository files into installed-toolchain resolution.\n"
                "Forbidden source subtree: ${repository_root}/${repository_subtree}\n"
                "${command_output}"
            )
        endif()
    endforeach()

    set(LAST_INSTALLED_OUTPUT "${command_output}" PARENT_SCOPE)
endfunction()

run_installed(
    "Installed CLI version probe"
    "${external_root}"
    --version
)
run_installed(
    "Installed self-hosted CLI probe"
    "${external_root}"
    --self-hosted-info
)
if(NOT LAST_INSTALLED_OUTPUT MATCHES "Argonaut-Wio bootstrap")
    message(FATAL_ERROR
        "Installed package did not expose the self-hosted CLI companion.\n"
        "${LAST_INSTALLED_OUTPUT}"
    )
endif()

file(GLOB public_std_files LIST_DIRECTORIES false "${repository_root}/std/*.wio")
list(SORT public_std_files)
set(combined_imports "")
set(module_index 0)
foreach(std_file IN LISTS public_std_files)
    get_filename_component(module_name "${std_file}" NAME_WE)
    math(EXPR module_index "${module_index} + 1")
    string(CONCAT import_source
        "use std::${module_name};\n\n"
        "fn Entry() -> i32 {\n"
        "    return 0;\n"
        "}\n"
    )
    set(import_file "${external_root}/import-${module_name}.wio")
    file(WRITE "${import_file}" "${import_source}")
    run_installed(
        "Independent std::${module_name} import"
        "${external_root}"
        file check "${import_file}"
    )
    string(APPEND combined_imports "use std::${module_name};\n")
endforeach()

set(combined_file "${external_root}/all-std-modules.wio")
file(WRITE "${combined_file}"
    "${combined_imports}\n"
    "fn Entry() -> i32 {\n"
    "    return 0;\n"
    "}\n"
)
run_installed(
    "Combined public std import"
    "${external_root}"
    file check "${combined_file}"
)

set(path_fs_contract_source "${repository_root}/tests/std_path_fs_contract_run.wio")
set(path_fs_contract_file "${external_root}/std-path-fs-contract.wio")
file(COPY_FILE "${path_fs_contract_source}" "${path_fs_contract_file}")
run_installed(
    "Installed std::path/std::fs contract"
    "${external_root}"
    file run "${path_fs_contract_file}"
)
string(FIND "${LAST_INSTALLED_OUTPUT}" "std-path-fs-contract-ok" path_fs_marker_index)
if(path_fs_marker_index EQUAL -1)
    message(FATAL_ERROR
        "Installed std::path/std::fs contract did not print its success marker.\n"
        "${LAST_INSTALLED_OUTPUT}"
    )
endif()

run_installed(
    "External project creation"
    "${external_root}"
    project new QualifiedApp --output-dir "${external_root}" --template wio-app
)
set(project_root "${external_root}/QualifiedApp")
file(WRITE "${project_root}/wio/module.wio"
    "${combined_imports}\n"
    "fn Entry(args: string[]) -> i32 {\n"
    "    std::console::Print(\"installed-project-ok\");\n"
    "    return 0;\n"
    "}\n"
)
run_installed(
    "External project build"
    "${project_root}"
    project build
)
run_installed(
    "External project run"
    "${project_root}"
    project run --no-build
)
string(FIND "${LAST_INSTALLED_OUTPUT}" "installed-project-ok" project_marker_index)
if(project_marker_index EQUAL -1)
    message(FATAL_ERROR
        "Installed external project did not print its success marker.\n${LAST_INSTALLED_OUTPUT}"
    )
endif()

file(MAKE_DIRECTORY "${project_root}/tests")
set(external_test "${project_root}/tests/installed-toolchain-test.wio")
file(WRITE "${external_test}"
    "use std::assert as qualification_assert;\n"
    "use std::console as qualification_console;\n\n"
    "fn Entry() -> i32 {\n"
    "    qualification_assert::ExpectEqual(6 * 7, 42, \"installed test arithmetic\");\n"
    "    qualification_console::Print(\"installed-test-ok\");\n"
    "    return 0;\n"
    "}\n"
)
run_installed(
    "External project test"
    "${project_root}"
    project test
)
string(FIND "${LAST_INSTALLED_OUTPUT}" "installed-test-ok" test_marker_index)
if(test_marker_index EQUAL -1)
    message(FATAL_ERROR
        "Installed external project test did not print its success marker.\n${LAST_INSTALLED_OUTPUT}"
    )
endif()

set(project_package_parent "${project_root}/packages")
run_installed(
    "External project package"
    "${project_root}"
    project package --output-dir "${project_package_parent}" --clean --no-build
)
set(project_package_root "${project_package_parent}/qualifiedapp")
if(NOT EXISTS "${project_package_root}/wio-package.json")
    message(FATAL_ERROR
        "External project package metadata was not produced: "
        "${project_package_root}/wio-package.json"
    )
endif()
if(WIN32)
    set(packaged_project_executable "${project_package_root}/bin/qualifiedapp.exe")
else()
    set(packaged_project_executable "${project_package_root}/bin/qualifiedapp")
endif()
if(NOT EXISTS "${packaged_project_executable}")
    message(FATAL_ERROR
        "External project package executable was not produced: "
        "${packaged_project_executable}"
    )
endif()
if(WIN32)
    set(packaged_project_launcher "${project_package_root}/run.cmd")
    execute_process(
        COMMAND cmd /c "${packaged_project_launcher}"
        WORKING_DIRECTORY "${project_package_root}"
        RESULT_VARIABLE packaged_run_result
        OUTPUT_VARIABLE packaged_run_stdout
        ERROR_VARIABLE packaged_run_stderr
    )
else()
    set(packaged_project_launcher "${project_package_root}/run.sh")
    execute_process(
        COMMAND "${packaged_project_launcher}"
        WORKING_DIRECTORY "${project_package_root}"
        RESULT_VARIABLE packaged_run_result
        OUTPUT_VARIABLE packaged_run_stdout
        ERROR_VARIABLE packaged_run_stderr
    )
endif()
set(packaged_run_output "${packaged_run_stdout}${packaged_run_stderr}")
if(NOT packaged_run_result EQUAL 0 OR
   NOT packaged_run_output MATCHES "installed-project-ok")
    message(FATAL_ERROR
        "Packaged external project launcher failed.\n${packaged_run_output}"
    )
endif()

run_installed(
    "External native project creation"
    "${external_root}"
    project new QualifiedNative --output-dir "${external_root}" --template wio-native-app
)
set(native_project_root "${external_root}/QualifiedNative")
run_installed(
    "External native project build"
    "${native_project_root}"
    project build
)
run_installed(
    "External native project run"
    "${native_project_root}"
    project run --no-build
)
string(FIND "${LAST_INSTALLED_OUTPUT}" "42" native_marker_index)
if(native_marker_index EQUAL -1)
    message(FATAL_ERROR
        "Installed native project did not print the expected result.\n${LAST_INSTALLED_OUTPUT}"
    )
endif()

if(EXISTS "${install_root}/.wio-build")
    message(FATAL_ERROR "Installed package root was mutated with a .wio-build directory.")
endif()

message(STATUS
    "Installed-package qualification succeeded using '${installed_wio}'. "
    "Validated ${module_index} public std modules independently and together, "
    "an external project build/run/test/package lifecycle, and native interop."
)
