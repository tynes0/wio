#pragma once

#include "vm_object_reference_fixture.h"
#include <optional>

namespace vm_object_borrowed_results
{
    template <class T> std::int32_t CheckReceiver(wio::runtime::Ref<T> object)
    {
        const auto claims = object->StrongCount();
        auto self = object->_WF_AsRef();
        auto view = object->_WF_AsView();
        if (self.Get() != object.Get() || view.Get() != object.Get() || object->StrongCount() != claims)
            return 1;
        self.Get()->_WF_Set_i32(37);
        return view.Get()->_WF_Read() == 37 && !self.canRebind() && !view.canRebind() ? 0 : 2;
    }

    template <class T> std::int32_t Exercise(wio::runtime::Ref<T> object)
    {
        using Place = wio::wir_backend::Place<wio::runtime::Ref<T>>;
        using Nullable = std::optional<wio::runtime::Ref<T>>;
        using NullablePlace = wio::wir_backend::Place<Nullable>;
        for (std::int32_t index = 0; index < 30; ++index)
        {
            const auto claims = object->StrongCount();
            auto self = object->_WF_AsRef();
            auto view = object->_WF_AsView();
            if (self.Get() != object.Get() || view.Get() != object.Get() || object->StrongCount() != claims)
                return 1;
            self.Get()->_WF_Set_i32(17);
            if (view.Get()->_WF_Read() != 17 || self.canRebind() || view.canRebind())
                return 2;
            auto target = object;
            auto returned = object->_WF_BorrowTarget_ref_Counter(Place::borrow(target));
            auto observed = object->_WF_ViewTarget_view_Counter(Place::borrowView(target));
            if (returned.borrowIdentity() != &target || observed.borrowIdentity() != &target || observed.canRebind())
                return 3;
            object->_WF_Make_ref_Counter_i32(Place::borrow(target), 100 + index);
            if (returned.Get() != target.Get() || observed.Get()->_WF_Read() != 100 + index)
                return 4;
            auto assigned = object->_WF_AssignAndBorrow_ref_Counter_Counter(Place::borrow(target), object);
            if (assigned.borrowIdentity() != &target || target.Get() != object.Get() || observed.Get() != object.Get())
                return 5;
            returned.write(object);
            auto other = object;
            for (const bool left : {false, true})
            {
                const auto selected =
                    object->_WF_Choose_ref_Counter_ref_Counter_bool(Place::borrow(target), Place::borrow(other), left);
                if (selected.borrowIdentity() != (left ? &target : &other))
                    return 6;
            }
            Nullable nullable;
            auto nullableAlias = object->_WF_NullableTarget_ref_CounterN(NullablePlace::borrow(nullable));
            auto nullableView = object->_WF_NullableView_view_CounterN(NullablePlace::borrowView(nullable));
            if (nullableAlias.borrowIdentity() != &nullable || nullableView.read() || nullableView.canRebind())
                return 7;
            nullableAlias.write(Nullable{object});
            if (!nullable || nullableView.read()->Get() != object.Get())
                return 8;
            nullableAlias.write(Nullable{});
            if (nullableView.read())
                return 9;
            auto local = Place::local();
            local.write(object);
            auto localAlias = object->_WF_BorrowTarget_ref_Counter(local);
            if (localAlias.borrowIdentity() != local.borrowIdentity())
                return 10;
            localAlias.write(other);
            if (local.Get() != other.Get())
                return 11;
            try
            {
                observed.write(other);
                return 12;
            }
            catch (const std::runtime_error&)
            {
            }
        }
        return 0;
    }
} // namespace vm_object_borrowed_results
