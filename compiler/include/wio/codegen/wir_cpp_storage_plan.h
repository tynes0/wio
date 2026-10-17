#pragma once

#include "wio/wir/lowered_ir.h"

#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace wio::codegen
{
    // Function-local emission decisions only. No WIR identity, ABI, ownership,
    // borrow lifetime or coroutine frame contract is rewritten by this plan.
    struct WirCppStoragePlan final
    {
        std::unordered_set<std::uint32_t> scalarPlaces;
        // Primitive Wio fields used only inside a bounded, call-free block
        // interval. The receiver is checked when forming the address.
        std::unordered_set<std::uint32_t> scalarFieldPlaces;
        std::unordered_set<std::uint32_t> stringPlaces;
        std::unordered_set<std::uint32_t> plainValues;
        std::unordered_map<std::uint32_t, wir::ValueId> borrowedStringCopies;
        // Sole-use concat tree followed by a store into its freshly loaded,
        // mutable direct string/text cell. Only stable local loads may intervene.
        // Field/global cells need identical place IDs; proxies never qualify.
        std::unordered_map<std::uint32_t, std::vector<wir::ValueId>> localAppendSuffixes;
        std::unordered_set<std::uint32_t> elidedConcatValues;
        // Pure borrowed-leaf concat trees: one checked reserve and no temporary
        // prefix buffers. Calls/effects and owned reusable leaves keep rvalues.
        std::unordered_map<std::uint32_t, std::vector<wir::ValueId>> flattenedConcatOperands;
        // Independently owned, sole semantic use at this concat; cleanup stays.
        std::unordered_set<std::uint32_t> consumedConcatOperands;
        bool directControlFlow = false;

        [[nodiscard]] static bool isPlainScalar(const wir::TypeTable& types, wir::TypeId type);

        // A synchronous native leaf with only scalar values and call-scoped
        // UTF-8 borrows. It cannot return or carry a retained reference/callback.
        [[nodiscard]] static bool supportsCallScopedStringAbi(const wir::lowered::Module& module,
                                                              const wir::lowered::Function& function);

        [[nodiscard]] static WirCppStoragePlan build(const wir::lowered::Module& module,
                                                     const wir::lowered::Function& function);
    };
} // namespace wio::codegen
