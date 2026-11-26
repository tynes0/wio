#pragma once

#include <atomic>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

namespace generated_stream
{
    inline int calls = 0;
    inline int mode = 0;

    inline std::string MakePayload(const std::int64_t size)
    {
        std::string result(static_cast<std::size_t>(size), 'a');
        for (std::size_t index = 0; index < result.size(); ++index)
            result[index] = index % 31 == 0 ? '\0' : static_cast<char>('a' + index % 26);
        return result;
    }

    inline const std::string& Payload(const std::int64_t size)
    {
        static const auto small = MakePayload(192);
        static const auto large = MakePayload(8192);
        return size == 192 ? small : large;
    }

    inline void Reset(const int value)
    {
        calls = 0;
        mode = value;
    }

    inline int Calls()
    {
        return calls;
    }

    struct Label
    {
        std::int64_t size;
        std::int64_t index;
    };

    inline std::ostream& operator<<(std::ostream& stream, const Label& value)
    {
        ++calls;
        if (value.size == 192 || value.size == 8192)
            stream << "payload=" << Payload(value.size);
        else
            stream << "payload=" << MakePayload(value.size);
        stream << ";index=" << value.index << ";ok=" << (value.index % 2 == 0 ? "true" : "false");
        if (mode == 1)
        {
            stream.seekp(3);
            stream << "XYZ";
        }
        if (mode == 2)
            stream.setstate(std::ios::badbit);
        if (mode == 3)
            throw std::runtime_error("generated stream hook failure");
        return stream;
    }

    inline std::uint64_t Hash(const std::string& value)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        std::uint64_t result = 14695981039346656037ULL;
        for (const unsigned char byte : value)
            result = (result ^ byte) * 1099511628211ULL;
        return result;
    }
} // namespace generated_stream
