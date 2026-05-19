#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "exception.h"

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

    template <typename T>
    [[nodiscard]] T EnumValue(const std::size_t index)
    {
        if (index >= EnumReflection<T>::Count)
            throw RuntimeException("Enum reflection index is out of range.");
        return EnumReflection<T>::Value(index);
    }

    template <typename T>
    [[nodiscard]] std::ptrdiff_t EnumIndex(const T value) noexcept
    {
        return EnumReflection<T>::Index(value);
    }

    template <typename T>
    [[nodiscard]] std::string EnumUnderlyingTypeName()
    {
        return std::string(EnumReflection<T>::UnderlyingTypeName);
    }

    template <typename T>
    [[nodiscard]] constexpr std::size_t EnumSize() noexcept
    {
        return EnumReflection<T>::Size;
    }
}
