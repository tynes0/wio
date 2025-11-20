#pragma once

#include <type_traits>
#include <memory>
#include <string>

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
    using boolean_t = bool;
    using integer_t = long long;
    using float_t = double;
    using float_ref_t = double*;
    using character_t = char;
    using character_ref_t = char*;
    using string_t = std::string;

    static constexpr id_t ID_T_MAX = (std::numeric_limits<id_t>::max)();
    static constexpr integer_t INTEGER_T_MAX = (std::numeric_limits<integer_t>::max)();
    static constexpr float_t FLOAT_T_MAX = (std::numeric_limits<float_t>::max)();
    static constexpr character_t CHAR_T_MAX = (std::numeric_limits<character_t>::max)();

    static constexpr id_t ID_T_MIN = (std::numeric_limits<id_t>::min)();
    static constexpr integer_t INTEGER_T_MIN = (std::numeric_limits<integer_t>::min)();
    static constexpr float_t FLOAT_T_MIN = (std::numeric_limits<float_t>::min)();
    static constexpr character_t CHAR_T_MIN = (std::numeric_limits<character_t>::min)();
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