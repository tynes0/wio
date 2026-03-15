add_library(coco INTERFACE)

target_include_directories(coco INTERFACE ${CMAKE_SOURCE_DIR}/libs/coco/include)

set_target_properties(coco PROPERTIES FOLDER "libs")