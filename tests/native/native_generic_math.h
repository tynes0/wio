#pragma once

namespace native_generic
{
    template <typename T>
    inline T DoubleValue(T value)
    {
        return value + value;
    }
}
