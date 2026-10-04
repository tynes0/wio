# Cli test registrations. Included in the app directory scope.

add_test(
    NAME wio_cmake_test_catalog
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_CTEST=${CMAKE_CTEST_COMMAND}"
        "-DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_CONFIG=$<CONFIG>"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/test_catalog_test.cmake"
)
wio_test_labels(wio_cmake_test_catalog cli cmake)
set_tests_properties(wio_cmake_test_catalog PROPERTIES TIMEOUT 60)

add_test(
    NAME wio_vm_native_project_cli
    COMMAND ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/vm-native-project-cli-test"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/vm_native_project_cli_test.cmake"
)
wio_test_labels(wio_vm_native_project_cli cli)

set_tests_properties(
    wio_vm_native_project_cli
    PROPERTIES LABELS "vm;native;project;cli" TIMEOUT 180
)

add_test(
    NAME wio_wir_emit_cli
    COMMAND
        ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_CLI_EXECUTABLE=${WIO_SELFHOST_CLI_OUTPUT}"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/wir_emit_cli_input.wio"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-emit-cli-test"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_emit_cli_test.cmake"
)
wio_test_labels(wio_wir_emit_cli cli)

set_tests_properties(
    wio_wir_emit_cli
    PROPERTIES LABELS "wir;cli"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_wir_cpp_backend_cli
    COMMAND
        ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/wir_emit_cli_input.wio"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-backend-cli-test"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_backend_cli_test.cmake"
)
wio_test_labels(wio_wir_cpp_backend_cli cli)

set_tests_properties(
    wio_wir_cpp_backend_cli
    PROPERTIES LABELS "wir;cpp-backend;cli"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_wir_cpp_project_differential
    COMMAND
        ${CMAKE_COMMAND}
        "-DWIO_EXECUTABLE=$<TARGET_FILE:wio_app>"
        "-DWIO_ROOT=${CMAKE_SOURCE_DIR}"
        "-DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/wir-cpp-project-differential"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/wir_cpp_project_differential_test.cmake"
)
wio_test_labels(wio_wir_cpp_project_differential cli)

set_tests_properties(
    wio_wir_cpp_project_differential
    PROPERTIES LABELS "wir;cpp-backend;project;std;native;differential;release-gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    TIMEOUT 300
    RESOURCE_LOCK wio_distribution_package
)

add_test(
    NAME wio_test_file_cli_check
    COMMAND $<TARGET_FILE:wio_app> file check "${CMAKE_SOURCE_DIR}/tests/test1.wio"
)
wio_test_labels(wio_test_file_cli_check cli)

set_tests_properties(
    wio_test_file_cli_check
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_file_cli_tokens
    COMMAND $<TARGET_FILE:wio_app> file tokens "${CMAKE_SOURCE_DIR}/tests/test1.wio"
)
wio_test_labels(wio_test_file_cli_tokens cli)

set_tests_properties(
    wio_test_file_cli_tokens
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_file_cli_run_args_forwarding
    COMMAND $<TARGET_FILE:wio_app> file run "${CMAKE_SOURCE_DIR}/tests/file_run_args_forwarding_run.wio" -- alpha "two words" "--literal" "amp&ersand" "--"
)
wio_test_labels(wio_test_file_cli_run_args_forwarding cli)

set_tests_properties(
    wio_test_file_cli_run_args_forwarding
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_file_cli_run_exit_code
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/file_run_exit_code.wio
        -DWIO_EXPECT_EXIT_CODE=37
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioExpectExitCode.cmake"
)
wio_test_labels(wio_test_file_cli_run_exit_code cli)

set_tests_properties(
    wio_test_file_cli_run_exit_code
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_file_cli_run_uses_hidden_output
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/file_run_args_forwarding_run.wio
        -DWIO_MODE=file-run
        -DWIO_WORK_DIR=${CMAKE_BINARY_DIR}/artifact-policy/file-cli-run
        -DWIO_EXPECT=file-run-args-ok
        "-DWIO_ARGS=--;alpha;two words;--literal;amp&ersand;--"
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioArtifactPolicy.cmake"
)
wio_test_labels(wio_test_file_cli_run_uses_hidden_output cli)

set_tests_properties(
    wio_test_file_cli_run_uses_hidden_output
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_env_cli_print
    COMMAND $<TARGET_FILE:wio_app> env print --wio-root "${CMAKE_SOURCE_DIR}" --shell powershell --add-path
)
wio_test_labels(wio_test_env_cli_print cli)

set_tests_properties(
    wio_test_env_cli_print
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_env_cli_setup_preview
    COMMAND $<TARGET_FILE:wio_app> env setup --wio-root "${CMAKE_SOURCE_DIR}" --no-prompt --add-path
)
wio_test_labels(wio_test_env_cli_setup_preview cli)

set_tests_properties(
    wio_test_env_cli_setup_preview
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_env_cli_status
    COMMAND $<TARGET_FILE:wio_app> env status --wio-root "${CMAKE_SOURCE_DIR}"
)
wio_test_labels(wio_test_env_cli_status cli)

