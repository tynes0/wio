#pragma once

#include "wio/codegen/wir_cpp_storage_plan.h"

namespace wio::codegen
{
    // Emission-only construction forwarding. Never changes WIR, public ABI,
    // reference identity or the order of observable cleanup/effects.
    struct WirCppStringInitPlan final
    {
        std::unordered_map<std::uint32_t, wir::ValueId> producerCells;
        std::unordered_set<const wir::lowered::Instruction*> suppressedInstructions;

        [[nodiscard]] static WirCppStringInitPlan build(const wir::lowered::Module& module,
                                                        const wir::lowered::Function& function,
                                                        const WirCppStoragePlan& storage);
    };
} // namespace wio::codegen
