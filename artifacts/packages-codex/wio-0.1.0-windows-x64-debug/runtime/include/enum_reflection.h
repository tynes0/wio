#pragma once

#include <cstddef>
#include <string>

namespace wio::runtime
{
    template <typename T>
    struct EnumReflection;

    template <typename T>
    [[nodiscard]] constexpr std::size_t EnumCount() noexcept
    {
        return EnumReflection<T>::Count;
    }

    template <typename T>
    [[nodiscard]] std::string EnumName(const T value)
    {
        return EnumReflection<T>::Name(value);
    }
}
