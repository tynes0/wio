// base.h
#pragma once

#include <type_traits>
#include <memory>

namespace wio
{
    template <class _Ty>
    using ref = std::shared_ptr<_Ty>;

    template <class _Ty>
    using weak_ref = std::weak_ptr<_Ty>;

    template <class T, class... _Types>
    [[nodiscard]] ref<T> make_ref(_Types&&... args)
    {
        return std::make_shared<T, _Types...>(std::forward<_Types>(args)...);
    }
}
