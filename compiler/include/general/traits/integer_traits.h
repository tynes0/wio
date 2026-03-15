#pragma once

#include <type_traits>

#include "trait_base.h"

#include "../result_values.h"

namespace wio::common::traits
{
    template <typename T>
    requires std::is_integral_v<T>
    class IntegerTraits : TraitBase<T>
    {
    public:
        using Type = T;

        static Type IntegerResultAs(const IntegerResult& result)
        {
            if constexpr(std::is_same_v<T, i8>) return result.value.v_i8;
            if constexpr(std::is_same_v<T, i16>) return result.value.v_i16;
            if constexpr(std::is_same_v<T, i32>) return result.value.v_i32;
            if constexpr(std::is_same_v<T, i64>) return result.value.v_i64;
            if constexpr(std::is_same_v<T, u8>) return result.value.v_u8;
            if constexpr(std::is_same_v<T, u16>) return result.value.v_u16;
            if constexpr(std::is_same_v<T, u32>) return result.value.v_u32;
            if constexpr(std::is_same_v<T, u64>) return result.value.v_u64;
            if constexpr(std::is_same_v<T, isize>) return result.value.v_isize;
            if constexpr(std::is_same_v<T, usize>) return result.value.v_usize;
            return static_cast<T>(0);
        }

        static Type IntegerResultCastedAs(const IntegerResult& result)
        {
            if (result.type == IntegerType::i8) return static_cast<T>(result.value.v_i8);
            if (result.type == IntegerType::i16) return static_cast<T>(result.value.v_i16);
            if (result.type == IntegerType::i32) return static_cast<T>(result.value.v_i32);
            if (result.type == IntegerType::i64) return static_cast<T>(result.value.v_i64);
            if (result.type == IntegerType::u8) return static_cast<T>(result.value.v_u8);
            if (result.type == IntegerType::u16) return static_cast<T>(result.value.v_u16);
            if (result.type == IntegerType::u32) return static_cast<T>(result.value.v_u32);
            if (result.type == IntegerType::u64) return static_cast<T>(result.value.v_u64);
            if (result.type == IntegerType::isize) return static_cast<T>(result.value.v_isize);
            if (result.type == IntegerType::usize) return static_cast<T>(result.value.v_usize);
            return static_cast<T>(0);
        }

        static std::string toString(Type value)
        {
            if constexpr (std::is_same_v<T, char>)
                return std::string(1, value);
            if constexpr (std::is_same_v<T, bool>)
                return std::string(value ? "true" : "false");
            return std::to_string(value);
        }
    };
}
