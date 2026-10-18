#pragma once

#include <string>

namespace native_hook
{
    inline int calls = 0;

    inline int Calls()
    {
        return calls;
    }

    template <class T> std::string Render(T* value)
    {
        ++calls;
        return "native-hook:" + std::to_string(value->_WF_Value());
    }
} // namespace native_hook
