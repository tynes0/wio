#pragma once

#include "wio/wir/lowered_ir.h"

#include <cstddef>

namespace wio::wir
{
    struct OptimizationStatistics
    {
        std::size_t constantsFolded = 0;
        std::size_t valuesPropagated = 0;
        std::size_t branchesSimplified = 0;
        std::size_t blocksRemoved = 0;
        std::size_t instructionsRemoved = 0;
        std::size_t stackAllocations = 0;
        std::size_t heapAllocations = 0;
        std::size_t coroutineFrameAllocations = 0;
        std::size_t boundsChecksEliminated = 0;

        auto operator<=>(const OptimizationStatistics&) const = default;
    };

    // Runs only semantics-preserving, backend-independent transformations.
    // Every pass keeps stable function/block/value ids and source spans; ids
    // are intentionally not compacted so diagnostics and differential backend
    // traces continue to identify the same source values.
    class CanonicalOptimizer final
    {
    public:
        [[nodiscard]] OptimizationStatistics optimize(lowered::Module& module) const;
    };
}
