#pragma once

#include <stdexcept>

namespace native_exception_boundary
{
    inline int ThrowStandardException()
    {
        throw std::runtime_error("native-boom");
    }
}
