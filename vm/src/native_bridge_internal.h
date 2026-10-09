#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/value.h"

#include <wio_native_abi.h>

#include <cstdint>
#include <string>

namespace wio::vm::detail
{
    struct NativePodLayouts;
    struct NativeValueResult
    {
        Value value;
        std::string error;

        [[nodiscard]] bool succeeded() const noexcept
        {
            return error.empty();
        }
    };

    // Call-local access to hosted objects. Never retained by a producer or
    // installed in shared layouts; Any payload conversion uses this machine.
    struct NativeObjectAccess
    {
        void* state;
        bool (*encode)(void*, const Value&, std::uint32_t, WioNativeAbiValue&, std::string&, const NativePodLayouts&);
        NativeValueResult (*decode)(void*, const WioNativeAbiValue&, std::uint32_t, const NativePodLayouts&);
    };

    [[nodiscard]] bool encodeNativeValue(const bytecode::Module& module, const Value& value, std::uint32_t declaredType,
                                         std::uint8_t marshalling, WioNativeAbiValue& output, std::string& error,
                                         const NativePodLayouts* layouts = nullptr, unsigned depth = 0,
                                         const NativeObjectAccess* objects = nullptr);
    [[nodiscard]] NativeValueResult decodeNativeValue(const bytecode::Module& module, const WioNativeAbiValue& value,
                                                      std::uint32_t declaredType, std::uint8_t marshalling,
                                                      const NativePodLayouts* layouts = nullptr, unsigned depth = 0,
                                                      const NativeObjectAccess* objects = nullptr);
    // Slice results must outlive the callback's ExecutionResult. This transfers
    // one producer-owned storage claim to the native caller (wire-v2 owner).
    [[nodiscard]] bool encodeOwnedNativeValue(const bytecode::Module& module, const Value& value,
                                              std::uint32_t declaredType, std::uint8_t marshalling,
                                              WioNativeAbiValue& output, std::string& error);
    [[nodiscard]] bool callbackValueMarshalling(const bytecode::Module& module, std::uint32_t declaredType,
                                                std::uint8_t& marshalling) noexcept;
    [[nodiscard]] bool nativeValueMarshalling(const bytecode::Module& module, std::uint32_t declaredType,
                                              std::uint8_t& marshalling) noexcept;
    [[nodiscard]] std::string nativeFailureMessage(const WioNativeAbiFailure& failure);
} // namespace wio::vm::detail
