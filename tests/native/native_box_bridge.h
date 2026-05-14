#pragma once

#include <cstdint>

namespace native_box
{
    template <typename TBoxRef>
    inline std::int32_t Read(const TBoxRef& value)
    {
        return value->_WF_Get();
    }

    template <typename TBoxRef>
    inline void Add(const TBoxRef& value, const std::int32_t delta)
    {
        value->_WF_Set_T(value->_WF_Get() + delta);
    }

    template <typename TBoxRef>
    inline void Rebind(TBoxRef& target, const std::int32_t value)
    {
        target = TBoxRef::Create(value);
    }
}
