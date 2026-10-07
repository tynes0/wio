#pragma once

#include "native_bridge_internal.h"
#include <wio_native_pod.h>

#include <unordered_map>
#include <vector>

namespace wio::vm::detail
{
    struct NativePodFieldLayout
    {
        std::uint64_t typeId;
        std::uint64_t offset;
        std::uint64_t size;
        std::uint8_t kind;

        auto operator<=>(const NativePodFieldLayout&) const = default;
    };

    struct NativePodLayout
    {
        std::uint64_t typeId;
        std::uint64_t size;
        std::uint64_t alignment;
        WioNativePodPackFn pack;
        WioNativePodReadFn read;
        std::vector<NativePodFieldLayout> fields;

        bool operator==(const NativePodLayout&) const = default;
    };

    struct NativePodLayouts
    {
        std::unordered_map<std::uint32_t, NativePodLayout> types;
        std::unordered_map<std::uint32_t, WioNativePodDefaultFn> defaults;
    };

    // POD pointers are borrowed void* storage, optionally nullable. No optional,
    // object handle, reference token, or nested nullable wrapper is a POD pointer.
    [[nodiscard]] std::uint32_t nativePodPointerType(const bytecode::Module& module, std::uint32_t type) noexcept;

    // Merge transactionally into an unpublished snapshot. Descriptor strings
    // and arrays are not borrowed after validation; only adapter code is pinned.
    [[nodiscard]] bool mergeNativePodLayouts(const bytecode::Module& module, const WioNativePodLayoutApi& api,
                                             NativePodLayouts& layouts, std::string& error);
    [[nodiscard]] bool mergeNativePodDefaults(const bytecode::Module& module, const WioNativePodDefaultApi& api,
                                              NativePodLayouts& layouts, std::string& error);
    [[nodiscard]] NativeValueResult defaultNativePod(const bytecode::Module& module, std::uint32_t type,
                                                     const NativePodLayouts& layouts);
    [[nodiscard]] bool packNativePod(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                     const NativePodLayouts& layouts, WioNativeAbiValue& output, std::string& error,
                                     unsigned depth);
    [[nodiscard]] NativeValueResult unpackNativePod(const bytecode::Module& module, const WioNativeAbiValue& value,
                                                    std::uint32_t type, const NativePodLayouts& layouts,
                                                    unsigned depth);
} // namespace wio::vm::detail
