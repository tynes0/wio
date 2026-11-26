cmake_minimum_required(VERSION 3.20)
project(VmCliArchiveConsumer LANGUAGES CXX)
find_package(WioVmRuntime CONFIG REQUIRED)
add_executable(archive_consumer "${WIO_HOST_SOURCE}")
target_compile_features(archive_consumer PRIVATE cxx_std_20)
target_compile_definitions(archive_consumer PRIVATE WIO_TEST_STATIC)
target_link_libraries(archive_consumer PRIVATE "${WIO_ARCHIVE}" Wio::wio_vm Wio::wio_sdk)
if(MSVC AND WioVmRuntime_MSVC_STATIC_RUNTIME)
    set_property(TARGET archive_consumer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
    target_link_options(archive_consumer PRIVATE -municode)
endif()
get_filename_component(manifest_directory "${WIO_NATIVE_MANIFEST}" DIRECTORY)
get_filename_component(stage "${manifest_directory}" DIRECTORY)
add_custom_command(TARGET archive_consumer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DMANIFEST=${WIO_NATIVE_MANIFEST}" "-DSTAGE=${stage}"
        "-DDESTINATION=$<TARGET_FILE_DIR:archive_consumer>"
        -P "${WioVmRuntime_DIR}/DeployWioVmNative.cmake" VERBATIM)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-$<CONFIG>.cmake"
    CONTENT "set(consumer [==[$<TARGET_FILE:archive_consumer>]==])\n")
