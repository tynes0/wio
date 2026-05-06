#pragma once

#include <cstdint>
#include <string_view>

namespace native_cstring
{
    inline std::int32_t CountChars(const char* value)
    {
        return value ? static_cast<std::int32_t>(std::string_view(value).size()) : -1;
    }

    inline std::int32_t CountCharsView(const std::string_view value)
    {
        return static_cast<std::int32_t>(value.size());
    }

    inline bool StartsWithWio(const char* value)
    {
        return value != nullptr && std::string_view(value).starts_with("Wio");
    }
}
