#pragma once

#include "vm_projected_borrow_fixture.h"

#include <map>

namespace vm_dictionary_intrinsic
{
    inline std::int32_t constructed = 0;
    inline std::int32_t destroyed = 0;

    inline void Constructed()
    {
        ++constructed;
    }

    inline void Destroyed()
    {
        ++destroyed;
    }

    inline bool Balanced()
    {
        return constructed == destroyed;
    }

    inline std::int32_t Live()
    {
        return constructed - destroyed;
    }

    inline void CleanupFailure()
    {
        throw std::runtime_error("dictionary intrinsic destructor failure");
    }

    template <class T> T& Borrow(std::unordered_map<std::string, T>& values, T& value)
    {
        if (&values.at("kept") != &value)
            throw std::runtime_error("dictionary intrinsic borrow address mismatch");
        return value;
    }

    template <class T> T& TreeBorrow(std::map<std::int32_t, T>& values, T& value)
    {
        if (&values.at(20) != &value)
            throw std::runtime_error("ordered dictionary intrinsic borrow address mismatch");
        return value;
    }

    inline void Print()
    {
        std::cout << "native-dictionary-intrinsic-ok:30\n";
    }
} // namespace vm_dictionary_intrinsic
