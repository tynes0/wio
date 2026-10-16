#pragma once

#include "wio/wir/lowered_ir.h"

#include <cstddef>

namespace wio::wir
{
    // Elides only sole-use, adjacent, empty-affix primitive formatting into
    // mutable string Append. Arbitrary hooks, containers and frames stay intact.
    class NumericAppendOptimizer final
    {
    public:
        [[nodiscard]] static std::size_t optimize(lowered::Module& module);
    };
} // namespace wio::wir
