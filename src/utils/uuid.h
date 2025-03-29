#pragma once

#include "buffer.h"

#include <random>
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>
#include <cstring>

namespace wio
{

    class uuid
    {
    public:
        uuid()
        {
            generate();
        }

        uuid(const uuid& other)
        {
            m_buffer.set(other.m_buffer.data, 16);
        }

        uuid& operator=(const uuid& other)
        {
            if (this != &other)
                m_buffer.set(other.m_buffer.data, 16);
            return *this;
        }

        uuid(const raw_buffer& buffer)
        {
            assert(buffer.size == 16 && "Invalid buffer size for UUID!");
            m_buffer.set(buffer.data, 16);
        }

        void generate()
        {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<uint64_t> dis;

            uint64_t part1 = dis(gen);
            uint64_t part2 = dis(gen);

            part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
            part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

            m_buffer.store<uint64_t>(part1);
            std::memcpy(m_buffer.data + 8, &part2, 8);
        }

        std::string to_string() const
        {
            const uint8_t* data = m_buffer.data;
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');

            for (size_t i = 0; i < 16; ++i)
            {
                if (i == 4 || i == 6 || i == 8 || i == 10)
                    oss << '-';
                oss << std::setw(2) << static_cast<int>(data[i]);
            }
            return oss.str();
        }

        raw_buffer buffer() const
        {
            return raw_buffer(m_buffer.data, 16);
        }

        bool operator==(const uuid& other) const
        {
            return std::memcmp(m_buffer.data, other.m_buffer.data, 16) == 0;
        }

        bool operator!=(const uuid& other) const
        {
            return !(*this == other);
        }

    private:
        stack_buffer<16> m_buffer;
    };
}

    namespace std
    {
        template <>
        struct hash<wio::uuid>
        {
            size_t operator()(const wio::uuid& id) const noexcept
            {
                const uint64_t* data64 = id.buffer().as<const uint64_t>();
                return std::hash<uint64_t>{}(data64[0]) ^ (std::hash<uint64_t>{}(data64[1]) << 1);
            }
        };
    }
