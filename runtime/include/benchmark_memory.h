#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace wio::runtime::benchmark
{
    struct AllocationSnapshot
    {
        std::uint64_t allocations = 0;
        std::uint64_t frees = 0;
        std::uint64_t requestedBytes = 0;
        std::uint64_t freedBytes = 0;
        std::uint64_t liveBytes = 0;
        std::uint64_t peakBytes = 0;
        std::uint64_t liveBlocks = 0;
        std::uint64_t peakBlocks = 0;
        std::uint64_t failures = 0;
        bool available = false;
    };

    namespace allocation_detail
    {
        inline std::atomic_flag gate = ATOMIC_FLAG_INIT;
        inline AllocationSnapshot counters;
        inline bool active = false;
        inline std::uint64_t epoch = 0;

        struct Lock
        {
            Lock() noexcept
            {
                while (gate.test_and_set(std::memory_order_acquire))
                {
                }
            }

            ~Lock()
            {
                gate.clear(std::memory_order_release);
            }
        };
    } // namespace allocation_detail

    inline bool AllocationAvailable() noexcept
    {
#ifdef WIO_BENCHMARK_ALLOCATIONS
        return true;
#else
        return false;
#endif
    }

    inline bool BeginAllocations() noexcept
    {
        using namespace allocation_detail;
        const Lock lock;
        if (!AllocationAvailable() || active || counters.liveBlocks ||
            epoch == std::numeric_limits<std::uint64_t>::max())
            return false;
        ++epoch;
        counters = {};
        counters.available = true;
        active = true;
        return true;
    }

    inline AllocationSnapshot Allocations() noexcept
    {
        const allocation_detail::Lock lock;
        auto result = allocation_detail::counters;
        result.available = AllocationAvailable();
        return result;
    }

    inline AllocationSnapshot EndAllocations() noexcept
    {
        const allocation_detail::Lock lock;
        allocation_detail::active = false;
        auto result = allocation_detail::counters;
        result.available = AllocationAvailable();
        return result;
    }
} // namespace wio::runtime::benchmark
