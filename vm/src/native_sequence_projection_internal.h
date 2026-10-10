#pragma once

#include "native_sequence_internal.h"

namespace wio::vm::detail
{
    struct NativeSequenceProjection final
    {
        std::uint32_t type = bytecode::InvalidIndex;
        std::uint64_t typeId = 0;
        std::vector<std::uint64_t> indices;
    };

    // Physical cell identity only; no traversal into shared object/Any payloads.
    [[nodiscard]] bool findNativeSequenceProjection(const bytecode::Module& module, const Value& root,
                                                    std::uint32_t rootType, const Value& target,
                                                    const NativePodLayouts& layouts, NativeSequenceProjection& output);
} // namespace wio::vm::detail
