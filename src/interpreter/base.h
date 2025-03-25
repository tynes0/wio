#pragma once

#include <type_traits>
#include <memory>

namespace wio
{
    struct packed_bool
    {
        bool b1 : 1;
        bool b2 : 1;
        bool b3 : 1;
        bool b4 : 1;
        bool b5 : 1;
        bool b6 : 1;
        bool b7 : 1;
        bool b8 : 1;
    };

    template <class _Ty>
    using ref = std::shared_ptr<_Ty>;

    template <class _Ty>
    using weak_ref = std::weak_ptr<_Ty>;

    template <class T, class... _Types>
    [[nodiscard]] ref<T> make_ref(_Types&&... args)
    {
        return std::make_shared<T, _Types...>(std::forward<_Types>(args)...);
    }

#define BIT(x) (1 << x)
}
