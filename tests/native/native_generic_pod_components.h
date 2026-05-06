#pragma once

#include <cstdint>

namespace native_pod_generic
{
    template <typename T>
    struct pair2
    {
        T first;
        T second;
    };

    template <typename T>
    struct wrapper
    {
        pair2<T> value;
    };

    std::int32_t SumI32(pair2<std::int32_t> value);
    void ScaleF32(pair2<float>& value, float factor);
    std::int32_t SumWrapperI32(wrapper<std::int32_t> value);
    pair2<std::int32_t> MakeI32(std::int32_t first, std::int32_t second);
}
