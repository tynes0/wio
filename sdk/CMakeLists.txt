set(WIO_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(WIO_SDK_HEADERS
    "${WIO_SDK_INCLUDE_DIR}/wio_version.h"
    "${WIO_SDK_INCLUDE_DIR}/module_api.h"
    "${WIO_SDK_INCLUDE_DIR}/wio_features.h"
    "${WIO_SDK_INCLUDE_DIR}/wio_sdk.h"
    "${WIO_SDK_INCLUDE_DIR}/wio_values.h"
    "${WIO_SDK_INCLUDE_DIR}/wio_native_abi.h"
)

add_library(wio_sdk INTERFACE)
target_include_directories(wio_sdk INTERFACE "${WIO_SDK_INCLUDE_DIR}")
target_sources(wio_sdk INTERFACE ${WIO_SDK_HEADERS})

if(BUILD_TESTING)
    add_executable(wio_sdk_version_contract_test
        "${CMAKE_SOURCE_DIR}/tests/sdk_version_contract_test.cpp"
    )
    target_link_libraries(wio_sdk_version_contract_test PRIVATE wio_sdk)
    target_compile_definitions(wio_sdk_version_contract_test
        PRIVATE WIO_EXPECTED_PRODUCT_VERSION="${PROJECT_VERSION}"
    )
    add_test(
        NAME wio_sdk_version_contract
        COMMAND wio_sdk_version_contract_test
    )

    add_executable(wio_sdk_013_value_surface_test
        "${CMAKE_SOURCE_DIR}/tests/sdk_013_value_surface_test.cpp"
    )
    target_link_libraries(wio_sdk_013_value_surface_test PRIVATE wio_sdk)
    add_test(
        NAME wio_sdk_013_value_surface
        COMMAND wio_sdk_013_value_surface_test
    )

    add_executable(wio_sdk_014_abi_markers_test
        "${CMAKE_SOURCE_DIR}/tests/sdk_014_abi_markers_test.cpp"
    )
    target_link_libraries(wio_sdk_014_abi_markers_test PRIVATE wio_sdk)
    add_test(
        NAME wio_sdk_014_abi_markers
        COMMAND wio_sdk_014_abi_markers_test
    )

    add_executable(wio_sdk_014_feature_inventory_test
        "${CMAKE_SOURCE_DIR}/tests/sdk_014_feature_inventory_test.cpp"
    )
    target_link_libraries(wio_sdk_014_feature_inventory_test PRIVATE wio_sdk)
    add_test(
        NAME wio_sdk_014_feature_inventory
        COMMAND wio_sdk_014_feature_inventory_test
    )

    add_executable(wio_sdk_016_callback_lifetime_test
        "${CMAKE_SOURCE_DIR}/tests/sdk_016_callback_lifetime_test.cpp"
    )
    target_link_libraries(wio_sdk_016_callback_lifetime_test PRIVATE wio_sdk)
    add_test(
        NAME wio_sdk_016_callback_lifetime
        COMMAND wio_sdk_016_callback_lifetime_test
    )

    add_executable(wio_sdk_016_native_resource_test
        "${CMAKE_SOURCE_DIR}/tests/sdk_016_native_resource_test.cpp"
    )
    target_link_libraries(wio_sdk_016_native_resource_test PRIVATE wio_sdk)
    add_test(
        NAME wio_sdk_016_native_resource
        COMMAND wio_sdk_016_native_resource_test
    )
endif()

install(DIRECTORY "${WIO_SDK_INCLUDE_DIR}/"
    DESTINATION sdk/include
)
