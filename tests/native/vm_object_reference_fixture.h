#pragma once

#include <ref.h>
#include <wir_object_abi.h>
#include <cstdint>
#include <functional>

namespace wio::wir_backend
{
    template <class T> class Place;
}

namespace vm_object_references
{
    inline std::int32_t constructed = 0;
    inline std::int32_t destroyed = 0;
    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> capturedReference;
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

    template <class T> std::int32_t Exercise(wio::runtime::Ref<T> object)
    {
        using Place = wio::wir_backend::Place<wio::runtime::Ref<T>>;
        for (std::int32_t index = 0; index < 30; ++index)
        {
            auto target = object;
            object->_WF_MakeTarget_ref_Counter_i32(Place::borrow(target), 100 + index);
            if (target.Get() == object.Get() || target->_WF_Read() != 100 + index)
                return 1;
            if (object->_WF_ReadFrom_view_Counter(Place::borrowView(target)) != 100 + index)
                return 2;
            if (object->_WF_ReadPair_view_Counter_view_Counter(Place::raw(object.Get()), Place::raw(target.Get())) !=
                object->_WF_Read() + 100 + index)
                return 7;
            if (object->_WF_Alias_ref_Counter_view_Counter_Counter(Place::borrow(target), Place::borrowView(target),
                                                                   object) != object->_WF_Read() ||
                target.Get() != object.Get())
                return 3;
            object->_WF_MakeTarget_ref_Counter_i32(Place::borrow(target), 200 + index);
            auto distinctCell = target;
            if (object->_WF_Alias_ref_Counter_view_Counter_Counter(
                    Place::borrow(target), Place::borrowView(distinctCell), object) != 200 + index ||
                target.Get() != object.Get() || distinctCell.Get() == object.Get())
                return 4;
            if (object->_WF_Twice_ref_Counter_ref_Counter_Counter(
                    Place::borrow(distinctCell), Place::borrow(distinctCell), object) != object->_WF_Read() ||
                distinctCell.Get() != object.Get())
                return 5;
            const auto returned = object->_WF_AssignAndReturn_ref_Counter_Counter(Place::borrow(distinctCell), object);
            if (returned.Get() != object.Get() || object->_WF_Same().Get() != object.Get())
                return 6;
        }
        return 0;
    }

    template <class T> bool ReadonlyReplace(wio::runtime::Ref<T> object, wio::runtime::Ref<T>& target)
    {
        using Place = wio::wir_backend::Place<wio::runtime::Ref<T>>;
        for (int kind = 0; kind < 2; ++kind)
        {
            try
            {
                object->_WF_MakeTarget_ref_Counter_i32(kind == 0 ? Place::borrowView(target) : Place::raw(object.Get()),
                                                       7);
                return false;
            }
            catch (const wio::wir_backend::abi::Error& error)
            {
                if (error.status != WIO_NATIVE_ABI_INVALID_ARGUMENT)
                    return false;
            }
        }
        return true;
    }

    template <class T> bool PanicReplace(wio::runtime::Ref<T> object, wio::runtime::Ref<T>& target)
    {
        using Place = wio::wir_backend::Place<wio::runtime::Ref<T>>;
        try
        {
            object->_WF_FailReplace_ref_Counter_Counter_i32(Place::borrow(target), object, 0);
        }
        catch (const wio::wir_backend::abi::Error& error)
        {
            return error.status == WIO_NATIVE_ABI_PANIC && target.Get() == object.Get();
        }
        return false;
    }
} // namespace vm_object_references
