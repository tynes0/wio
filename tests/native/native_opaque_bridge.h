#pragma once

#include <cstdint>

namespace native_opaque
{
    inline void* MakeTagged(const std::int32_t value)
    {
        return reinterpret_cast<void*>(static_cast<std::intptr_t>(value) + 1);
    }

    inline bool IsNull(const void* value)
    {
        return value == nullptr;
    }

    inline bool Same(const void* lhs, const void* rhs)
    {
        return lhs == rhs;
    }

    inline std::int32_t ReadTag(const void* value)
    {
        return value ? static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(value) - 1) : -1;
    }

    inline std::int32_t ReadTagView(void* const& value)
    {
        return value ? static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(value) - 1) : -1;
    }

    inline void Replace(void*& target, void* value)
    {
        target = value;
    }
}
