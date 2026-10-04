cmake_minimum_required(VERSION 3.20)
project(WioTestCatalogFixture LANGUAGES NONE)
include(CTest)

if(BUILD_TESTING)
    include("${WIO_ROOT}/tests/cmake/WioTestHelpers.cmake")

    # Deliberately unrelated names: suite membership is not name-driven.
    add_test(NAME renamed_positive COMMAND ${CMAKE_COMMAND} -E echo positive)
    wio_test_labels(renamed_positive language traceability coherence coherence)
    set_tests_properties(renamed_positive PROPERTIES LABELS "existing;native")

    add_test(NAME renamed_negative COMMAND ${CMAKE_COMMAND} -E echo negative)
    wio_test_labels(renamed_negative invalid freeze)
    wio_finalize_test_labels()

    get_property(labels TEST renamed_positive PROPERTY LABELS)
    foreach(expected existing native language traceability coherence)
        if(NOT expected IN_LIST labels)
            message(FATAL_ERROR "Suite finalization lost label ${expected}: ${labels}")
        endif()
    endforeach()
    list(LENGTH labels count)
    if(NOT count EQUAL 5)
        message(FATAL_ERROR "Suite labels were not deduplicated: ${labels}")
    endif()
endif()
