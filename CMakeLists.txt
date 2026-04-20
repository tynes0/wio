cmake_minimum_required(VERSION 3.20)

project(wio_lang
    VERSION 0.1.0
    LANGUAGES C CXX
)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WIO_BACKEND_CXX_COMPILER "g++" CACHE STRING "C++ compiler used for generated Wio output.")
set(WIO_BACKEND_AR "ar" CACHE STRING "Archiver used for generated Wio static libraries.")

if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

option(WIO_BUILD_APP "Build application" ON)
set(WIO_DIST_DIR "${CMAKE_BINARY_DIR}/dist" CACHE PATH "Default packaged Wio distribution directory.")

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

install(FILES
    "${CMAKE_SOURCE_DIR}/cmake/WioProject.cmake"
    DESTINATION cmake
)

install(FILES
    "${CMAKE_SOURCE_DIR}/scripts/Invoke-WioProject.ps1"
    "${CMAKE_SOURCE_DIR}/scripts/Invoke-WithSanitizedPath.ps1"
    "${CMAKE_SOURCE_DIR}/scripts/New-WioProject.ps1"
    "${CMAKE_SOURCE_DIR}/scripts/Pack-Wio.ps1"
    DESTINATION scripts
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/std/"
    DESTINATION std
    FILES_MATCHING PATTERN "*.wio"
)

install(FILES
    "${CMAKE_SOURCE_DIR}/docs/WIO_PROJECT_SYSTEM.md"
    DESTINATION docs
)

if (WIO_BUILD_APP)
    add_custom_target(wio_dist
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --config $<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Debug> --prefix "${WIO_DIST_DIR}"
        DEPENDS wio_app wio_runtime_backend
        USES_TERMINAL
        VERBATIM
    )
endif()
