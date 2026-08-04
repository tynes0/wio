#pragma once

#include <cstdint>

namespace native_resource_owned
{
    inline std::int32_t closeCount = 0;

    inline void* Open(const std::int32_t value)
    {
        return new std::int32_t(value);
    }

    inline void* MaybeOpen(const bool shouldOpen, const std::int32_t value)
    {
        return shouldOpen ? Open(value) : nullptr;
    }

    inline std::int32_t Read(const void* handle)
    {
        return *static_cast<const std::int32_t*>(handle);
    }

    inline void Close(void* handle)
    {
        if (handle != nullptr)
        {
            delete static_cast<std::int32_t*>(handle);
            ++closeCount;
        }
    }

    inline std::int32_t CloseCount()
    {
        return closeCount;
    }
}
