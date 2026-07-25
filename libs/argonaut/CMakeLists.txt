add_library(argonaut INTERFACE)

target_include_directories(argonaut INTERFACE ${CMAKE_SOURCE_DIR}/libs/argonaut/include)

set_target_properties(argonaut PROPERTIES FOLDER "libs")

if(BUILD_TESTING)
    add_executable(argonaut_parser_smoke
        tests/parser_smoke.cpp
    )
    target_link_libraries(argonaut_parser_smoke PRIVATE argonaut)
    add_test(NAME argonaut_parser_smoke COMMAND argonaut_parser_smoke)
endif()
