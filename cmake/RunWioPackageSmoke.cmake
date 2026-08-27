if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_BUILD_DIR OR WIO_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_BUILD_DIR was not provided.")
endif()

if(NOT DEFINED WIO_OUTPUT_DIR OR WIO_OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_OUTPUT_DIR was not provided.")
endif()

if(NOT DEFINED WIO_CONFIG OR WIO_CONFIG STREQUAL "")
    set(WIO_CONFIG Debug)
endif()

file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")

file(GLOB existing_entries LIST_DIRECTORIES true "${WIO_OUTPUT_DIR}/wio-*")
foreach(existing_entry IN LISTS existing_entries)
    file(REMOVE_RECURSE "${existing_entry}")
endforeach()

execute_process(
    COMMAND "${WIO_EXE}" package --build-dir "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}" --output-dir "${WIO_OUTPUT_DIR}" --no-zip --no-visual-installer --clean
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE wio_result
    OUTPUT_VARIABLE wio_stdout
    ERROR_VARIABLE wio_stderr
)

set(wio_output "${wio_stdout}${wio_stderr}")

if(NOT wio_result EQUAL 0)
    message(FATAL_ERROR
        "Package smoke failed with code ${wio_result}.\n"
        "Tool output:\n${wio_output}"
    )
endif()

file(GLOB package_roots LIST_DIRECTORIES true "${WIO_OUTPUT_DIR}/wio-*")
set(package_directories)
foreach(package_entry IN LISTS package_roots)
    if(IS_DIRECTORY "${package_entry}")
        list(APPEND package_directories "${package_entry}")
    endif()
endforeach()
set(package_roots ${package_directories})
list(LENGTH package_roots package_root_count)
if(NOT package_root_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one staged package root under '${WIO_OUTPUT_DIR}', but found ${package_root_count}.\n"
        "Tool output:\n${wio_output}"
    )
endif()

list(GET package_roots 0 package_root)

set(required_files
    "${package_root}/WIO_PACKAGE_INFO.json"
    "${package_root}/Install-Wio.ps1"
    "${package_root}/install-wio.sh"
    "${package_root}/QUICKSTART.md"
    "${package_root}/README.md"
    "${package_root}/release-manifest.json"
    "${package_root}/sdk/include/module_api.h"
    "${package_root}/sdk/include/wio_features.h"
    "${package_root}/sdk/include/wio_sdk.h"
    "${package_root}/sdk/include/wio_values.h"
    "${package_root}/sdk/include/wio_version.h"
    "${package_root}/docs/README.md"
    "${package_root}/docs/WIO_ASYNC_EVOLUTION_PLAN.md"
    "${package_root}/docs/WIO_ASYNC_MODEL.md"
    "${package_root}/docs/WIO_LANGUAGE_DRAFT.md"
    "${package_root}/docs/WIO_STD.md"
    "${package_root}/docs/WIO_SDK.md"
    "${package_root}/docs/WIO_SDK_0_13_PARITY_MATRIX.md"
    "${package_root}/docs/WIO_SDK_0_14_PARITY_MATRIX.md"
    "${package_root}/docs/WIO_SDK_EVOLUTION_PLAN.md"
    "${package_root}/docs/WIO_0_14_RELEASE_NOTES.md"
    "${package_root}/docs/WIO_0_15_RELEASE_NOTES.md"
    "${package_root}/docs/WIO_0_16_ACCEPTANCE.md"
    "${package_root}/docs/WIO_0_16_RELEASE_NOTES.md"
    "${package_root}/docs/WIO_V1_RELEASE_PLAN.md"
    "${package_root}/docs/WIO_V1_FREEZE.md"
    "${package_root}/docs/spec/WIO_LANGUAGE_SPEC_0_8.md"
    "${package_root}/docs/spec/WIO_LANGUAGE_SPEC_0_9.md"
    "${package_root}/docs/spec/WIO_LANGUAGE_SPEC_0_10.md"
    "${package_root}/docs/spec/WIO_LANGUAGE_SPEC_0_11.md"
    "${package_root}/docs/spec/WIO_LANGUAGE_SPEC_0_15.md"
    "${package_root}/docs/spec/WIO_LANGUAGE_SPEC_0_16.md"
    "${package_root}/docs/spec/WIO_STD_SPEC_0_11.md"
)

if(WIN32)
    list(APPEND required_files "${package_root}/bin/wio.exe")
else()
    list(APPEND required_files "${package_root}/bin/wio")
endif()

foreach(required_file IN LISTS required_files)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Expected packaged file was not found: ${required_file}\n"
            "Tool output:\n${wio_output}"
        )
    endif()
