set(WIO_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(WIO_SDK_HEADERS
    "${WIO_SDK_INCLUDE_DIR}/wio_version.h"
    "${WIO_SDK_INCLUDE_DIR}/module_api.h"
    "${WIO_SDK_INCLUDE_DIR}/wio_sdk.h"
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
endif()

install(DIRECTORY "${WIO_SDK_INCLUDE_DIR}/"
    DESTINATION sdk/include
)
