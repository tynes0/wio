#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/value.h"

#include <wio_native_abi.h>

#include <cstdint>
#include <memory>
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
        // Optional invocation-local lifetime hooks for materialized callback cells.
        void (*pinValue)(void*, const Value&) = nullptr;
        void (*pinWire)(void*, WioNativeAbiValue&) = nullptr;
        bool (*cleanup)(void*, Value&, std::string&) noexcept = nullptr;
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
    // one VM-owned storage claim to the native caller (wire-v2 owner). POD pack
    // adapters are used only during conversion; returned bytes do not pin them.
    [[nodiscard]] bool encodeOwnedNativeValue(const bytecode::Module& module, const Value& value,
                                              std::uint32_t declaredType, std::uint8_t marshalling,
                                              WioNativeAbiValue& output, std::string& error,
                                              const NativePodLayouts* layouts = nullptr,
                                              const NativeObjectAccess* objects = nullptr);
    // Pins adapter code until an escaped owning callback result is released,
    // preserving any pre-existing wire owner. Manual registries still require
    // the host to keep their producer code loaded.
    void pinNativeCallbackResult(WioNativeAbiValue& value, std::shared_ptr<void> codeLease,
                                 const std::shared_ptr<void>& scope);
    // A live machine can drain its own leases while its loader still pins code.
    // Without a scope, call only at a host safe point after producer calls return.
    void collectNativeCallbackResultLeases(const std::shared_ptr<void>& scope = {});
    [[nodiscard]] bool callbackValueMarshalling(const bytecode::Module& module, std::uint32_t declaredType,
                                                std::uint8_t& marshalling,
                                                const NativePodLayouts* layouts = nullptr) noexcept;
    // Arguments may borrow any qualified non-void value cell for this invocation.
    // Results deliberately retain the stricter by-value contract.
    [[nodiscard]] bool callbackArgumentMarshalling(const bytecode::Module& module, std::uint32_t declaredType,
                                                   std::uint8_t& marshalling,
                                                   const NativePodLayouts* layouts = nullptr) noexcept;
    [[nodiscard]] bool nativeValueMarshalling(const bytecode::Module& module, std::uint32_t declaredType,
                                              std::uint8_t& marshalling) noexcept;
    [[nodiscard]] std::string nativeFailureMessage(const WioNativeAbiFailure& failure);
} // namespace wio::vm::detail
