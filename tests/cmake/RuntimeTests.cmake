# Runtime test registrations. Included in the app directory scope.

add_test(NAME wio_n04_runtime_profile
    COMMAND "${CMAKE_COMMAND}" "-DWIO_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/backend_runtime_profile_test.cmake")
wio_test_labels(wio_n04_runtime_profile runtime configuration optimization)
set_tests_properties(wio_n04_runtime_profile PROPERTIES TIMEOUT 30)

find_package(Python3 COMPONENTS Interpreter QUIET)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    set(WIO_FUZZ_WORK_DIR "${CMAKE_BINARY_DIR}/fuzz-smoke")
    add_test(
        NAME wio_fuzz_smoke
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/fuzz/wio_fuzz.py"
            --wio "$<TARGET_FILE:wio_app>"
            --backend "${WIO_BACKEND_CXX_COMPILER}"
            --root "${CMAKE_SOURCE_DIR}"
            --corpus "${CMAKE_SOURCE_DIR}/tests/fuzz/corpus"
            --work "${WIO_FUZZ_WORK_DIR}"
            --iterations 24
            --seed 5712207
            --timeout 5
            --max-diagnostics 64
    )
    set_tests_properties(wio_fuzz_smoke PROPERTIES
        TIMEOUT 180
        LABELS "fuzz;p0"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    wio_test_labels(wio_fuzz_smoke runtime)

    add_custom_target(
        wio_fuzz
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/fuzz/wio_fuzz.py"
            --wio "$<TARGET_FILE:wio_app>"
            --backend "${WIO_BACKEND_CXX_COMPILER}"
            --root "${CMAKE_SOURCE_DIR}"
            --corpus "${CMAKE_SOURCE_DIR}/tests/fuzz/corpus"
            --work "${WIO_FUZZ_WORK_DIR}"
            --iterations 250
            --seed 5712207
            --timeout 5
            --max-diagnostics 64
        DEPENDS wio_app
        USES_TERMINAL
        VERBATIM
    )
endif()

add_wio_output_test(
    wio_test_std_process_run_smoke_run
    "${CMAKE_SOURCE_DIR}/tests/std_process_run_smoke.wio"
    "std-process-run-ok"
    --run
)
wio_test_labels(wio_test_std_process_run_smoke_run runtime)

add_wio_output_test(
    wio_test_std_process_executable_path_run
    "${CMAKE_SOURCE_DIR}/tests/std_process_executable_path_run.wio"
    "Std process executable path: true/true"
    --run
)
wio_test_labels(wio_test_std_process_executable_path_run runtime)

add_wio_output_test(
    wio_test_std_foundation_extensions_run
    "${CMAKE_SOURCE_DIR}/tests/std_foundation_extensions_run.wio"
    "STD foundation extensions: ok"
    --run
)
wio_test_labels(wio_test_std_foundation_extensions_run runtime)

add_wio_output_test(
    wio_test_std_utility_stream_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_utility_stream_wave_run.wio"
    "STD utility stream wave: ok"
    --run
)
wio_test_labels(wio_test_std_utility_stream_wave_run runtime)

add_wio_output_test(
    wio_test_std_json_packed_semver_run
    "${CMAKE_SOURCE_DIR}/tests/std_json_packed_semver_run.wio"
    "JSON \\+ packed semver: ok"
    --run
)
wio_test_labels(wio_test_std_json_packed_semver_run runtime)

add_wio_output_test(
    wio_test_std_option_result_consistency_run
    "${CMAKE_SOURCE_DIR}/tests/std_option_result_consistency_run.wio"
    "STD option/result consistency: ok"
    --run
)
wio_test_labels(wio_test_std_option_result_consistency_run runtime)

add_wio_output_test(
    wio_test_std_resource_owned_run
    "${CMAKE_SOURCE_DIR}/tests/std_resource_owned_run.wio"
    "std-resource-owned-ok"
    --run
)
wio_test_labels(wio_test_std_resource_owned_run runtime)

add_wio_output_test(
    wio_test_std_option_adoption_run
    "${CMAKE_SOURCE_DIR}/tests/std_option_adoption_run.wio"
    "std-option-adoption-ok"
    --run
)
wio_test_labels(wio_test_std_option_adoption_run runtime)

add_wio_output_test(
    wio_test_std_meta_pack_algorithms_run
    "${CMAKE_SOURCE_DIR}/tests/std_meta_pack_algorithms_run.wio"
    "Std meta pack algorithms: ok"
    --run
)
wio_test_labels(wio_test_std_meta_pack_algorithms_run runtime)

add_wio_output_test(
    wio_test_std_meta_second_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_meta_second_wave_run.wio"
    "Std meta second wave: ok"
    --run
)
wio_test_labels(wio_test_std_meta_second_wave_run runtime traceability)

add_wio_output_test(
    wio_test_std_meta_wave3_run
    "${CMAKE_SOURCE_DIR}/tests/std_meta_wave3_run.wio"
    "Std meta wave 3: ok"
    --run
)
wio_test_labels(wio_test_std_meta_wave3_run runtime traceability)

add_wio_output_test(
    wio_test_std_console_direct_run
    "${CMAKE_SOURCE_DIR}/tests/std_console_direct.wio"
    "Std direct: hello\nStd direct value: 7"
    --run
)
wio_test_labels(wio_test_std_console_direct_run runtime)

add_wio_output_test(
    wio_test_std_math_run
    "${CMAKE_SOURCE_DIR}/tests/std_math_run.wio"
    "Std math: 3/10/4/2.5"
    --run
)
wio_test_labels(wio_test_std_math_run runtime)

add_wio_output_test(
    wio_test_std_math_extended_run
    "${CMAKE_SOURCE_DIR}/tests/std_math_extended_run.wio"
    "Std math extended: 49/-1/3/true"
    --run
)
wio_test_labels(wio_test_std_math_extended_run runtime)

add_wio_output_test(
    wio_test_std_collections_alias_run
    "${CMAKE_SOURCE_DIR}/tests/std_collections_alias.wio"
    "Std collections: 22"
    --run
)
wio_test_labels(wio_test_std_collections_alias_run runtime)

add_wio_output_test(
    wio_test_std_algorithms_callbacks_run
    "${CMAKE_SOURCE_DIR}/tests/std_algorithms_callbacks_run.wio"
    "Callback std: true/true/true/7"
    --run
)
wio_test_labels(wio_test_std_algorithms_callbacks_run runtime)

add_wio_output_test(
    wio_test_std_collections_helpers_run
    "${CMAKE_SOURCE_DIR}/tests/std_collections_helpers_run.wio"
    "Std collections helpers: true/false/3/-1/true"
    --run
)
wio_test_labels(wio_test_std_collections_helpers_run runtime)

add_wio_output_test(
    wio_test_std_collections_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_collections_surface_run.wio"
    "Std collections surface: true/3/9/7/5/1/4/a:b:c/true/3/true/2/3/3"
    --run
)
wio_test_labels(wio_test_std_collections_surface_run runtime)

add_wio_output_test(
    wio_test_std_strings_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_strings_surface_run.wio"
    "Std strings surface: 9/true/4/Arena/Arena/cba/007/!DLROWOIW/hi/true"
    --run
)
wio_test_labels(wio_test_std_strings_surface_run runtime)

add_wio_output_test(
    wio_test_std_convert_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_convert_surface_run.wio"
    "Std convert: -2147483648/42/42/3\\.5/true/true/65535/true/true/true/42/false/wio/BEEF/1010/true/true/true"
    --run
)
wio_test_labels(wio_test_std_convert_surface_run runtime traceability)

add_wio_output_test(
    wio_test_std_text_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_text_surface_run.wio"
    "Std text: alpha\\|beta\\|gamma/alpha beta gamma/true/true/true/-1/2/convert/report\\.txt/a/b/c\\.txt/Wio/abc\\.\\.\\./true/true"
    --run
)
wio_test_labels(wio_test_std_text_surface_run runtime traceability)

add_wio_output_test(
    wio_test_std_path_fs_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_path_fs_surface_run.wio"
    "Std path/fs surface: true/true/true/3/gamma/true/true/logs/final\\.log/final\\.log/2/logs/true/alpha\\|beta\\|gamma/true"
    --run
)
wio_test_labels(wio_test_std_path_fs_surface_run runtime)

add_wio_output_test(
    wio_test_std_fs_recursive_run
    "${CMAKE_SOURCE_DIR}/tests/std_fs_recursive_run.wio"
    "Std fs recursive: true/1/true/true/true/true"
    --run
)
wio_test_labels(wio_test_std_fs_recursive_run runtime)

add_wio_output_test(
    wio_test_std_fs_executable_run
    "${CMAKE_SOURCE_DIR}/tests/std_fs_executable_run.wio"
    "Std fs executable: true/true/true/true"
    --run
)
wio_test_labels(wio_test_std_fs_executable_run runtime)

add_wio_output_test(
    wio_test_std_environment_statistics_run
    "${CMAKE_SOURCE_DIR}/tests/std_environment_statistics_run.wio"
    "Std environment/statistics: true/true/true/true/true/true/true"
    --run
)
wio_test_labels(wio_test_std_environment_statistics_run runtime)

add_wio_output_test(
    wio_test_std_platform_run
    "${CMAKE_SOURCE_DIR}/tests/std_platform_run.wio"
    "Std platform: true/true/true/true/true"
    --run
)
wio_test_labels(wio_test_std_platform_run runtime)

add_wio_output_test(
    wio_test_std_console_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_console_surface_run.wio"
    "Std console surface: 1,2,3 xxx a/b"
    --run
)
wio_test_labels(wio_test_std_console_surface_run runtime)

add_wio_output_test(
    wio_test_std_vector_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_vector_surface_run.wio"
    "std-vector-surface-ok"
    --run
)
wio_test_labels(wio_test_std_vector_surface_run runtime)

add_wio_output_test(
    wio_test_std_enum_flagset_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_enum_flagset_surface_run.wio"
    "enum-flagset-surface-ok"
    --run
)
wio_test_labels(wio_test_std_enum_flagset_surface_run runtime traceability)

add_wio_output_test(
    wio_test_std_io_surface_run
    "${CMAKE_SOURCE_DIR}/tests/std_io_surface_run.wio"
    "Std io surface: 2/3/3/8/AB/CD/read\\|share_read\\|sequential/none"
    --run
)
wio_test_labels(wio_test_std_io_surface_run runtime)

add_wio_output_test(
    wio_test_std_algorithms_run
    "${CMAKE_SOURCE_DIR}/tests/std_algorithms_run.wio"
    "Std algorithms: 2/true/false/false/true/false"
    --run
)
wio_test_labels(wio_test_std_algorithms_run runtime)

add_wio_output_test(
    wio_test_std_algorithms_extended_run
    "${CMAKE_SOURCE_DIR}/tests/std_algorithms_extended_run.wio"
    "Std algorithms extended: true/2/-1/true"
    --run
)
wio_test_labels(wio_test_std_algorithms_extended_run runtime)

add_wio_output_test(
    wio_test_std_algorithms_utility_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_algorithms_utility_wave_run.wio"
    "Std algorithms utility: 4/4/3/3/4/3/2/2/4/15/3/1/true/3/5/true"
    --run
)
wio_test_labels(wio_test_std_algorithms_utility_wave_run runtime traceability)

add_wio_output_test(
    wio_test_std_algorithms_callback_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_algorithms_callback_wave_run.wio"
    "Callback wave: 3/2/6/2/12/3/21/2/2/2"
    --run
)
wio_test_labels(wio_test_std_algorithms_callback_wave_run runtime)

add_wio_output_test(
    wio_test_std_assert_run
    "${CMAKE_SOURCE_DIR}/tests/std_assert_run.wio"
    "Std assert: ok"
    --run
)
wio_test_labels(wio_test_std_assert_run runtime)

add_wio_output_test(
    wio_test_std_hash_random_time_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_hash_random_time_wave_run.wio"
    "std-hash-random-time-wave-ok"
    --run
)
wio_test_labels(wio_test_std_hash_random_time_wave_run runtime)

add_wio_output_test(
    wio_test_std_containers_buffer_span_sort_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_containers_buffer_span_sort_wave_run.wio"
    "std-containers-buffer-span-sort-wave-ok"
    --run
)
wio_test_labels(wio_test_std_containers_buffer_span_sort_wave_run runtime)

add_wio_output_test(
    wio_test_std_container_contracts_run
    "${CMAKE_SOURCE_DIR}/tests/std_container_contracts_run.wio"
    "std-container-contracts-ok"
    --run
)
wio_test_labels(wio_test_std_container_contracts_run runtime)

add_wio_output_test(
    wio_test_std_numeric_integer_matrix_run
    "${CMAKE_SOURCE_DIR}/tests/std_numeric_integer_matrix_run.wio"
    "std-numeric-integer-matrix-ok"
    --run
)
wio_test_labels(wio_test_std_numeric_integer_matrix_run runtime)

add_wio_output_test(
    wio_test_std_path_fs_contract_run
    "${CMAKE_SOURCE_DIR}/tests/std_path_fs_contract_run.wio"
    "std-path-fs-contract-ok"
    --run
)
wio_test_labels(wio_test_std_path_fs_contract_run runtime)

add_wio_output_test(
    wio_test_std_regex_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_regex_wave_run.wio"
    "std-regex-wave-ok"
    --run
)
wio_test_labels(wio_test_std_regex_wave_run runtime)

add_wio_output_test(
    wio_test_std_traits_reflection_wave_run
    "${CMAKE_SOURCE_DIR}/tests/std_traits_reflection_wave_run.wio"
    "std-traits-reflection-wave-ok"
    --run
)
wio_test_labels(wio_test_std_traits_reflection_wave_run runtime)

add_wio_output_test(
    wio_test_std_traits_text_static_array_run
    "${CMAKE_SOURCE_DIR}/tests/std_traits_text_static_array_run.wio"
    "std-traits-text-static-array-ok"
    --run
)
wio_test_labels(wio_test_std_traits_text_static_array_run runtime coherence)

set_tests_properties(
    wio_test_std_traits_text_static_array_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_contract_v1_run
    "${CMAKE_SOURCE_DIR}/tests/std_contract_v1_run.wio"
    "Std contract v1: true/5/9/surface\\.txt/WIO-STD-V1/true"
    --run
)
wio_test_labels(wio_test_std_contract_v1_run runtime traceability)

add_wio_success_test(
    wio_test_std_heap_box_dry_run
    "${CMAKE_SOURCE_DIR}/tests/std_heap_box_run.wio"
)
wio_test_labels(wio_test_std_heap_box_dry_run runtime)

add_wio_success_test(
    wio_test_std_event_any_dry_run
    "${CMAKE_SOURCE_DIR}/tests/std_event_any_run.wio"
)
wio_test_labels(wio_test_std_event_any_dry_run runtime)

add_wio_success_test(
    wio_test_std_result_unwrap_sugar_dry_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_unwrap_sugar_run.wio"
)
wio_test_labels(wio_test_std_result_unwrap_sugar_dry_run runtime)

add_wio_success_test(
    wio_test_std_result_propagate_sugar_dry_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_propagate_sugar_run.wio"
)
wio_test_labels(wio_test_std_result_propagate_sugar_dry_run runtime)

add_wio_success_test(
    wio_test_std_result_generic_helpers_dry_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_generic_helpers_run.wio"
)
wio_test_labels(wio_test_std_result_generic_helpers_dry_run runtime)

add_wio_success_test(
    wio_test_std_result_explicit_generic_sugar_dry_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_explicit_generic_sugar_run.wio"
)
wio_test_labels(wio_test_std_result_explicit_generic_sugar_dry_run runtime)

add_wio_output_test(
    wio_test_std_heap_box_run
    "${CMAKE_SOURCE_DIR}/tests/std_heap_box_run.wio"
    "std-heap-box-ok"
    --run
)
wio_test_labels(wio_test_std_heap_box_run runtime traceability)

add_wio_output_test(
    wio_test_std_heap_box_semantics_run
    "${CMAKE_SOURCE_DIR}/tests/std_heap_box_semantics_run.wio"
    "std-heap-box-semantics-ok"
    --run
)
wio_test_labels(wio_test_std_heap_box_semantics_run runtime traceability)

add_wio_output_test(
    wio_test_std_event_any_run
    "${CMAKE_SOURCE_DIR}/tests/std_event_any_run.wio"
    "std-event-any-ok"
    --run
)
wio_test_labels(wio_test_std_event_any_run runtime traceability)

add_wio_output_test(
    wio_test_std_result_console_io_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_console_io_run.wio"
    "std-result-console-io-ok"
    --run
)
wio_test_labels(wio_test_std_result_console_io_run runtime)

add_wio_output_test(
    wio_test_std_result_unwrap_sugar_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_unwrap_sugar_run.wio"
    "std-result-unwrap-ok"
    --run
)
wio_test_labels(wio_test_std_result_unwrap_sugar_run runtime)

add_wio_output_test(
    wio_test_std_result_propagate_sugar_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_propagate_sugar_run.wio"
    "std-result-propagate-ok"
    --run
)
wio_test_labels(wio_test_std_result_propagate_sugar_run runtime)

add_wio_output_test(
    wio_test_std_result_generic_helpers_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_generic_helpers_run.wio"
    "std-result-generic-helpers-ok"
    --run
)
wio_test_labels(wio_test_std_result_generic_helpers_run runtime)

add_wio_output_test(
    wio_test_std_result_combinators_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_combinators_run.wio"
    "std-result-combinators-ok"
    --run
)
wio_test_labels(wio_test_std_result_combinators_run runtime)

add_wio_output_test(
    wio_test_std_result_explicit_generic_sugar_run
    "${CMAKE_SOURCE_DIR}/tests/std_result_explicit_generic_sugar_run.wio"
    "std-result-explicit-generic-ok"
    --run
)
wio_test_labels(wio_test_std_result_explicit_generic_sugar_run runtime)

add_wio_output_test(
    wio_test_std_unit_result_alias_run
    "${CMAKE_SOURCE_DIR}/tests/std_unit_result_alias_run.wio"
    "std-unit-result-alias-ok"
    --run
)
wio_test_labels(wio_test_std_unit_result_alias_run runtime)

set_tests_properties(
    wio_test_std_unit_result_alias_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_attribute_contracts_run
    "${CMAKE_SOURCE_DIR}/tests/std_attribute_contracts_run.wio"
    "std-attribute-contracts-ok"
    --run
)
wio_test_labels(wio_test_std_attribute_contracts_run runtime freeze)

set_tests_properties(
    wio_test_std_attribute_contracts_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_unicode_builders_run
    "${CMAKE_SOURCE_DIR}/tests/std_unicode_builders_run.wio"
    "std-unicode-builders-ok"
    --run
)
wio_test_labels(wio_test_std_unicode_builders_run runtime)

add_wio_output_test(
    wio_test_std_binary_serialization_run
    "${CMAKE_SOURCE_DIR}/tests/std_binary_serialization_run.wio"
    "std-binary-serialization-ok"
    --run
)
wio_test_labels(wio_test_std_binary_serialization_run runtime)

add_wio_output_test(
    wio_test_std_net_udp_loopback_run
    "${CMAKE_SOURCE_DIR}/tests/std_net_udp_loopback_run.wio"
    "std-net-udp-loopback-ok"
    --run
)
wio_test_labels(wio_test_std_net_udp_loopback_run runtime)

add_wio_output_test(
    wio_test_std_structured_log_run
    "${CMAKE_SOURCE_DIR}/tests/std_structured_log_run.wio"
    "std-structured-log-ok"
    --run
)
wio_test_labels(wio_test_std_structured_log_run runtime)

add_wio_output_test(
    wio_test_std_unicode_grapheme_run
    "${CMAKE_SOURCE_DIR}/tests/std_unicode_grapheme_run.wio"
    "std-unicode-grapheme-ok"
    --run
)
wio_test_labels(wio_test_std_unicode_grapheme_run runtime)

add_wio_output_test(
    wio_test_std_concurrency_future_run
    "${CMAKE_SOURCE_DIR}/tests/std_concurrency_future_run.wio"
    "std-concurrency-future-ok"
    --run
)
wio_test_labels(wio_test_std_concurrency_future_run runtime)

set_tests_properties(
    wio_test_std_concurrency_future_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_std_unicode_grapheme_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_unicode_normalization_run
    "${CMAKE_SOURCE_DIR}/tests/std_unicode_normalization_run.wio"
    "std-unicode-normalization-ok"
    --run
)
wio_test_labels(wio_test_std_unicode_normalization_run runtime)

add_wio_output_test(
    wio_test_std_container_invariants_014_run
    "${CMAKE_SOURCE_DIR}/tests/std_container_invariants_014_run.wio"
    "std-container-invariants-014-ok"
    --run
)
wio_test_labels(wio_test_std_container_invariants_014_run runtime)

set_tests_properties(
    wio_test_std_container_invariants_014_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_platform_conformance_vectors_run
    "${CMAKE_SOURCE_DIR}/tests/std_platform_conformance_vectors_run.wio"
    "std-platform-conformance-vectors-ok"
    --run
)
wio_test_labels(wio_test_std_platform_conformance_vectors_run runtime)

set_tests_properties(
    wio_test_std_platform_conformance_vectors_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_std_unicode_normalization_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_std_structured_log_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_std_net_udp_loopback_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_std_binary_serialization_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_std_unicode_builders_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_json_hardening_run
    "${CMAKE_SOURCE_DIR}/tests/std_json_hardening_run.wio"
    "std-json-hardening-ok"
    --run
)
wio_test_labels(wio_test_std_json_hardening_run runtime)

set_tests_properties(
    wio_test_std_json_hardening_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_json_numeric_bounds_run
    "${CMAKE_SOURCE_DIR}/tests/std_json_numeric_bounds_run.wio"
    "std-json-numeric-bounds-ok"
    --run
)
wio_test_labels(wio_test_std_json_numeric_bounds_run runtime)

set_tests_properties(
    wio_test_std_json_numeric_bounds_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_serialization_codec_run
    "${CMAKE_SOURCE_DIR}/tests/std_serialization_codec_run.wio"
    "std-serialization-codec-ok"
    --run
)
wio_test_labels(wio_test_std_serialization_codec_run runtime)

set_tests_properties(
    wio_test_std_serialization_codec_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_concurrency_foundation_run
    "${CMAKE_SOURCE_DIR}/tests/std_concurrency_foundation_run.wio"
    "std-concurrency-foundation-ok"
    --run
)
wio_test_labels(wio_test_std_concurrency_foundation_run runtime)

set_tests_properties(
    wio_test_std_concurrency_foundation_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_net_foundation_run
    "${CMAKE_SOURCE_DIR}/tests/std_net_foundation_run.wio"
    "std-net-foundation-ok"
    --run
)
wio_test_labels(wio_test_std_net_foundation_run runtime)

set_tests_properties(
    wio_test_std_net_foundation_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_os_facilities_run
    "${CMAKE_SOURCE_DIR}/tests/std_os_facilities_run.wio"
    "std-os-facilities-ok"
    --run
)
wio_test_labels(wio_test_std_os_facilities_run runtime)

set_tests_properties(
    wio_test_std_os_facilities_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_data_utilities_run
    "${CMAKE_SOURCE_DIR}/tests/std_data_utilities_run.wio"
    "std-data-utilities-ok"
    --run
)
wio_test_labels(wio_test_std_data_utilities_run runtime)

set_tests_properties(
    wio_test_std_data_utilities_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_identity_random_hardening_run
    "${CMAKE_SOURCE_DIR}/tests/std_identity_random_hardening_run.wio"
    "std-identity-random-hardening-ok"
    --run
)
wio_test_labels(wio_test_std_identity_random_hardening_run runtime)

set_tests_properties(
    wio_test_std_identity_random_hardening_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_binary_bigint_compression_run
    "${CMAKE_SOURCE_DIR}/tests/std_binary_bigint_compression_run.wio"
    "std-binary-bigint-compression-ok"
    --run
)
wio_test_labels(wio_test_std_binary_bigint_compression_run runtime)

set_tests_properties(
    wio_test_std_binary_bigint_compression_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_success_test(
    wio_test_async_cancellable_io_surface_check
    "${CMAKE_SOURCE_DIR}/tests/async_cancellable_io_surface_check.wio"
)
wio_test_labels(wio_test_async_cancellable_io_surface_check runtime freeze)

set_tests_properties(
    wio_test_async_cancellable_io_surface_check
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_async_cancellation_outcome_run
    "${CMAKE_SOURCE_DIR}/tests/async_cancellation_outcome_run.wio"
    "async-cancellation-outcome-ok"
    --run
)
wio_test_labels(wio_test_async_cancellation_outcome_run runtime freeze)

set_tests_properties(
    wio_test_async_cancellation_outcome_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_regex_time_hardening_run
    "${CMAKE_SOURCE_DIR}/tests/std_regex_time_hardening_run.wio"
    "std-regex-time-hardening-ok"
    --run
)
wio_test_labels(wio_test_std_regex_time_hardening_run runtime)

set_tests_properties(
    wio_test_std_regex_time_hardening_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_std_regex_records_safety_run
    "${CMAKE_SOURCE_DIR}/tests/std_regex_records_safety_run.wio"
    "std-regex-records-safety-ok"
    --run
)
wio_test_labels(wio_test_std_regex_records_safety_run runtime)

set_tests_properties(
    wio_test_std_regex_records_safety_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_async_coroutine_run
    "${CMAKE_SOURCE_DIR}/tests/async_coroutine_run.wio"
    "async-coroutine-ok"
    --run
)
wio_test_labels(wio_test_async_coroutine_run runtime freeze)

set_tests_properties(
    wio_test_async_coroutine_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

add_wio_output_test(
    wio_test_async_structured_run
    "${CMAKE_SOURCE_DIR}/tests/async_structured_run.wio"
    "async-structured-ok"
    --run
)
wio_test_labels(wio_test_async_structured_run runtime freeze)

add_wio_output_test(
    wio_test_async_block_on_run
    "${CMAKE_SOURCE_DIR}/tests/async_block_on_run.wio"
    "async-block-on-ok"
    --run
)
wio_test_labels(wio_test_async_block_on_run runtime freeze)

add_wio_output_test(
    wio_test_async_freeze_run
    "${CMAKE_SOURCE_DIR}/tests/async_freeze_run.wio"
    "async-freeze-ok"
    --run
)
wio_test_labels(wio_test_async_freeze_run runtime freeze)

add_wio_output_test(
    wio_test_async_detached_exit_run
    "${CMAKE_SOURCE_DIR}/tests/async_detached_exit_run.wio"
    "async-detached-exit-ok"
    --run
)
wio_test_labels(wio_test_async_detached_exit_run runtime freeze)

add_wio_output_test(
    wio_test_async_worker_config_run
    "${CMAKE_SOURCE_DIR}/tests/async_worker_config_run.wio"
    "async-worker-config-ok"
    --run
)
wio_test_labels(wio_test_async_worker_config_run runtime freeze)

add_wio_output_test(
    wio_test_async_correctness_run
    "${CMAKE_SOURCE_DIR}/tests/async_correctness_run.wio"
    "async-correctness-ok"
    --run
)
wio_test_labels(wio_test_async_correctness_run runtime freeze)

add_wio_output_test(
    wio_test_async_task_surface_run
    "${CMAKE_SOURCE_DIR}/tests/async_task_surface_run.wio"
    "async-task-surface-ok"
    --run
)
wio_test_labels(wio_test_async_task_surface_run runtime freeze)

add_wio_output_test(
    wio_test_async_scope_run
    "${CMAKE_SOURCE_DIR}/tests/async_scope_run.wio"
    "async-scope-ok"
    --run
)
wio_test_labels(wio_test_async_scope_run runtime freeze)

add_wio_output_test(
    wio_test_async_scope_syntax_run
    "${CMAKE_SOURCE_DIR}/tests/async_scope_syntax_run.wio"
    "async-scope-syntax-ok"
    --run
)
wio_test_labels(wio_test_async_scope_syntax_run runtime freeze)

add_wio_output_test(
    wio_test_async_scope_auto_import_run
    "${CMAKE_SOURCE_DIR}/tests/async_scope_auto_import_run.wio"
    "async-scope-auto-import-ok"
    --run
)
wio_test_labels(wio_test_async_scope_auto_import_run runtime freeze)

add_wio_output_test(
    wio_test_async_main_executor_run
    "${CMAKE_SOURCE_DIR}/tests/async_main_executor_run.wio"
    "async-main-executor-ok"
    --run
)
wio_test_labels(wio_test_async_main_executor_run runtime freeze)

add_wio_output_test(
    wio_test_async_fs_io_run
    "${CMAKE_SOURCE_DIR}/tests/async_fs_io_run.wio"
    "async-fs-io-ok"
    --run
)
wio_test_labels(wio_test_async_fs_io_run runtime freeze)

add_wio_output_test(
    wio_test_async_process_run
    "${CMAKE_SOURCE_DIR}/tests/async_process_run.wio"
    "async-process-ok"
    --run
)
wio_test_labels(wio_test_async_process_run runtime freeze)

add_wio_output_test(
    wio_test_async_process_stream_run
    "${CMAKE_SOURCE_DIR}/tests/async_process_stream_run.wio"
    "async-process-stream-ok"
    --run
)
wio_test_labels(wio_test_async_process_stream_run runtime freeze)

add_wio_output_test(
    wio_test_async_channel_run
    "${CMAKE_SOURCE_DIR}/tests/async_channel_run.wio"
    "async-channel-ok"
    --run
)
wio_test_labels(wio_test_async_channel_run runtime freeze)

add_wio_output_test(
    wio_test_std_module_reimport_alias_run
    "${CMAKE_SOURCE_DIR}/tests/std_module_reimport_alias_run.wio"
    "std-module-reimport-alias-ok"
    --run
)
wio_test_labels(wio_test_std_module_reimport_alias_run runtime traceability)

add_wio_output_test(
    wio_test_async_file_watcher_run
    "${CMAKE_SOURCE_DIR}/tests/async_file_watcher_run.wio"
    "async-file-watcher-ok"
    --run
)
wio_test_labels(wio_test_async_file_watcher_run runtime freeze)

add_wio_output_test(
    wio_test_async_net_io_run
    "${CMAKE_SOURCE_DIR}/tests/async_net_io_run.wio"
    "async-net-io-ok"
    --run
)
wio_test_labels(wio_test_async_net_io_run runtime freeze)

add_wio_output_test(
    wio_test_async_net_accept_run
    "${CMAKE_SOURCE_DIR}/tests/async_net_accept_run.wio"
    "async-net-accept-ok"
    --run
)
wio_test_labels(wio_test_async_net_accept_run runtime freeze)

set_tests_properties(
    wio_test_async_net_io_run
    PROPERTIES TIMEOUT 360
)

set_tests_properties(
    wio_test_async_net_accept_run
    PROPERTIES TIMEOUT 360
)

add_wio_output_test(
    wio_test_std_net_timeout_run
    "${CMAKE_SOURCE_DIR}/tests/std_net_timeout_run.wio"
    "std-net-timeout-ok"
    --run
)
wio_test_labels(wio_test_std_net_timeout_run runtime)

set_tests_properties(
    wio_test_async_structured_run
    wio_test_async_block_on_run
    wio_test_async_freeze_run
    wio_test_async_detached_exit_run
    wio_test_async_correctness_run
    wio_test_async_task_surface_run
    wio_test_async_scope_run
    wio_test_async_scope_syntax_run
    wio_test_async_scope_auto_import_run
    wio_test_async_main_executor_run
    wio_test_async_fs_io_run
    wio_test_async_process_run
    wio_test_async_process_stream_run
    wio_test_async_channel_run
    wio_test_std_module_reimport_alias_run
    wio_test_async_file_watcher_run
    wio_test_async_net_io_run
    wio_test_async_net_accept_run
    wio_test_std_net_timeout_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    wio_test_async_process_stream_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR};WIO_ASYNC_IO_WORKERS=1"
)

set_tests_properties(
    wio_test_async_worker_config_run
    PROPERTIES ENVIRONMENT "WIO_ROOT=${CMAKE_SOURCE_DIR};WIO_ASYNC_WORKERS=3"
)

set_tests_properties(
    wio_test_async_detached_exit_run
    PROPERTIES TIMEOUT 360
)
