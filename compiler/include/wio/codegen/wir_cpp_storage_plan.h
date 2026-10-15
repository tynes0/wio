#pragma once

#include "wio/wir/lowered_ir.h"

#include <unordered_set>

namespace wio::codegen
{
    // Function-local emission decisions only. No WIR identity, ABI, ownership,
    // borrow lifetime or coroutine frame contract is rewritten by this plan.
    struct WirCppStoragePlan final
    {
        std::unordered_set<std::uint32_t> scalarPlaces;
        std::unordered_set<std::uint32_t> plainValues;
        bool directControlFlow = false;

        [[nodiscard]] static WirCppStoragePlan build(const wir::lowered::Module& module,
                                                     const wir::lowered::Function& function);
    };
} // namespace wio::codegen
