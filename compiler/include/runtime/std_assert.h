#pragma once

#include <string>

#include "exception.h"

namespace wio::runtime::std_assert
{
    [[noreturn]] inline void Fail(const std::string& message)
    {
        throw RuntimeException("Assertion failed: " + message);
    }

    inline void Require(bool condition, const std::string& message)
    {
        if (!condition)
            Fail(message);
    }

    [[noreturn]] inline void Unreachable(const std::string& message)
    {
        throw RuntimeException("Unreachable code reached: " + message);
    }
}
