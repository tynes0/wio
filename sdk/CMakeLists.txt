set(WIO_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(WIO_SDK_HEADERS
    "${WIO_SDK_INCLUDE_DIR}/module_api.h"
    "${WIO_SDK_INCLUDE_DIR}/wio_sdk.h"
)

add_library(wio_sdk INTERFACE)
target_include_directories(wio_sdk INTERFACE "${WIO_SDK_INCLUDE_DIR}")
target_sources(wio_sdk INTERFACE ${WIO_SDK_HEADERS})

install(DIRECTORY "${WIO_SDK_INCLUDE_DIR}/"
    DESTINATION sdk/include
)
