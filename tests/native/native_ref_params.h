#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace native_refs
{
    inline void Increment(std::int32_t& value)
    {
        value += 5;
    }

    inline std::int32_t ReadView(const std::int32_t& value)
    {
        return value + 1;
    }

    inline void AppendBang(std::string& value)
    {
        value += "!";
    }

    inline bool StartsWithHi(const std::string_view value)
    {
        return value.starts_with("Hi");
    }

    inline void PushValue(std::vector<std::int32_t>& values, const std::int32_t value)
    {
        values.push_back(value);
    }

    inline std::int32_t LastPlusOne(const std::vector<std::int32_t>& values)
    {
        return values.empty() ? -1 : values.back() + 1;
    }
}
