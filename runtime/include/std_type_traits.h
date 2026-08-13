#pragma once

#include "type_reflection.h"

#include <array>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace wio::runtime::traits
{
    template <typename T>
    using Bare = std::remove_cv_t<std::remove_reference_t<T>>;

    template <typename T>
    inline constexpr bool IsStdVector = false;

    template <typename T, typename TAllocator>
    inline constexpr bool IsStdVector<std::vector<T, TAllocator>> = true;

    template <typename T>
    inline constexpr bool IsStdArray = false;

    template <typename T, std::size_t N>
    inline constexpr bool IsStdArray<std::array<T, N>> = true;

    template <typename T>
    inline constexpr bool IsStdDictionary = false;

    template <typename K, typename V, typename... Rest>
    inline constexpr bool IsStdDictionary<std::unordered_map<K, V, Rest...>> = true;

    template <typename K, typename V, typename... Rest>
    inline constexpr bool IsStdDictionary<std::map<K, V, Rest...>> = true;

    template <typename T>
    [[nodiscard]] constexpr bool IsIntegerValue() noexcept
    {
        return std::is_integral_v<Bare<T>> && !std::is_same_v<Bare<T>, bool>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsNumericValue() noexcept
    {
        return IsIntegerValue<T>() || std::is_floating_point_v<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsFloatingValue() noexcept
    {
        return std::is_floating_point_v<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsSignedValue() noexcept
    {
        return IsNumericValue<T>() && std::is_signed_v<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsUnsignedValue() noexcept
    {
        return IsIntegerValue<T>() && std::is_unsigned_v<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsArrayValue() noexcept
    {
        return IsStdVector<Bare<T>> || IsStdArray<Bare<T>> || std::is_array_v<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsPrimitiveValue() noexcept
    {
        return TypeReflection<Bare<T>>::Kind == ReflectedTypeKind::primitive_type;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsStringValue() noexcept
    {
        return std::is_same_v<Bare<T>, std::string>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsTextValue() noexcept
    {
        return std::is_same_v<Bare<T>, Text>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsDictionaryValue() noexcept
    {
        return IsStdDictionary<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsEnumValue() noexcept
    {
        return TypeReflection<Bare<T>>::Kind == ReflectedTypeKind::enum_type;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsFlagsetValue() noexcept
    {
        return TypeReflection<Bare<T>>::Kind == ReflectedTypeKind::flagset_type;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsObjectValue() noexcept
    {
        return TypeReflection<Bare<T>>::Kind == ReflectedTypeKind::object_type;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsComponentValue() noexcept
    {
        return TypeReflection<Bare<T>>::Kind == ReflectedTypeKind::component_type;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsInterfaceValue() noexcept
    {
        return TypeReflection<Bare<T>>::Kind == ReflectedTypeKind::interface_type;
    }

    template <typename Left, typename Right>
    [[nodiscard]] constexpr bool IsSameValue() noexcept
    {
        return std::is_same_v<Bare<Left>, Bare<Right>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsDefaultConstructibleValue() noexcept
    {
        return std::is_default_constructible_v<Bare<T>>;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsCopyConstructibleValue() noexcept
    {
        return std::is_copy_constructible_v<Bare<T>>;
    }
}
