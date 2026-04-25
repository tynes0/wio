#include "std_assert.h"

#include "exception.h"

namespace wio::runtime::std_assert
{
    [[noreturn]] void Fail(const std::string& message)
    {
        throw RuntimeException("Assertion failed: " + message);
    }

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
            Fail(message);
    }

    [[noreturn]] void Unreachable(const std::string& message)
    {
        throw RuntimeException("Unreachable code reached: " + message);
    }
}
