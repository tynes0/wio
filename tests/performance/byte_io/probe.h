#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace byte_io
{
    inline void Setup()
    {
        std::ofstream file("payload.bin", std::ios::binary | std::ios::trunc);
        for (std::uint32_t index = 0; index < 65536; ++index)
        {
            file.put(static_cast<char>((index * 17 + 3) & 255));
        }

        file.close();
        if (!file)
        {
            throw std::runtime_error("Unable to prepare byte I/O payload");
        }
    }

    inline std::uint64_t Hash(const std::vector<std::uint8_t>& bytes)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        std::uint64_t checksum = 14695981039346656037ULL;
        for (const auto value : bytes)
        {
            checksum = (checksum ^ value) * 1099511628211ULL;
        }

        return checksum;
    }
}
