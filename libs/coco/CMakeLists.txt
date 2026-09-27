add_library(coco INTERFACE)
add_library(wio::coco ALIAS coco)

target_include_directories(coco INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

set_target_properties(coco PROPERTIES FOLDER "libs")

if(BUILD_TESTING)
    add_executable(wio_coco_utility_test
        "${PROJECT_SOURCE_DIR}/tests/coco_utility_test.cpp"
    )
    target_link_libraries(wio_coco_utility_test PRIVATE wio::coco)
    set_target_properties(wio_coco_utility_test PROPERTIES FOLDER "tests")

    add_test(NAME wio_coco_utility_test COMMAND wio_coco_utility_test)
    set_tests_properties(wio_coco_utility_test PROPERTIES
        TIMEOUT 15
        LABELS "utility;performance;trace"
    )
endif()
