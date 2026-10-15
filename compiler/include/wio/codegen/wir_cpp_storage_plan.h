#pragma once

#include "wio/wir/lowered_ir.h"

#include <unordered_set>
#include <unordered_map>

namespace wio::codegen
{
    // Function-local emission decisions only. No WIR identity, ABI, ownership,
    // borrow lifetime or coroutine frame contract is rewritten by this plan.
    struct WirCppStoragePlan final
    {
        std::unordered_set<std::uint32_t> scalarPlaces;
        std::unordered_set<std::uint32_t> stringPlaces;
        std::unordered_set<std::uint32_t> plainValues;
        std::unordered_map<std::uint32_t, wir::ValueId> borrowedStringCopies;
        // Sole-use Add immediately followed by a store to its freshly loaded,
        // mutable local string/text cell. RHS snapshots remain owned.
        std::unordered_map<std::uint32_t, wir::ValueId> localAppendSuffixes;
        bool directControlFlow = false;

        [[nodiscard]] static WirCppStoragePlan build(const wir::lowered::Module& module,
                                                     const wir::lowered::Function& function);
    };
} // namespace wio::codegen
