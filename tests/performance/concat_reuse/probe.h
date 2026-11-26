#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <text.h>

namespace concat_probe
{
    inline std::uint64_t Hash(const std::string& value)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
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
} // namespace concat_probe
