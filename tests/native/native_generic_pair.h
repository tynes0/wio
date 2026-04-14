#pragma once

namespace native_generic
{
    template <typename T, typename U>
    inline T PickLeft(T left, U)
    {
        return left;
    }
}
