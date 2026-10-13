# Runtime-only wire model. Keep WIR conversion/enum encoders in the compiler.
set(WIO_BYTECODE_CORE_SOURCES
    "${CMAKE_SOURCE_DIR}/compiler/src/wio_compiler/bytecode/codec.cpp"
    "${CMAKE_SOURCE_DIR}/compiler/src/wio_compiler/bytecode/module.cpp"
    "${CMAKE_SOURCE_DIR}/compiler/src/wio_compiler/bytecode/format.cpp"
    "${CMAKE_SOURCE_DIR}/compiler/src/wio_compiler/bytecode/verifier.cpp"
    "${CMAKE_SOURCE_DIR}/compiler/src/wio_compiler/bytecode/disassembler.cpp")
add_library(wio_bytecode_core STATIC ${WIO_BYTECODE_CORE_SOURCES})
add_library(wio-bytecode-core ALIAS wio_bytecode_core)
target_include_directories(wio_bytecode_core PUBLIC "${CMAKE_SOURCE_DIR}/compiler/include")
set_target_properties(wio_bytecode_core PROPERTIES FOLDER "runtime" POSITION_INDEPENDENT_CODE ON)
install(TARGETS wio_bytecode_core ARCHIVE DESTINATION lib COMPONENT WioVmRuntime)
install(FILES
    "${CMAKE_SOURCE_DIR}/compiler/include/wio/bytecode/codec.h"
    "${CMAKE_SOURCE_DIR}/compiler/include/wio/bytecode/module.h"
    "${CMAKE_SOURCE_DIR}/compiler/include/wio/bytecode/contract.h"
    "${CMAKE_SOURCE_DIR}/compiler/include/wio/bytecode/format.h"
    "${CMAKE_SOURCE_DIR}/compiler/include/wio/bytecode/verifier.h"
    "${CMAKE_SOURCE_DIR}/compiler/include/wio/bytecode/disassembler.h"
    DESTINATION include/wio/bytecode COMPONENT WioVmRuntime)
