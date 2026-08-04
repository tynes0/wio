#pragma once

#include <cstdint>
#include <array>

namespace native_specialized
{
    template <typename T>
    struct record
    {
        T value;
    };

    template <>
    struct record<std::int32_t>
    {
        std::int32_t value;
        std::int32_t bonus;
    };

    inline std::int32_t Sum(record<std::int32_t> value)
    {
        return value.value + value.bonus;
    }

    template <typename T, std::size_t N>
    struct block
    {
        std::array<T, N> values;
    };

    inline std::int32_t SumBlock4(block<std::int32_t, 4> value)
    {
        return value.values[0] + value.values[1] + value.values[2] + value.values[3];
    }
}
