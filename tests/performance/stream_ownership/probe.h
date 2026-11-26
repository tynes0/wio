#pragma once

#ifndef STREAM_OWNERSHIP_REFERENCE
#include "format.h"
#endif

#include <atomic>
#include <cstdint>
#include <string>

namespace stream_ownership
{
    inline std::string Payload(const std::int64_t mode)
    {
        std::string result(mode == 0 ? 192 : 8192, 'a');
        for (std::size_t index = 0; index < result.size(); ++index)
            result[index] = index % 31 == 0 ? '\0' : static_cast<char>('a' + index % 26);
        return result;
    }

#ifndef STREAM_OWNERSHIP_REFERENCE
    inline std::string Row(const std::string& payload, const std::int64_t index)
    {
        return wio::runtime::Format("payload={};index={};ok={}", payload, index, index % 2 == 0);
    }
#endif

    inline std::uint64_t Hash(const std::string& value)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        std::uint64_t result = 14695981039346656037ULL;
        for (const unsigned char byte : value)
            result = (result ^ byte) * 1099511628211ULL;
        return result;
    }
} // namespace stream_ownership