set_tests_properties(
    wio_test_env_cli_status
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_env_cli_remove_preview
    COMMAND $<TARGET_FILE:wio_app> env remove --wio-root "${CMAKE_SOURCE_DIR}" --no-prompt --remove-path
)
wio_test_labels(wio_test_env_cli_remove_preview cli)

set_tests_properties(
    wio_test_env_cli_remove_preview
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_env_cli_doctor
    COMMAND $<TARGET_FILE:wio_app> env doctor --wio-root "${CMAKE_SOURCE_DIR}"
)
wio_test_labels(wio_test_env_cli_doctor cli)

set_tests_properties(
    wio_test_env_cli_doctor
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_env_cli_backend_smoke
    COMMAND $<TARGET_FILE:wio_app> env doctor --wio-root "${CMAKE_SOURCE_DIR}" --backend-smoke
)
wio_test_labels(wio_test_env_cli_backend_smoke cli)

set_tests_properties(
    wio_test_env_cli_backend_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 120
)

add_test(
    NAME wio_test_cli_help_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioCliHelpSmoke.cmake"
)
wio_test_labels(wio_test_cli_help_smoke cli)

set_tests_properties(
    wio_test_cli_help_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_attribute_migration_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SCRATCH_ROOT=${CMAKE_BINARY_DIR}/attribute-migration-smoke
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioAttributeMigrationSmoke.cmake"
)
wio_test_labels(wio_test_attribute_migration_smoke cli freeze)

set_tests_properties(
    wio_test_attribute_migration_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_self_hosted_cli_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/entry_default_void_run.wio
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioSelfHostedCliSmoke.cmake"
)
wio_test_labels(wio_test_self_hosted_cli_smoke cli)

add_test(
    NAME wio_test_self_hosted_dev_cli_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DWIO_CONFIG=$<CONFIG>
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioSelfHostedDevCliSmoke.cmake"
)
wio_test_labels(wio_test_self_hosted_dev_cli_smoke cli)

set_tests_properties(
    wio_test_self_hosted_dev_cli_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 300
)

set_tests_properties(
    wio_test_self_hosted_cli_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_package_cli_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/package-smoke
        -DWIO_CONFIG=$<CONFIG>
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioPackageSmoke.cmake"
)
wio_test_labels(wio_test_package_cli_smoke cli)

set_tests_properties(
    wio_test_package_cli_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESOURCE_LOCK wio_distribution_package
        RUN_SERIAL TRUE
)

add_test(
    NAME wio_test_perf_cli_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/perf-smoke-test
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioPerfSmoke.cmake"
)
wio_test_labels(wio_test_perf_cli_smoke cli)

set_tests_properties(
    wio_test_perf_cli_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        RESOURCE_LOCK wio_distribution_package
)

add_test(
    NAME wio_test_project_cli_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/project-smoke
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioProjectSmoke.cmake"
)
wio_test_labels(wio_test_project_cli_smoke cli)

set_tests_properties(
    wio_test_project_cli_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_project_emit_cpp_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/project-emit-cpp-smoke
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioProjectEmitCppSmoke.cmake"
)
wio_test_labels(wio_test_project_emit_cpp_smoke cli)

set_tests_properties(
    wio_test_project_emit_cpp_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_project_test_package_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/project-test-package-smoke
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioProjectTestPackageSmoke.cmake"
)
wio_test_labels(wio_test_project_test_package_smoke cli)

set_tests_properties(
    wio_test_project_test_package_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_hybrid_project_cli_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/hybrid-project-smoke
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioHybridProjectSmoke.cmake"
)
wio_test_labels(wio_test_hybrid_project_cli_smoke cli)

set_tests_properties(
    wio_test_hybrid_project_cli_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_example_binding_import
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DEXAMPLE_ROOT=${CMAKE_SOURCE_DIR}/examples/binding_import
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/binding-example-smoke
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioBindingExample.cmake"
)
wio_test_labels(wio_test_example_binding_import cli)

set_tests_properties(
    wio_test_example_binding_import
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_packaged_file_run_uses_user_cache
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DWIO_OUTPUT_DIR=${CMAKE_BINARY_DIR}/package-file-run-smoke
        -DWIO_CONFIG=$<CONFIG>
        -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/file_run_args_forwarding_run.wio
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioPackagedFileRunSmoke.cmake"
)
wio_test_labels(wio_test_packaged_file_run_uses_user_cache cli)

set(WIO_QUALIFICATION_BACKEND_ROOT "")

get_filename_component(wio_qualification_compiler_name "${CMAKE_CXX_COMPILER}" NAME)

