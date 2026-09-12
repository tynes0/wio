#pragma once
#include "wio/codegen/wir_module_emitter.h"

namespace wio::codegen {
class WirReflectionEmitter final {
public:
    using ThunkEmitter = std::function<void(const wir::lowered::Function&, const std::string&)>;
    static std::string emit(const wir::lowered::Module&, const WirModuleEmitter::TypeSpelling&, const ThunkEmitter&);
    static std::string traits(const wir::lowered::Module&, const WirModuleEmitter::TypeSpelling&);
};
} // namespace wio::codegen
