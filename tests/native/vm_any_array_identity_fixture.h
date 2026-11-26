#pragma once

#include "any.h"

#include <iostream>
#include <string>

namespace vm_any_array_identity
{
    inline wio::runtime::Any Identity(wio::runtime::Any value)
    {
        return value;
    }

    template <class T> T Echo(T value)
    {
        return value;
    }

    inline void Print(std::string message)
    {
        std::cout << message << '\n';
    }
} // namespace vm_any_array_identity
