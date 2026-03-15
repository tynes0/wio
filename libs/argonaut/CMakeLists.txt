add_library(argonaut INTERFACE)

target_include_directories(argonaut INTERFACE ${CMAKE_SOURCE_DIR}/libs/argonaut/include)

set_target_properties(argonaut PROPERTIES FOLDER "libs")