#pragma once

#include "vm_object_any_fixture.h"

namespace vm_nullable_any
{
    inline bool IsNull(const wio::runtime::Any& value)
    {
        return value.IsNull();
    }
    inline bool IsOpaque(const wio::runtime::Any& value)
    {
        return value.IsOpaque();
    }
    inline void* Pointer(const wio::runtime::Any& value)
    {
        return value.AsOpaque();
    }
    inline void* Token()
    {
        static int token;
        return &token;
    }
    inline void* NullPointer()
    {
        return nullptr;
    }
    inline void RebindNull(wio::runtime::Any& value)
    {
        value = nullptr;
    }
    inline void Print()
    {
        std::cout << "native-nullable-any-ok:30\n";
    }
} // namespace vm_nullable_any
