#pragma once

#include "native_pod_internal.h"

namespace wio::vm::detail
{
    [[nodiscard]] bool mergeNativeSequences(const bytecode::Module& module, const WioNativeSequenceApi& api,
                                            NativePodLayouts& layouts, std::string& error,
                                            bool validateElements = true);
    [[nodiscard]] bool supportedNativeContainerValue(const bytecode::Module& module, std::uint32_t type,
                                                     const NativePodLayouts& layouts);
    [[nodiscard]] bool validateNativeContainers(const bytecode::Module& module, const NativePodLayouts& layouts,
                                                std::string& error);
    [[nodiscard]] bool validNativeContainerWire(const WioNativeAbiValue& value, std::uint64_t typeId);
    [[nodiscard]] bool packNativeSequence(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                          const NativePodLayouts& layouts, WioNativeAbiValue& output,
                                          std::string& error, unsigned depth,
                                          const NativeObjectAccess* objects = nullptr);
    [[nodiscard]] NativeValueResult unpackNativeSequence(const bytecode::Module& module, const WioNativeAbiValue& value,
                                                         std::uint32_t type, const NativePodLayouts& layouts,
                                                         unsigned depth, const NativeObjectAccess* objects = nullptr);
    // Recurses through sequence/component cells by storage identity. Container
    // element projections have no producer-side address contract yet.
    [[nodiscard]] bool containsNativeSequencePlace(const Value& root, const Value& target, unsigned depth = 0);
    [[nodiscard]] bool compatibleNativeSequenceCopyBack(const Value& target, const Value& replacement,
                                                        unsigned depth = 0);
} // namespace wio::vm::detail
