#include "native_generic_pod_components.h"

namespace native_pod_generic
{
    std::int32_t SumI32(pair2<std::int32_t> value)
    {
        return value.first + value.second;
    }

    void ScaleF32(pair2<float>& value, float factor)
    {
        value.first *= factor;
        value.second *= factor;
    }

    std::int32_t SumWrapperI32(wrapper<std::int32_t> value)
    {
        return value.value.first + value.value.second;
    }

    pair2<std::int32_t> MakeI32(std::int32_t first, std::int32_t second)
    {
        return pair2<std::int32_t>{ first, second };
    }
}
