file(GLOB_RECURSE WIO_TEST_FILES "${CMAKE_SOURCE_DIR}/tests/*.wio")

source_group("tests" FILES ${WIO_TEST_FILES})

add_executable(wio_app
    main.cpp
    ${WIO_TEST_FILES}
)

target_link_libraries(wio_app
    PRIVATE
        wio_compiler
)

target_include_directories(wio_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/compiler/include
)

if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
    add_custom_command(TARGET wio_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/tests"
            "$<TARGET_FILE_DIR:wio_app>/tests"
        COMMENT "Test files are being copied...."
    )
else()
    message(WARNING "WARNING: The 'tests' folder was not found; the copy operation was skipped.")
endif()

if (BUILD_TESTING AND EXISTS "${CMAKE_SOURCE_DIR}/tests")
    add_test(
        NAME wio_test1_dry_run
        COMMAND $<TARGET_FILE:wio_app> "${CMAKE_SOURCE_DIR}/tests/test1.wio" --dry-run
    )

    add_test(
        NAME wio_test2_dry_run
        COMMAND $<TARGET_FILE:wio_app> "${CMAKE_SOURCE_DIR}/tests/test2.wio" --dry-run
    )

    set_tests_properties(
        wio_test1_dry_run
        wio_test2_dry_run
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
