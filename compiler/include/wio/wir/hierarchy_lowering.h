#pragma once
#include "wio/wir/lowered_ir.h"
#include <string>
#include <vector>

namespace wio::wir
{
    // Pins transitive cast identities and per-contract implementations.
    // Returns errors instead of accepting cyclic or incomplete hierarchies.
    [[nodiscard]] std::vector<std::string> lowerHierarchy(lowered::Module& module);
}
