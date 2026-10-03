#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/value.h"

#include <wio_native_abi.h>

#include <cstdint>
#include <string>

namespace wio::vm::detail
{
    struct NativeValueResult
    {
        Value value;
        std::string error;

        [[nodiscard]] bool succeeded() const noexcept
        {
            return error.empty();
        }
    };

    [[nodiscard]] bool encodeNativeValue(const bytecode::Module& module, const Value& value, std::uint32_t declaredType,
                                         std::uint8_t marshalling, WioNativeAbiValue& output, std::string& error);
    [[nodiscard]] NativeValueResult decodeNativeValue(const bytecode::Module& module, const WioNativeAbiValue& value,
                                                      std::uint32_t declaredType, std::uint8_t marshalling);
    [[nodiscard]] std::string nativeFailureMessage(const WioNativeAbiFailure& failure);
} // namespace wio::vm::detail
