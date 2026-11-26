#pragma once

#include "vm_object_reference_fixture.h"
#include <iostream>
#include <optional>

namespace vm_object_nullable_references
{
    inline void ReportCounts()
    {
        std::cerr << "nullable reference constructed/destroyed: " << vm_object_references::Constructions() << '/'
                  << vm_object_references::Destructions() << '\n';
    }
    template <class T> std::int32_t Exercise(wio::runtime::Ref<T> object)
    {
        using Nullable = std::optional<wio::runtime::Ref<T>>;
        using Place = wio::wir_backend::Place<Nullable>;
        using PlainPlace = wio::wir_backend::Place<wio::runtime::Ref<T>>;
        for (std::int32_t index = 0; index < 30; ++index)
        {
            Nullable target;
            if (object->_WF_ReadFrom_view_CounterN(Place::borrowView(target)) != -1)
                return 1;
            if (object->_WF_Mixed_ref_Counter_view_CounterN(PlainPlace::borrow(object), Place::borrowView(target)) !=
                object->_WF_Read() - 1)
                return 11;
            object->_WF_Make_ref_CounterN_i32(Place::borrow(target), 100 + index);
            if (!target || (*target)->_WF_Read() != 100 + index)
                return 2;
            if (object->_WF_Alias_ref_CounterN_view_CounterN_CounterN(Place::borrow(target), Place::borrowView(target),
                                                                      Nullable{object}) != object->_WF_Read() ||
                !target || target->Get() != object.Get())
                return 3;
            if (!object->_WF_ClearAlias_ref_CounterN_view_CounterN(Place::borrow(target), Place::borrowView(target)) ||
                target)
                return 4;
            target = object;
            auto distinct = target;
            if (object->_WF_ClearAlias_ref_CounterN_view_CounterN(Place::borrow(target), Place::borrowView(distinct)) ||
                target || !distinct || distinct->Get() != object.Get())
                return 5;
            if (object->_WF_Twice_ref_CounterN_ref_CounterN_CounterN(Place::borrow(target), Place::borrow(target),
                                                                     Nullable{object}) != object->_WF_Read())
                return 6;
            object->_WF_Set_ref_CounterN_CounterN(Place::borrow(target), Nullable{});
            if (target)
                return 7;
            if (object->_WF_ReverseAlias_view_CounterN_ref_CounterN_CounterN(
                    Place::borrowView(target), Place::borrow(target), Nullable{object}) != object->_WF_Read())
                return 10;
            auto local = Place::local();
            local.write(Nullable{});
            if (object->_WF_Alias_ref_CounterN_view_CounterN_CounterN(local, local, Nullable{object}) !=
                object->_WF_Read())
                return 8;
            object->_WF_Clear_ref_CounterN(local);
            if (local.read())
                return 9;
        }
        return 0;
    }

    template <class T> bool ReadonlyClear(wio::runtime::Ref<T> object)
    {
        using Nullable = std::optional<wio::runtime::Ref<T>>;
        using Place = wio::wir_backend::Place<Nullable>;
        Nullable target{object};
        try
        {
            object->_WF_Clear_ref_CounterN(Place::borrowView(target));
        }
        catch (const wio::wir_backend::abi::Error& error)
        {
            return error.status == WIO_NATIVE_ABI_INVALID_ARGUMENT && target && target->Get() == object.Get();
        }
        return false;
    }

    template <class T> bool PanicClear(wio::runtime::Ref<T> object, std::optional<wio::runtime::Ref<T>>& target)
    {
        using Place = wio::wir_backend::Place<std::optional<wio::runtime::Ref<T>>>;
        try
        {
            object->_WF_FailClear_ref_CounterN_i32(Place::borrow(target), 0);
        }
        catch (const wio::wir_backend::abi::Error& error)
        {
            return error.status == WIO_NATIVE_ABI_PANIC && !target;
        }
        return false;
    }
} // namespace vm_object_nullable_references
