#pragma once

#include "wio/wir/lowered_ir.h"

#include <unordered_set>

namespace wio::codegen
{
    // A private non-owning entry, never a replacement for the public Place ABI.
    // No receiver use may survive a potentially reentrant formatting operation.
    struct WirCppReceiverPlan final
    {
        std::unordered_set<std::uint32_t> receiverValues;

        [[nodiscard]] bool eligible() const
        {
            return !receiverValues.empty();
        }

        [[nodiscard]] static WirCppReceiverPlan build(const wir::lowered::Module& module,
                                                      const wir::lowered::Function& function);
    };
} // namespace wio::codegen
