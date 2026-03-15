add_library(frenum INTERFACE)

target_include_directories(frenum INTERFACE ${CMAKE_SOURCE_DIR}/libs/frenum/include)

set_target_properties(frenum PROPERTIES FOLDER "libs")