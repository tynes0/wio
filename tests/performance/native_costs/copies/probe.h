#pragma once

#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include <text.h>

namespace copy_costs
{
    inline std::atomic<bool> active{false};
    inline std::atomic<std::uint64_t> copyCalls{0};
    inline std::atomic<std::uint64_t> copyBytes{0};
    inline std::atomic<std::uint64_t> moveCalls{0};
    inline std::atomic<std::uint64_t> moveBytes{0};

    inline void Begin()
    {
#ifdef WIO_COPY_DIAGNOSTIC
        active.store(true, std::memory_order_relaxed);
#endif
    }

    inline int Report(const std::uint64_t checksum)
    {
        active.store(false, std::memory_order_relaxed);
        std::printf("{\"Checksum\":\"%llu\",", static_cast<unsigned long long>(checksum));
#ifdef WIO_COPY_DIAGNOSTIC
        std::printf("\"CountersEnabled\":true,\"MemcpyCalls\":%llu,\"MemcpyBytes\":%llu,"
                    "\"MemmoveCalls\":%llu,\"MemmoveBytes\":%llu}\n",
                    static_cast<unsigned long long>(copyCalls.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(copyBytes.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(moveCalls.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(moveBytes.load(std::memory_order_relaxed)));
#else
        std::printf("\"CountersEnabled\":false,\"MemcpyCalls\":null,\"MemcpyBytes\":null,"
                    "\"MemmoveCalls\":null,\"MemmoveBytes\":null}\n");
#endif
        return 0;
    }

    inline std::int32_t ParseCount(const std::string& value)
    {
        std::int32_t result = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < 1 || result > 100000)
        {
            throw std::invalid_argument("count must be in [1,100000]");
        }

        return result;
    }

    inline std::uint64_t Hash(const std::string& value)
    {
        std::uint64_t result = 14695981039346656037ULL;
        for (const unsigned char byte : value)
        {
            result = (result ^ byte) * 1099511628211ULL;
        }

        return result;
    }

    inline std::uint64_t HashText(const wio::runtime::Text& value)
    {
        return Hash(value.Utf8());
    }

    // Volatile function-pointer loads guarantee actual calls in the calibration
    // in both variants. The observed wrappers forward to the real C routines.
    inline void* (*volatile copyFunction)(void*, const void*, std::size_t) = std::memcpy;
    inline void* (*volatile moveFunction)(void*, const void*, std::size_t) = std::memmove;

    inline std::uint64_t Control(const std::int32_t count)
    {
        unsigned char source[32];
        unsigned char destination[32];
        for (std::size_t index = 0; index < 32; ++index)
        {
            static_cast<volatile unsigned char*>(source)[index] = static_cast<unsigned char>(index);
            static_cast<volatile unsigned char*>(destination)[index] = 0;
        }

        for (std::int32_t index = 0; index < count; ++index)
        {
            if (copyFunction(destination, source, 7) != destination ||
                copyFunction(destination, source, 0) != destination ||
                moveFunction(destination + 2, destination, 11) != destination + 2 ||
                moveFunction(destination, destination + 3, 5) != destination)
            {
                throw std::logic_error("copy wrapper return value mismatch");
            }
        }

        std::uint64_t result = 14695981039346656037ULL;
        for (const unsigned char byte : destination)
        {
            result = (result ^ byte) * 1099511628211ULL;
        }

        return result;
    }
} // namespace copy_costs

// GNU linker wrapping affects unresolved references, not all memory transfers.
// Never link this diagnostic into production or other benchmark probes.
#ifdef WIO_COPY_DIAGNOSTIC
extern "C" void* __real_memcpy(void*, const void*, std::size_t);
extern "C" void* __real_memmove(void*, const void*, std::size_t);

extern "C" void* __wrap_memcpy(void* destination, const void* source, const std::size_t size)
{
    if (copy_costs::active.load(std::memory_order_relaxed))
    {
        copy_costs::copyCalls.fetch_add(1, std::memory_order_relaxed);
        copy_costs::copyBytes.fetch_add(size, std::memory_order_relaxed);
    }

    return __real_memcpy(destination, source, size);
}

extern "C" void* __wrap_memmove(void* destination, const void* source, const std::size_t size)
{
    if (copy_costs::active.load(std::memory_order_relaxed))
    {
        copy_costs::moveCalls.fetch_add(1, std::memory_order_relaxed);
        copy_costs::moveBytes.fetch_add(size, std::memory_order_relaxed);
    }

    return __real_memmove(destination, source, size);
}
#endif
