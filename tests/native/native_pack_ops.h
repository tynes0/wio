#pragma once

#include <cstdint>

namespace native_pack
{
    template <typename... Args>
    inline std::int32_t CountTypes(Args...)
    {
        return static_cast<std::int32_t>(sizeof...(Args));
    }

    template <typename T, typename... Args>
    inline std::int32_t CountWithHead(T, Args...)
    {
        return static_cast<std::int32_t>(1 + sizeof...(Args));
    }
}
