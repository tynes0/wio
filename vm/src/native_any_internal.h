#pragma once

#include "native_pod_internal.h"

namespace wio::vm::detail
{
    [[nodiscard]] bool mergeNativeAny(const bytecode::Module& module, const WioNativeAnyApi& api,
                                      NativePodLayouts& layouts, std::string& error);
    [[nodiscard]] bool packNativeAny(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                     const NativePodLayouts& layouts, WioNativeAbiValue& output, std::string& error,
                                     unsigned depth);
    [[nodiscard]] NativeValueResult unpackNativeAny(const bytecode::Module& module, const WioNativeAbiValue& value,
                                                    std::uint32_t type, const NativePodLayouts& layouts,
                                                    unsigned depth);
} // namespace wio::vm::detail
