#pragma once

#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace byte_write_range
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
            throw std::runtime_error("Unable to prepare range-write payload");
        }
    }

    inline std::uint64_t CheckOutput()
    {
        std::ifstream file("output.bin", std::ios::binary);
        std::uint64_t checksum = 14695981039346656037ULL;
        for (std::uint32_t index = 7; index < 65526; ++index)
        {
            const int value = file.get();
            if (value != static_cast<int>((index * 17 + 3) & 255))
            {
                throw std::runtime_error("Range-write output bytes changed");
            }

            checksum = (checksum ^ static_cast<unsigned char>(value)) * 1099511628211ULL;
        }

        if (file.get() != std::char_traits<char>::eof() || file.bad())
        {
            throw std::runtime_error("Range-write output length changed");
        }

        return checksum;
    }
}
