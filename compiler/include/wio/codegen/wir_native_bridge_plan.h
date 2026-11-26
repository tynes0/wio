#pragma once

#include "wio/wir/lowered_ir.h"

#include <vector>

namespace wio::codegen
{
    // A private C++ emission view, never serialized as the VM program. IDs and
    // ABI identities remain those of the original canonical module.
    struct WirNativeBridgePlan
    {
        wir::lowered::Module module;
        std::vector<wir::TypeId> dynamicPayloadTypes;

        [[nodiscard]] static WirNativeBridgePlan build(const wir::lowered::Module& source);
    };
} // namespace wio::codegen
