#pragma once

#include <cstdint>
#include <string>

#include <any.h>

namespace native_any
{
    inline void* MakeTaggedOpaque(const std::int32_t value)
    {
        return reinterpret_cast<void*>(static_cast<std::intptr_t>(value) + 1);
    }

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

    inline bool IsOpaquePayload(const wio::runtime::Any& value)
    {
        return value.Kind() == wio::runtime::AnyStorageKind::OpaquePayload && value.IsOpaque();
    }

    inline std::int32_t ReadOpaqueTag(const wio::runtime::Any& value)
    {
        void* payload = value.AsOpaque();
        return payload ? static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(payload) - 1) : -1;
    }

    inline void ReplaceWithOpaqueTag(wio::runtime::Any& target, const std::int32_t value)
    {
        target = MakeTaggedOpaque(value);
    }

    inline wio::runtime::Any Echo(const wio::runtime::Any& value)
    {
        return value;
    }
}
