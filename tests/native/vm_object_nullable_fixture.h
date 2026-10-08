#pragma once

#include <ref.h>
#include <wir_object_abi.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <iostream>

namespace vm_object_nullable
{
    inline std::int32_t constructed[2]{};
    inline std::int32_t destroyed[2]{};
    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> capturedReference;
    inline void Constructed(std::int32_t kind)
    {
        ++constructed[kind];
    }
    inline void Destroyed(std::int32_t kind)
    {
        ++destroyed[kind];
    }
    inline std::int32_t Constructions(std::int32_t kind)
    {
        return constructed[kind];
    }
    inline std::int32_t Destructions(std::int32_t kind)
    {
        return destroyed[kind];
    }
    inline void ReportCounts()
    {
        std::cerr << "Counter constructed/destroyed: " << constructed[0] << '/' << destroyed[0]
                  << "; Badge constructed/destroyed: " << constructed[1] << '/' << destroyed[1] << '\n';
    }

    template <class T> std::uint64_t Capture(wio::runtime::Ref<T> object)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(object.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        capturedHost = proxy->hostLease().host();
        capturedReference = [object = std::move(object)] {};
        return reinterpret_cast<std::uintptr_t>(&capturedHost);
    }
    inline void ClearCapture()
    {
        capturedHost = {};
        capturedReference = {};
    }
    template <class T> auto Echo(std::optional<wio::runtime::Ref<T>> value)
    {
        return value;
    }
    template <class T> std::int32_t Exercise(wio::runtime::Ref<T> object)
    {
        using Nullable = std::optional<wio::runtime::Ref<T>>;
        for (std::int32_t index = 0; index < 40; ++index)
        {
            if (object->_WF_MaybeSelf_bool(false))
                return 1;
            auto same = object->_WF_MaybeSelf_bool(true);
            if (!same || same->Get() != object.Get())
                return 2;
            auto echoed = object->_WF_Echo_CounterN(same);
            if (!echoed || echoed->Get() != object.Get())
                return 3;
            if (object->_WF_Echo_CounterN(Nullable{}) || object->_WF_ReadMaybe_CounterN(Nullable{}) != -1)
                return 4;
            if (object->_WF_ReadMaybe_CounterN(same) != object->_WF_Read())
                return 5;
            if (object->_WF_MakeBadge_i32_bool(index, false))
                return 6;
            auto badge = object->_WF_MakeBadge_i32_bool(100 + index, true);
            if (!badge || (*badge)->_WF_Read() != 100 + index || object->_WF_ReadBadge_BadgeN(badge) != 100 + index)
                return 7;
        }
        return 0;
    }
} // namespace vm_object_nullable
