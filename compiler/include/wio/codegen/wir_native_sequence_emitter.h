#pragma once

#include "wio/codegen/wir_module_emitter.h"

namespace wio::codegen
{
    class WirNativeSequenceEmitter final
    {
    public:
        [[nodiscard]] static std::string emit(const wir::lowered::Module& module,
                                              const WirModuleEmitter::TypeSpelling& cpp);
    };
} // namespace wio::codegen
