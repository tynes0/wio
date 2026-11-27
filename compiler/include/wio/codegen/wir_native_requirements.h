#pragma once

#include "wio/wir/lowered_ir.h"

#include <string>
#include <vector>

namespace wio::codegen
{
    // Inspection identity of the existing native bridge plan, not a cache key.
    // Native file contents/toolchain/options are intentionally a separate layer.
    struct WirNativeRequirements final
    {
        std::string sha256;
        std::vector<std::string> nativeBindings;
        std::vector<std::string> retainedTypes;
        std::vector<std::string> dynamicAnyPayloads;
        std::vector<std::string> embeddedWioFunctions;
        std::vector<std::string> nativeImports;

        [[nodiscard]] static WirNativeRequirements build(const wir::lowered::Module& source);
        [[nodiscard]] std::string toJson() const;
    };
} // namespace wio::codegen
