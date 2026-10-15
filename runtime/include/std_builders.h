#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace wio::runtime::std_builders
{
    inline void ReserveString(std::string& value, const std::size_t capacity)
    {
        value.reserve(capacity);
    }

    inline void AppendBytes(std::vector<std::uint8_t>& value, const std::string& source)
    {
        value.insert(value.end(), source.begin(), source.end());
    }

    inline std::string ReadBytes(const std::vector<std::uint8_t>& value, const std::size_t start,
                                 const std::size_t count)
    {
        if (start > value.size() || count > value.size() - start)
            throw std::out_of_range("ByteReader string range is outside the input");
        if (count == 0)
            return {};
        return {reinterpret_cast<const char*>(value.data() + start), count};
    }
} // namespace wio::runtime::std_builders
