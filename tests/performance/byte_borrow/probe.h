#pragma once

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

#if defined(WIO_BENCHMARK_ALLOCATIONS)
#include "wio_benchmark_allocations.h"
#endif

namespace byte_borrow
{
    inline std::vector<std::uint8_t> Make()
    {
        return {3, 0, 255, 128, 7, 42, 99, 1};
    }

    inline std::uint64_t Hash(const std::vector<std::uint8_t>& bytes)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        std::uint64_t value = 14695981039346656037ULL;
        for (const auto byte : bytes)
        {
            value = (value ^ byte) * 1099511628211ULL;
        }

        return value;
    }

    inline bool Same(const std::vector<std::uint8_t>& left, const std::vector<std::uint8_t>& right)
    {
        return &left == &right;
    }

    inline std::uint64_t Throw(const std::vector<std::uint8_t>&)
    {
        throw std::runtime_error("byte-borrow-failure");
    }

    inline void Start()
    {
#if defined(WIO_BENCHMARK_ALLOCATIONS)
        if (!wio::runtime::benchmark::BeginAllocations())
            throw std::runtime_error("allocation window unavailable");
#endif
    }

    inline bool Finish(const std::uint64_t allocations, const std::uint64_t requested)
    {
#if defined(WIO_BENCHMARK_ALLOCATIONS)
        const auto value = wio::runtime::benchmark::EndAllocations();
        return value.allocations == allocations && value.frees == allocations && value.requestedBytes == requested &&
               value.freedBytes == requested && value.liveBytes == 0 && value.liveBlocks == 0 && value.failures == 0;
#else
        return false;
#endif
    }
} // namespace byte_borrow
