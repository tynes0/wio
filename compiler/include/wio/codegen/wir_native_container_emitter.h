#pragma once

#include "wio/codegen/wir_module_emitter.h"

namespace wio::codegen
{
    class WirNativeContainerEmitter final
    {
    public:
        [[nodiscard]] static std::string emit(const wir::lowered::Module& module,
                                              const WirModuleEmitter::TypeSpelling& cpp,
                                              std::span<const wir::TypeId> dynamicPayloadTypes = {});
    };
} // namespace wio::codegen
