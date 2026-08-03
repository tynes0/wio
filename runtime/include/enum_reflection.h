#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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

    template <typename T>
    [[nodiscard]] constexpr std::underlying_type_t<T> EnumRawValue(const T value) noexcept
    {
        return static_cast<std::underlying_type_t<T>>(value);
    }

    template <typename T>
    [[nodiscard]] bool EnumIsValid(const T value) noexcept
    {
        return EnumReflection<T>::Index(value) >= 0;
    }

    template <typename T, typename TRaw>
    [[nodiscard]] bool EnumTryFromRaw(const TRaw raw, T& value) noexcept
    {
        static_assert(std::is_enum_v<T> && std::is_integral_v<TRaw>);
        using Underlying = std::underlying_type_t<T>;
        if (!std::in_range<Underlying>(raw))
            return false;
        const T candidate = static_cast<T>(static_cast<Underlying>(raw));
        if (!EnumIsValid(candidate))
            return false;
        value = candidate;
        return true;
    }

    template <typename T, typename TRaw>
    [[nodiscard]] T EnumFromRaw(const TRaw raw)
    {
        T value{};
        if (!EnumTryFromRaw(raw, value))
            throw RuntimeException("Enum value is outside the declared value set.");
        return value;
    }
}
