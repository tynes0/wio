#pragma once

#include <any.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace vm_object_any
{
    inline std::array<std::int32_t, 4> constructions{};
    inline std::array<std::int32_t, 4> destructions{};
    inline wio::runtime::Any held;

    inline void Constructed(std::int32_t kind)
    {
        ++constructions.at(kind);
    }
    inline void Destroyed(std::int32_t kind)
    {
        ++destructions.at(kind);
    }
    inline std::int32_t Constructions(std::int32_t kind)
    {
        return constructions.at(kind);
    }
    inline std::int32_t Destructions(std::int32_t kind)
    {
        return destructions.at(kind);
    }
    inline wio::runtime::Any Echo(wio::runtime::Any value)
    {
        return value;
    }
    inline bool IsObject(const wio::runtime::Any& value)
    {
        return value.Kind() == wio::runtime::AnyStorageKind::ObjectReference;
    }
    inline bool Same(const wio::runtime::Any& first, wio::runtime::Any& second)
    {
        return &first == &second;
    }
    inline void Rebind(wio::runtime::Any& target, wio::runtime::Any source)
    {
        target = std::move(source);
    }
    inline void Hold(wio::runtime::Any value)
    {
        held = std::move(value);
    }
    inline wio::runtime::Any Held()
    {
        return held;
    }
    inline void Clear()
    {
        held = nullptr;
    }
    template <class T> std::int32_t Read(const wio::runtime::Any& value)
    {
        return value.CastObject<typename T::element_type>()->_WF_Read();
    }
    template <class T> std::string ReadString(const wio::runtime::Any& value)
    {
        return value.CastObject<typename T::element_type>()->_WF_Read();
    }
    template <class T> void Set(wio::runtime::Any value, std::int32_t next)
    {
        value.CastObject<typename T::element_type>()->_WF_Set_i32(next);
    }
    inline void Print()
    {
        std::cout << "native-object-any-ok:30\n";
    }
    inline void DumpCounts()
    {
        for (std::size_t kind = 0; kind < constructions.size(); ++kind)
            std::cerr << "kind=" << kind << " constructed=" << constructions[kind]
                      << " destroyed=" << destructions[kind] << '\n';
    }
} // namespace vm_object_any
