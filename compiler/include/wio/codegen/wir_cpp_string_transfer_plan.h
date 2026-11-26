#pragma once

#include "wio/wir/lowered_ir.h"

#include <unordered_map>

namespace wio::codegen
{
    // Implementation-only storage forwarding, not an ownership or CFG rewrite.
    // A fresh string producer immediately followed by its only edge transfer
    // can write the destination slot without two intermediate string moves.
    struct WirCppStringTransferPlan final
    {
        std::unordered_map<std::uint32_t, wir::ValueId> edgeDestinations;

        [[nodiscard]] static bool isFreshStringProducer(const wir::lowered::Module& module,
                                                        const wir::lowered::Instruction& instruction);

        [[nodiscard]] static WirCppStringTransferPlan build(const wir::lowered::Module& module,
                                                            const wir::lowered::Function& function);
    };
} // namespace wio::codegen