if(WIN32 AND wio_qualification_compiler_name MATCHES "^g\\+\\+(\\.exe)?$")
    get_filename_component(wio_qualification_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(WIO_QUALIFICATION_BACKEND_ROOT "${wio_qualification_compiler_bin}" DIRECTORY)
endif()

add_test(
    NAME wio_test_installed_package_qualification
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DWIO_SCRATCH_DIR=${CMAKE_SOURCE_DIR}/.wio-qualification
        -DWIO_CONFIG=$<CONFIG>
        -DWIO_PORTABLE_BACKEND_ROOT=${WIO_QUALIFICATION_BACKEND_ROOT}
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioInstalledPackageQualification.cmake"
)
wio_test_labels(wio_test_installed_package_qualification cli)

set_tests_properties(
    wio_test_installed_package_qualification
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
        RESOURCE_LOCK wio_distribution_package
        RUN_SERIAL TRUE
)

set_tests_properties(
    wio_test_packaged_file_run_uses_user_cache
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESOURCE_LOCK wio_distribution_package
        RUN_SERIAL TRUE
)

add_test(
    NAME wio_test_binding_import_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_OUTPUT=${CMAKE_BINARY_DIR}/generated/binding_import_smoke.wio
        "-DWIO_TOOL_ARGS=bind;import;--header;${CMAKE_SOURCE_DIR}/tests/native/binding_import_smoke.h;--realm;binding_import_smoke;--output;${CMAKE_BINARY_DIR}/generated/binding_import_smoke.wio"
        "-DWIO_COMPILER_ARGS=--dry-run;--target;static;--include-dir;${CMAKE_SOURCE_DIR}/tests/native"
        "-DWIO_EXPECT_1=const DefaultPort: i32 = 8080;"
        "-DWIO_EXPECT_2=component color3 with native, cpp::name(binding_import_smoke::color3) {"
        "-DWIO_EXPECT_3=rgb: [u8; 3];"
        "-DWIO_EXPECT_4=flagset feature_mask with native, abi::type(u32), cpp::name(binding_import_smoke::feature_mask) {"
        "-DWIO_EXPECT_5=using cpp::header(\"binding_import_smoke.h\");"
        "-DWIO_EXPECT_6=with native, cpp::name(binding_import_smoke::Scale)"
        -P "${CMAKE_SOURCE_DIR}/cmake/RunBindingGenerationSmoke.cmake"
)
wio_test_labels(wio_test_binding_import_smoke cli traceability)

set_tests_properties(
    wio_test_binding_import_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_binding_manifest_smoke
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_OUTPUT=${CMAKE_BINARY_DIR}/generated/binding_manifest_smoke.wio
        "-DWIO_TOOL_ARGS=bind;new;--manifest;${CMAKE_SOURCE_DIR}/tests/native/binding_manifest_smoke.json;--output;${CMAKE_BINARY_DIR}/generated/binding_manifest_smoke.wio"
        "-DWIO_COMPILER_ARGS=--dry-run;--target;static;--include-dir;${CMAKE_SOURCE_DIR}/tests/native"
        "-DWIO_EXPECT_1=const DefaultPort: i32 = 8080;"
        "-DWIO_EXPECT_2=component color3 with native, cpp::name(binding_import_smoke::color3) {"
        "-DWIO_EXPECT_3=flagset feature_mask with abi::type(u32) {"
        "-DWIO_EXPECT_4=fn Scale(value: ref color3, factor: f32) with native, cpp::name(binding_import_smoke::Scale);"
        "-DWIO_EXPECT_5=using cpp::header(\"binding_import_smoke.h\");"
        "-DWIO_EXPECT_6=with native, cpp::name(binding_import_smoke::Scale)"
        -P "${CMAKE_SOURCE_DIR}/cmake/RunBindingGenerationSmoke.cmake"
)
wio_test_labels(wio_test_binding_manifest_smoke cli traceability)

set_tests_properties(
    wio_test_binding_manifest_smoke
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME wio_test_cli_path_dispatch_console_output
    COMMAND
        ${CMAKE_COMMAND}
        -DWIO_EXE=$<TARGET_FILE:wio_app>
        -DWIO_SOURCE=${CMAKE_SOURCE_DIR}/tests/entry_default_void_run.wio
        -DWIO_SCRATCH_DIR=${CMAKE_BINARY_DIR}/cli_path_dispatch_console_output
        -P "${CMAKE_SOURCE_DIR}/cmake/RunWioCliPathDispatchConsoleSmoke.cmake"
)
wio_test_labels(wio_test_cli_path_dispatch_console_output cli)

set_tests_properties(
    wio_test_cli_path_dispatch_console_output
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_cli_migrate_application_rewrite_run
    "${CMAKE_SOURCE_DIR}/tests/cli_migrate_application_rewrite_run.wio"
    "cli-migrate-application-rewrite-ok"
    --run --module-dir "${CMAKE_SOURCE_DIR}/cli" --module-dir "${CMAKE_SOURCE_DIR}/cli/vendor"
)
wio_test_labels(wio_test_cli_migrate_application_rewrite_run cli freeze)

set_tests_properties(
    wio_test_cli_migrate_application_rewrite_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)
