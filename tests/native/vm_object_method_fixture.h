#pragma once

#include <ref.h>
#include <text.h>
#include <wir_object_abi.h>
#include <cstdint>
#include <functional>
#include <string>

namespace vm_object_methods
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
    template <class T> auto Fail(wio::runtime::Ref<T> object)
    {
        return object->_WF_Fail_Counter_i32(object, 0);
    }
    template <class T> auto MakeForeign(wio::runtime::Ref<T>)
    {
        // Raw producer storage is intentionally not a Wio constructor thunk.
        return wio::runtime::Ref<T>::Create(13);
    }

    template <class T> bool Same(wio::runtime::Ref<T> object)
    {
        return object->_WF_Same().Get() == object.Get();
    }
    template <class T> auto Pick(wio::runtime::Ref<T> object, wio::runtime::Ref<T> other)
    {
        return object->_WF_Choose_Counter(other);
    }
    template <class T> std::int32_t Exercise(wio::runtime::Ref<T> object)
    {
        std::int32_t sum = 0;
        for (std::int32_t index = 0; index < 50; ++index)
        {
            auto badge = object->_WF_MakeBadge_i32(100 + index);
            if (badge->_WF_Same().Get() != badge.Get())
                return -3;
            sum += object->_WF_ReadFrom_Badge(badge);
            if (object->_WF_Relay_Counter_Badge(object, badge).Get() != object.Get())
                return -1;
            const std::string label{"a\0b", 3};
            if (object->_WF_Label_string(label) != label ||
                object->_WF_EchoText_text(wio::runtime::Text::FromUtf8("\xc4\xb0 \xf0\x9f\x9a\x80")).Utf8() !=
                    "\xc4\xb0 \xf0\x9f\x9a\x80")
                return -2;
        }
        return sum;
    }
} // namespace vm_object_methods
