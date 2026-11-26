#include "fixture.h"

#include <iostream>

namespace vm_fixture
{
    std::int32_t Add(const std::int32_t left, const std::int32_t right)
    {
        return left + right;
    }

    wio::runtime::AsyncTask<std::int32_t> Delayed(const std::int32_t value)
    {
        co_await wio::runtime::AsyncSleep(value == 99 ? 50 : 5);
        co_return value;
    }

    void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_fixture
