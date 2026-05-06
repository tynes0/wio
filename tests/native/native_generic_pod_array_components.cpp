#include "native_generic_pod_array_components.h"

namespace native_pod_array_generic
{
    std::int32_t SumI32(triplet<std::int32_t> value)
    {
        return value.values[0] + value.values[1] + value.values[2];
    }

    void RotateLeftI32(triplet<std::int32_t>& value)
    {
        const auto first = value.values[0];
        value.values[0] = value.values[1];
        value.values[1] = value.values[2];
        value.values[2] = first;
    }

    triplet<std::int32_t> MakeI32(std::int32_t a, std::int32_t b, std::int32_t c)
    {
        return triplet<std::int32_t>{ std::array<std::int32_t, 3>{ a, b, c } };
    }
}
