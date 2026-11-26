#pragma once

#include <charconv>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <text.h>

#ifdef WIO_APPEND_MEMORY
#include "memory_probe.h"
#endif

namespace append_probe
{
    inline std::chrono::steady_clock::time_point started;

    inline void Begin()
    {
#ifdef WIO_APPEND_MEMORY
        memory_costs::Begin();
#endif
        started = std::chrono::steady_clock::now();
    }

    inline void Live()
    {
#ifdef WIO_APPEND_MEMORY
        memory_costs::Checkpoint();
#endif
    }

    inline int End(const std::uint64_t checksum)
    {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
#ifdef WIO_APPEND_MEMORY
        memory_costs::Report(checksum);
#endif
        std::printf("seconds=%.9f checksum=%llu\n", seconds, static_cast<unsigned long long>(checksum));
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
} // namespace append_probe
