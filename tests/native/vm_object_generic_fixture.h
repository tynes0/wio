#pragma once

#include <ref.h>
#include <wir_object_abi.h>
#include <cstdint>
#include <functional>
#include <string>

namespace vm_object_generic
{
    inline std::int32_t constructed = 0;
    inline std::int32_t destroyed = 0;
    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> capturedReference;

    inline void Constructed()
    {
        ++constructed;
    }
    inline void Destroyed()
    {
        ++destroyed;
    }
    inline std::int32_t Constructions()
    {
        return constructed;
    }
    inline std::int32_t Destructions()
    {
        return destroyed;
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

    template <class T> std::int32_t Exercise(wio::runtime::Ref<T> object)
    {
        const auto expected = object->_WF_Read();
        const auto tag = object->_WF_TagValue();
        for (std::int32_t index = 0; index < 30; ++index)
        {
            if (object->_WF_Same().Get() != object.Get())
                return 1;
            const auto claims = object->StrongCount();
            auto borrowed = object->_WF_AsRef();
            auto view = object->_WF_AsView();
            if (borrowed.Get() != object.Get() || view.Get() != object.Get() || object->StrongCount() != claims)
                return 2;
            auto cloned = object->_WF_Clone();
            if (cloned.Get() == object.Get() || cloned->_WF_Read() != expected || cloned->_WF_TagValue() != tag)
                return 3;
        }
        return 0;
    }
} // namespace vm_object_generic
