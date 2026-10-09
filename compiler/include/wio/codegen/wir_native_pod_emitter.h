#pragma once

#include "wio/codegen/wir_module_emitter.h"

#include <vector>

namespace wio::codegen
{
    class WirNativePodEmitter final
    {
    public:
        [[nodiscard]] static std::vector<wir::TypeId> eligibleTypes(const wir::lowered::Module& module);
        [[nodiscard]] static std::string emit(const wir::lowered::Module& module,
                                              const WirModuleEmitter::TypeSpelling& cpp);
    };
} // namespace wio::codegen
