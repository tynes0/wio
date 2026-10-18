#pragma once

#include "wio/wir/lowered_ir.h"

#include <unordered_map>

namespace wio::codegen
{
    // Only private sole-consumer rendered bytes change representation. Numeric
    // formatting stays at the original instruction, before any later Wio hook.
    struct WirCppScalarFormatPlan final
    {
        enum class Storage
        {
            BooleanView,
            IntegerBytes
        };
        std::unordered_map<std::uint32_t, Storage> fragments;

        [[nodiscard]] static WirCppScalarFormatPlan build(const wir::lowered::Module& module,
                                                          const wir::lowered::Function& function);
    };
} // namespace wio::codegen