endforeach()

file(READ "${package_root}/release-manifest.json" release_manifest_text)
string(JSON manifest_abi_version ERROR_VARIABLE manifest_abi_error
    GET "${release_manifest_text}" moduleAbiDescriptorVersion)
if(NOT manifest_abi_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Packaged release manifest has no valid moduleAbiDescriptorVersion: ${manifest_abi_error}")
endif()

file(READ "${package_root}/sdk/include/module_api.h" module_api_header_text)
string(FIND "${module_api_header_text}"
    "WIO_MODULE_API_DESCRIPTOR_VERSION = ${manifest_abi_version}u"
    matching_abi_version_index)
if(matching_abi_version_index EQUAL -1)
    message(FATAL_ERROR
        "Packaged release manifest ABI ${manifest_abi_version} does not match sdk/include/module_api.h.")
endif()

if(NOT WIN32)
    file(READ "${package_root}/WIO_PACKAGE_INFO.json" package_info_text)
    string(FIND "${package_info_text}" "\"bundledBackendRoot\": \"\"" host_backend_index)
    if(host_backend_index EQUAL -1)
        message(FATAL_ERROR "POSIX package metadata incorrectly claims to bundle the host system toolchain.")
    endif()
endif()

file(READ "${package_root}/QUICKSTART.md" quickstart_text)
file(READ "${package_root}/Install-Wio.ps1" powershell_installer_text)
string(FIND "${powershell_installer_text}" "SkipEnvironmentSetup" powershell_skip_environment_index)
if(powershell_skip_environment_index EQUAL -1)
    message(FATAL_ERROR "Packaged Install-Wio.ps1 does not expose the qualification-safe environment opt-out.")
endif()

file(READ "${package_root}/install-wio.sh" shell_installer_text)
string(FIND "${shell_installer_text}" "--install-root" shell_install_root_index)
if(shell_install_root_index EQUAL -1)
    message(FATAL_ERROR "Packaged install-wio.sh does not parse --install-root.")
endif()

if(WIN32)
    get_filename_component(package_name "${package_root}" NAME)
    set(bootstrap_installer "${WIO_OUTPUT_DIR}/${package_name}-installer.ps1")
    if(NOT EXISTS "${bootstrap_installer}")
        message(FATAL_ERROR "Expected standalone bootstrap installer was not found: ${bootstrap_installer}")
    endif()
    file(READ "${bootstrap_installer}" bootstrap_installer_text)
    string(FIND "${bootstrap_installer_text}" "Expand-Archive" bootstrap_expand_index)
    string(FIND "${bootstrap_installer_text}" "${package_name}.zip" bootstrap_zip_index)
    string(FIND "${bootstrap_installer_text}" "installArgs = @{}" bootstrap_named_args_index)
    if(bootstrap_expand_index EQUAL -1 OR bootstrap_zip_index EQUAL -1 OR bootstrap_named_args_index EQUAL -1)
        message(FATAL_ERROR "Bootstrap installer is not self-contained for the adjacent package zip.")
    endif()
endif()

string(FIND "${quickstart_text}" "Install-Wio.ps1" quickstart_install_index)
if(quickstart_install_index EQUAL -1)
    message(FATAL_ERROR
        "Packaged QUICKSTART.md did not contain the expected install guidance.\n"
        "Contents:\n${quickstart_text}"
    )
endif()

string(FIND "${quickstart_text}" "project new" quickstart_project_index)
if(quickstart_project_index EQUAL -1)
    message(FATAL_ERROR
        "Packaged QUICKSTART.md did not contain the expected project creation guidance.\n"
        "Contents:\n${quickstart_text}"
    )
endif()

string(FIND "${quickstart_text}" "env setup" quickstart_env_index)
if(quickstart_env_index EQUAL -1)
    message(FATAL_ERROR
        "Packaged QUICKSTART.md did not contain the expected environment setup guidance.\n"
        "Contents:\n${quickstart_text}"
    )
endif()

message(STATUS "Package smoke succeeded for ${package_root}")
