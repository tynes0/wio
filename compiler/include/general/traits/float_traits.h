#pragma once
#include <type_traits>

#include "trait_base.h"
#include "../result_values.h"

namespace wio::common::traits
{
    template <typename T>
    requires std::is_floating_point_v<T>
    class FloatTraits : TraitBase<T>
    {
    public:
        using Type = T;

        static Type FloatResultAs(const FloatResult& result)
        {
            if constexpr(std::is_same_v<T, f32>) return result.value.v_f32;
            if constexpr(std::is_same_v<T, f64>) return result.value.v_f64;
            return static_cast<T>(0);
        }

        static Type FloatResultCastedAs(const FloatResult& result)
        {
            if (result.type == FloatType::f32) return static_cast<T>(result.value.v_f32);
            if (result.type == FloatType::f64) return static_cast<T>(result.value.v_f64);
            return static_cast<T>(0);
        }

        static std::string toString(Type value)
        {
            return std::to_string(value);
        }
    };
}
