#pragma once

#include <array>
#include <cstdint>

namespace native_pod_array_generic
{
    template <typename T>
    struct triplet
    {
        std::array<T, 3> values;
    };

    std::int32_t SumI32(triplet<std::int32_t> value);
    void RotateLeftI32(triplet<std::int32_t>& value);
    triplet<std::int32_t> MakeI32(std::int32_t a, std::int32_t b, std::int32_t c);
}
