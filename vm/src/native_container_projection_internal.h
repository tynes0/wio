#pragma once

#include "native_sequence_internal.h"

namespace wio::vm::detail
{
    struct NativeContainerProjectionStep final
    {
        std::uint32_t kind = WIO_NATIVE_ABI_CONTAINER_INDEX;
        std::uint64_t containerTypeId = 0;
        std::uint64_t index = 0;
        std::uint32_t keyType = bytecode::InvalidIndex;
        const Value* key = nullptr;
    };

    struct NativeContainerProjection final
    {
        std::uint32_t type = bytecode::InvalidIndex;
        std::uint64_t typeId = 0;
        std::vector<NativeContainerProjectionStep> steps;
        std::uint64_t podTypeId = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    // Traverses physical value cells only, not shared object/Any payloads or map keys.
    [[nodiscard]] bool findNativeContainerProjection(const bytecode::Module& module, const Value& root,
                                                     std::uint32_t rootType, const Value& target,
                                                     const NativePodLayouts& layouts,
                                                     NativeContainerProjection& output);
} // namespace wio::vm::detail
