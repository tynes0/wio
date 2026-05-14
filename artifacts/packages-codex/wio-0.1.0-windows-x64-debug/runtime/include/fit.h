#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

namespace wio
{
    template <std::integral Dest, std::integral Src>
    constexpr Dest FitNumeric(Src value)
    {
        if constexpr (std::same_as<Dest, Src>)
        {
            return value;
        }
        else if constexpr (std::signed_integral<Src> && std::unsigned_integral<Dest>)
        {
            if (value <= 0)
                return static_cast<Dest>(0);

            using UnsignedSrc = std::make_unsigned_t<Src>;
            using CompareType = std::common_type_t<UnsignedSrc, Dest>;
            CompareType convertedValue = static_cast<CompareType>(static_cast<UnsignedSrc>(value));
            CompareType maxValue = static_cast<CompareType>(std::numeric_limits<Dest>::max());
            return static_cast<Dest>(convertedValue > maxValue ? maxValue : convertedValue);
        }
        else if constexpr (std::unsigned_integral<Src> && std::signed_integral<Dest>)
        {
            using UnsignedDest = std::make_unsigned_t<Dest>;
            using CompareType = std::common_type_t<Src, UnsignedDest>;
            CompareType convertedValue = static_cast<CompareType>(value);
            CompareType maxValue = static_cast<CompareType>(std::numeric_limits<Dest>::max());
            return static_cast<Dest>(convertedValue > maxValue ? maxValue : convertedValue);
        }
        else
        {
            using CompareType = std::common_type_t<Src, Dest>;
            CompareType convertedValue = static_cast<CompareType>(value);
            CompareType minValue = static_cast<CompareType>(std::numeric_limits<Dest>::lowest());
            CompareType maxValue = static_cast<CompareType>(std::numeric_limits<Dest>::max());

            if (convertedValue < minValue)
                return static_cast<Dest>(minValue);
            if (convertedValue > maxValue)
                return static_cast<Dest>(maxValue);
            return static_cast<Dest>(convertedValue);
        }
    }

    template <std::floating_point Dest, std::integral Src>
    constexpr Dest FitNumeric(Src value)
    {
        return static_cast<Dest>(value);
    }

    template <std::integral Dest, std::floating_point Src>
    constexpr Dest FitNumeric(Src value)
    {
        long double convertedValue = static_cast<long double>(value);
        long double minValue = static_cast<long double>(std::numeric_limits<Dest>::lowest());
        long double maxValue = static_cast<long double>(std::numeric_limits<Dest>::max());

        if (convertedValue < minValue)
            return static_cast<Dest>(std::numeric_limits<Dest>::lowest());
        if (convertedValue > maxValue)
            return static_cast<Dest>(std::numeric_limits<Dest>::max());
        return static_cast<Dest>(value);
    }

    template <std::floating_point Dest, std::floating_point Src>
    constexpr Dest FitNumeric(Src value)
    {
        long double convertedValue = static_cast<long double>(value);
        long double minValue = static_cast<long double>(std::numeric_limits<Dest>::lowest());
        long double maxValue = static_cast<long double>(std::numeric_limits<Dest>::max());

        if (convertedValue < minValue)
            return static_cast<Dest>(std::numeric_limits<Dest>::lowest());
        if (convertedValue > maxValue)
            return static_cast<Dest>(std::numeric_limits<Dest>::max());
        return static_cast<Dest>(value);
    }
}
