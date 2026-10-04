set(WIO_CLI_TEST_DEPENDENCIES wio_app)

if(TARGET wio_selfhost_cli)
    list(APPEND WIO_CLI_TEST_DEPENDENCIES wio_selfhost_cli)
endif()

set(wio_ctest_config_args)

if (CMAKE_CONFIGURATION_TYPES)
    set(wio_ctest_config_args -C $<CONFIG>)
endif()

add_custom_target(
    wio_tests
    COMMAND ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS ${WIO_CLI_TEST_DEPENDENCIES}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    wio_tests_list
    COMMAND ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -N
    COMMAND_EXPAND_LISTS
    DEPENDS wio_app
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    wio_tests_traceability
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -L "^traceability$" --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS ${WIO_CLI_TEST_DEPENDENCIES}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    wio_tests_freeze
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args}
        -L "^(traceability|freeze|coherence)$" --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS ${WIO_CLI_TEST_DEPENDENCIES} wio_async_runtime_stress
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    wio_tests_language_coherence
    COMMAND ${CMAKE_COMMAND} -E env "WIO_ROOT=${CMAKE_SOURCE_DIR}"
        ${CMAKE_CTEST_COMMAND} ${wio_ctest_config_args} -L "^coherence$" --output-on-failure
    COMMAND_EXPAND_LISTS
    DEPENDS ${WIO_CLI_TEST_DEPENDENCIES}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)
