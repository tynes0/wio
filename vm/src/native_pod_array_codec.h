#pragma once

#include "native_pod_internal.h"

#include <span>

namespace wio::vm::detail
{
    // Only the fixed-array fields of registered native PODs use this codec.
    // Ordinary native container arguments retain their independent contract.
    class NativePodArrayCodec final
    {
    public:
        [[nodiscard]] static bool byteSize(const bytecode::Module& module, std::uint32_t type,
                                           const NativePodLayouts& layouts, std::size_t& size, std::string& error,
                                           unsigned depth = 0);
        [[nodiscard]] static bool validateByteSize(const bytecode::Module& module, std::uint32_t type,
                                                   const NativePodLayouts& layouts, std::size_t size,
                                                   std::string& error, unsigned depth = 0);
        [[nodiscard]] static bool pack(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                       const NativePodLayouts& layouts, std::span<std::byte> bytes, std::string& error,
                                       unsigned depth = 0);
        [[nodiscard]] static NativeValueResult unpack(const bytecode::Module& module, std::span<const std::byte> bytes,
                                                      std::uint32_t type, const NativePodLayouts& layouts,
                                                      unsigned depth = 0);
    };
} // namespace wio::vm::detail
