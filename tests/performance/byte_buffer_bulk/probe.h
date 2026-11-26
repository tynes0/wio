#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace byte_buffer_bulk
{
    inline std::vector<std::uint8_t> Make()
    {
        std::vector<std::uint8_t> result(1024);
        for (std::size_t index = 0; index < result.size(); ++index)
        {
            result[index] = static_cast<std::uint8_t>((index * 17 + 3) & 255);
        }

        return result;
    }

    inline std::uint64_t Check(const std::vector<std::uint8_t>& bytes)
    {
        if (bytes.size() != 1536)
        {
            throw std::runtime_error("ByteBuffer bulk output length changed");
        }

        std::uint64_t checksum = 14695981039346656037ULL;
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            if (bytes[index] != ((index * 17 + 3) & 255))
            {
                throw std::runtime_error("ByteBuffer bulk output bytes changed");
            }

            checksum = (checksum ^ bytes[index]) * 1099511628211ULL;
        }

        return checksum;
    }
}
