#pragma once
#include "wio/codegen/wir_module_emitter.h"

namespace wio::codegen {
class WirApplicationEmitter final {
public:
    static std::string emit(const wir::lowered::Module&, const WirModuleEmitter::TypeSpelling&);
};
} // namespace wio::codegen
