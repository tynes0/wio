#pragma once

#include <cstdint>
#include <string>

#include <any.h>

namespace native_any
{
    inline wio::runtime::Any MakeInt(const std::int32_t value)
    {
        return wio::runtime::Any(value * 2);
    }

    inline std::int32_t ReadInt(const wio::runtime::Any& value)
    {
        return value.AsBoxed<std::int32_t>();
    }

    inline bool IsNull(const wio::runtime::Any& value)
    {
        return value == nullptr;
    }

    inline bool IsString(const wio::runtime::Any& value)
    {
        return value.IsBoxed<std::string>();
    }

    inline void ReplaceWithGreeting(wio::runtime::Any& target)
    {
        target = "payload";
    }

    inline bool IsObjectPayload(const wio::runtime::Any& value)
    {
        return value.Kind() == wio::runtime::AnyStorageKind::ObjectReference;
    }

    inline wio::runtime::Any Echo(const wio::runtime::Any& value)
    {
        return value;
    }
}
