cmake_minimum_required(VERSION 3.20)

project(wio_lang
    VERSION 0.1.0
    LANGUAGES C CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WIO_BACKEND_CXX_COMPILER "g++" CACHE STRING "C++ compiler used for generated Wio output.")
set(WIO_BACKEND_AR "ar" CACHE STRING "Archiver used for generated Wio static libraries.")

if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

option(WIO_BUILD_APP "Build application" ON)

# If the project is added to someone else's project, let's set the app to OFF by default.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(WIO_BUILD_APP OFF CACHE BOOL "" FORCE)
endif()

# --- LIBRARIES (Internal) ---

add_subdirectory(libs/dtlog)
add_subdirectory(libs/argonaut)
add_subdirectory(libs/coco)
add_subdirectory(libs/frenum)

# --- Main Project ---
add_subdirectory(sdk)
add_subdirectory(runtime)
add_subdirectory(compiler)
if (WIO_BUILD_APP)
    add_subdirectory(app)
endif()
