#pragma once

#include <std_async.h>

#include <cstdint>
#include <string>

namespace vm_fixture
{
    std::int32_t Add(std::int32_t left, std::int32_t right);
    wio::runtime::AsyncTask<std::int32_t> Delayed(std::int32_t value);
    void Print(std::string value);
} // namespace vm_fixture
