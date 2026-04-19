#pragma once

#include <string>

#include "exception.h"

namespace wio::runtime::std_assert
{
    [[noreturn]] void Fail(const std::string& message);
    void Require(bool condition, const std::string& message);
    [[noreturn]] void Unreachable(const std::string& message);
}
