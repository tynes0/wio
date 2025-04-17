#pragma once

#include <type_traits>
#include <memory>

#define BIT(x) (1ll << x##ll)
#define UBIT(x) (1ull << x##ull)

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

    struct packed_bool16
    {
        bool b1 : 1;
        bool b2 : 1;
        bool b3 : 1;
        bool b4 : 1;
        bool b5 : 1;
        bool b6 : 1;
        bool b7 : 1;
        bool b8 : 1;
        bool b9 : 1;
        bool b10 : 1;
        bool b11 : 1;
        bool b12 : 1;
        bool b13 : 1;
        bool b14 : 1;
        bool b15 : 1;
        bool b16 : 1;
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

    using id_t = uint32_t;
}

namespace std
{
    template <>
    struct hash<wio::id_t>
    {
        size_t operator()(const wio::id_t& id) const noexcept
        {
            return std::hash<uint32_t>{}(id);
        }
    };

}