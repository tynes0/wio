#pragma once

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>

namespace native_costs
{
    inline std::atomic<bool> counting{false};
    inline std::atomic<std::uint64_t> allocations{0};
    inline std::atomic<std::uint64_t> requestedBytes{0};
    inline std::chrono::steady_clock::time_point started;

    inline void Begin()
    {
        allocations.store(0, std::memory_order_relaxed);
        requestedBytes.store(0, std::memory_order_relaxed);
#ifdef WIO_COST_NO_COUNTERS
        counting.store(false, std::memory_order_relaxed);
#else
        counting.store(true, std::memory_order_relaxed);
#endif
        started = std::chrono::steady_clock::now();
    }

    inline void Record(const std::size_t size)
    {
#ifdef WIO_COST_NO_COUNTERS
        (void)size;
#else
        if (counting.load(std::memory_order_relaxed))
        {
            allocations.fetch_add(1, std::memory_order_relaxed);
            requestedBytes.fetch_add(size, std::memory_order_relaxed);
        }
#endif
    }

    inline std::uint64_t Hash(const std::string& value)
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const unsigned char byte : value)
        {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }

        return hash;
    }

    inline std::int32_t ParseCount(const std::string& value)
    {
        std::int32_t count = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || count < 1 || count > 100000)
            throw std::invalid_argument("count must be in [1, 100000]");
        return count;
    }

    // Force a genuine call in both languages; this is not an inline arithmetic reference.
#if defined(_MSC_VER)
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    inline std::uint64_t
    Step(const std::uint64_t value)
    {
        return value * 1664525ULL + 1013904223ULL;
    }

    inline int Report(const std::uint64_t checksum)
    {
        const auto finished = std::chrono::steady_clock::now();
        counting.store(false, std::memory_order_relaxed);
        const double seconds = std::chrono::duration<double>(finished - started).count();
        std::printf("seconds=%.9f checksum=%llu allocations=%llu requested_bytes=%llu\n", seconds,
                    static_cast<unsigned long long>(checksum),
                    static_cast<unsigned long long>(allocations.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(requestedBytes.load(std::memory_order_relaxed)));
        return 0;
    }
} // namespace native_costs

// Diagnostic executable only. Covers ordinary throwing new/new[], not malloc,
// aligned allocation, live bytes, frees or process RSS. Never link into production.
#ifndef WIO_COST_NO_COUNTERS
void* operator new(const std::size_t size)
{
    if (void* value = std::malloc(size ? size : 1))
    {
        native_costs::Record(size);
        return value;
    }

    throw std::bad_alloc{};
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* value) noexcept
{
    std::free(value);
}

void operator delete[](void* value) noexcept
{
    ::operator delete(value);
}

void operator delete(void* value, std::size_t) noexcept
{
    ::operator delete(value);
}

void operator delete[](void* value, std::size_t) noexcept
{
    ::operator delete(value);
}
#endif
