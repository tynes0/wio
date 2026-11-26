#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace borrow_probe
{
    inline std::size_t Read(const std::string& value)
    {
        return value.size();
    }

    inline std::size_t Grow(std::string& value)
    {
        value.append("-expanded:012345678901234567890123456789");
        value.push_back('\0');
        return value.size();
    }

    inline std::size_t ReadOffset(const std::string& value, const std::size_t offset)
    {
        return value.size() + offset;
    }

    inline std::size_t Fail(const std::string&)
    {
        throw std::runtime_error("borrow-probe failure");
    }
} // namespace borrow_probe
