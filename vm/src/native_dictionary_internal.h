#pragma once

#include "native_sequence_internal.h"

namespace wio::vm::detail
{
    [[nodiscard]] bool supportedNativeDictionaryKey(const bytecode::Module& module, std::uint32_t type);
    [[nodiscard]] bool mergeNativeDictionaries(const bytecode::Module& module, const WioNativeDictionaryApi& api,
                                               NativePodLayouts& layouts, std::string& error,
                                               bool validateElements = true);
    [[nodiscard]] bool packNativeDictionary(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                            const NativePodLayouts& layouts, WioNativeAbiValue& output,
                                            std::string& error, unsigned depth);
    [[nodiscard]] NativeValueResult unpackNativeDictionary(const bytecode::Module& module,
                                                           const WioNativeAbiValue& value, std::uint32_t type,
                                                           const NativePodLayouts& layouts, unsigned depth);
    // Match by key, never by unordered producer iteration order. Does not move
    // or mutate either container; output is published only for a full match.
    [[nodiscard]] bool nativeDictionaryCopyBackOrder(const Value& target, const Value& replacement,
                                                     std::vector<std::size_t>& order);
} // namespace wio::vm::detail
