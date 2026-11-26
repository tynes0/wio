#pragma once

#include "scalar_codegen_probe.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace builder_benchmark
{
    inline std::chrono::steady_clock::time_point started;

    inline void Begin()
    {
        scalar_codegen_probe::Begin();
        started = std::chrono::steady_clock::now();
    }

    inline int Report(const std::string& value)
    {
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        const auto allocations = scalar_codegen_probe::End();
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char character : value)
        {
            hash ^= character;
            hash *= 1099511628211ull;
        }
        std::printf("seconds=%.9f bytes=%zu hash=%llu allocations=%llu\n", elapsed, value.size(),
                    static_cast<unsigned long long>(hash), static_cast<unsigned long long>(allocations));
        return value.size() == 960000 ? 0 : 1;
    }
} // namespace builder_benchmark
