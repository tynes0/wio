cmake_minimum_required(VERSION 3.24)
project(wio_static_consumer LANGUAGES CXX)

set(WIO_ROOT "$ENV{WIO_ROOT}" CACHE PATH "Path to the Wio toolchain root or packaged distribution.")
if(NOT WIO_ROOT)
    get_filename_component(WIO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

if(NOT EXISTS "${WIO_ROOT}/cmake/WioProject.cmake")
    message(FATAL_ERROR "WioProject.cmake was not found under WIO_ROOT='${WIO_ROOT}'.")
endif()

include("${WIO_ROOT}/cmake/WioProject.cmake")

wio_add_project_targets(example_math
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/wio.makewio"
    WIO_ROOT "${WIO_ROOT}"
)

add_executable(wio_static_consumer
    host/main.cpp
)

target_compile_features(wio_static_consumer PRIVATE cxx_std_20)
wio_target_link_project(wio_static_consumer PRIVATE example_math)
