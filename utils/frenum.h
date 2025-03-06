/**
 * This file is part of the Frenum library, an advanced enum management library for C++.
 *
 * Frenum provides enhanced functionality for managing enums in modern C++ projects.
 * Features include enum-to-string conversion, metadata retrieval, and seamless support
 * for enums in namespaces. The library is built using modern C++20 features, ensuring
 * both compile-time safety and runtime efficiency.
 *
 * Key Features:
 * - String conversion for enums.
 * - Enum metadata retrieval (size, indices, and more).
 * - Namespace and scoped enum support.
 * - Compile-time safety with `concepts` and `constexpr`.
 * - Extend existing enums for additional functionality.
 * - Iterate over enum values with ease.
 *
 * Usage:
 * For detailed usage examples and more, refer to the project repository:
 * https://github.com/tynes0/Frenum
 *
 * Requirements:
 * - C++20 or later.
 * - A modern C++ compiler (e.g., GCC 10+, Clang 10+, MSVC 2019+).
 *
 * License:
 * This library is distributed under the MIT License. See the LICENSE file in the repository for details.
 *
 * Author: https://github.com/tynes0
 * Contact: cihanbilgihan@gmail.com
 * Repository: https://github.com/tynes0/Frenum
 */


#pragma once
#ifndef _FRENUM_DEFINED_
#define _FRENUM_DEFINED_

#include <type_traits>
#include <array>
#include <string_view>
#include <string>
#include <utility>
#include <optional>

#define __FRENUM_EXPAND(x) x
#define __FRENUM_CONCAT__(L, R) L##R
#define __FRENUM_CONCAT(L, R) __FRENUM_CONCAT__(L, R)
#define __FRENUM_STRINGIZE__(V) #V
#define __FRENUM_STRINGIZE(V) __FRENUM_STRINGIZE__(V)

#define __FRENUM_DATA(ENUM_NAME) __FRENUM_CONCAT(ENUM_NAME, _data)

#define __FRENUM_REGISTER_ENUM_DATA(NAMESPACE, ENUM_NAME)                               \
    template <>                                                                         \
    struct frenum::enum_data_registry<NAMESPACE ENUM_NAME>                              \
    {                                                                                   \
        using type = __FRENUM_DATA(ENUM_NAME);                                          \
    };

 // Macro max param -> 127
#define __FRENUM_ARG_SIZE_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10,_11,             \
    _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25,               \
    _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39,               \
    _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53,               \
    _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, _65, _66, _67,               \
    _68, _69, _70, _71, _72, _73, _74, _75, _76, _77, _78, _79, _80, _81,               \
    _82, _83, _84, _85, _86, _87, _88, _89, _90, _91, _92, _93, _94, _95,               \
    _96, _97, _98, _99, _100, _101, _102, _103, _104, _105, _106, _107,                 \
    _108,_109, _110, _111, _112, _113, _114, _115, _116, _117, _118, _119,              \
    _120, _121, _122, _123, _124, _125, COUNT, ...) COUNT

#define __FRENUM_ARG_SIZE(...) __FRENUM_EXPAND(__FRENUM_ARG_SIZE_IMPL(__VA_ARGS__,      \
    125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111,          \
    110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95,          \
    94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77,             \
    76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59,             \
    58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,             \
    40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22,         \
    21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

#define __FRENUM_NOT_ASSIGNABLE_VALUE(ARG, UNDERLYING_TYPE) ((frenum::detail::not_assignable_value<UNDERLYING_TYPE>)(UNDERLYING_TYPE)ARG)

#define __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM) { __FRENUM_NOT_ASSIGNABLE_VALUE(NAMESPACE ENUM_NAME::ITEM, underlying_type), frenum::detail::raw_name(__FRENUM_STRINGIZE(ITEM)) }

#define __FRENUM_FOR_EACH_1(NAMESPACE, ENUM_NAME, ITEM)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM)         
#define __FRENUM_FOR_EACH_2(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_1(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_3(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_2(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_4(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_3(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_5(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_4(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_6(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_5(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_7(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_6(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_8(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_7(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_9(NAMESPACE, ENUM_NAME, ITEM, ...)   __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_8(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_10(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),   __FRENUM_EXPAND(__FRENUM_FOR_EACH_9(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_11(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_10(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_12(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_11(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_13(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_12(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_14(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_13(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_15(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_14(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_16(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_15(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_17(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_16(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_18(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_17(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_19(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_18(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_20(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_19(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_21(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_20(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_22(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_21(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_23(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_22(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_24(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_23(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_25(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_24(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_26(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_25(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_27(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_26(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_28(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_27(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_29(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_28(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_30(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_29(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_31(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_30(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_32(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_31(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_33(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_32(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_34(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_33(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_35(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_34(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_36(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_35(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_37(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_36(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_38(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_37(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_39(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_38(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_40(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_39(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_41(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_40(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_42(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_41(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_43(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_42(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_44(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_43(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_45(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_44(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_46(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_45(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_47(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_46(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_48(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_47(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_49(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_48(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_50(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_49(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_51(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_50(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_52(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_51(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_53(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_52(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_54(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_53(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_55(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_54(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_56(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_55(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_57(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_56(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_58(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_57(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_59(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_58(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_60(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_59(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_61(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_60(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_62(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_61(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_63(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_62(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_64(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_63(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_65(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_64(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_66(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_65(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_67(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_66(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_68(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_67(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_69(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_68(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_70(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_69(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_71(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_70(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_72(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_71(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_73(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_72(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_74(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_73(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_75(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_74(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_76(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_75(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_77(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_76(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_78(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_77(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_79(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_78(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_80(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_79(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_81(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_80(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_82(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_81(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_83(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_82(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_84(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_83(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_85(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_84(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_86(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_85(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_87(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_86(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_88(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_87(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_89(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_88(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_90(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_89(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_91(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_90(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_92(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_91(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_93(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_92(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_94(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_93(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_95(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_94(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_96(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_95(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_97(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_96(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_98(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_97(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_99(NAMESPACE, ENUM_NAME, ITEM, ...)  __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_98(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_100(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM),  __FRENUM_EXPAND(__FRENUM_FOR_EACH_99(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_101(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_100(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_102(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_101(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_103(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_102(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_104(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_103(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_105(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_104(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_106(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_105(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_107(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_106(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_108(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_107(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_109(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_108(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_110(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_109(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_111(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_110(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_112(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_111(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_113(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_112(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_114(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_113(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_115(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_114(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_116(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_115(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_117(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_116(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_118(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_117(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_119(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_118(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_120(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_119(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_121(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_120(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_122(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_121(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_123(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_122(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_124(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_123(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_125(NAMESPACE, ENUM_NAME, ITEM, ...) __FRENUM_PAIR(NAMESPACE, ENUM_NAME, ITEM), __FRENUM_EXPAND(__FRENUM_FOR_EACH_124(NAMESPACE, ENUM_NAME, __VA_ARGS__))

#define __FRENUM_FOR_EACH(NAMESPACE, ENUM_NAME, ...) __FRENUM_EXPAND(__FRENUM_CONCAT(__FRENUM_FOR_EACH_, __FRENUM_ARG_SIZE(__VA_ARGS__))(NAMESPACE, ENUM_NAME, __VA_ARGS__))
#define __FRENUM_FOR_EACH_MAX 125 // Macro max param -> 127

#define __FRENUM_ENUM(CLASS_OR_ENUM, ENUM_NAME, UNDERLYING_TYPE, ...) CLASS_OR_ENUM ENUM_NAME : UNDERLYING_TYPE { __VA_ARGS__ };     
#define __FRENUM_UTT(X) std::underlying_type_t<X>
#define __FRENUM_EMPTY_NAMESPACE ::
#define __FRENUM_AS_NAMESPACE(NAMESPACE) __FRENUM_CONCAT(NAMESPACE, ::)
#define __FRENUM_NAMESPACE_BEGIN(NAMESPACE) namespace NAMESPACE {
#define __FRENUM_NAMESPACE_END }

#define __FRENUM_ENUM_DATA_CLASS(NAMESPACE, ENUM_NAME, UNDERLYING_TYPE, ...)                                                    \
        class __FRENUM_DATA(ENUM_NAME)                                                                                          \
        {                                                                                                                       \
        public:                                                                                                                 \
            using enum_type = NAMESPACE ENUM_NAME;                                                                              \
            using underlying_type = UNDERLYING_TYPE;                                                                            \
            static constexpr std::string_view name = __FRENUM_STRINGIZE(ENUM_NAME);                                             \
            static constexpr std::array<std::pair<underlying_type, std::string_view>, __FRENUM_ARG_SIZE(__VA_ARGS__)> values =  \
            {                                                                                                                   \
                { __FRENUM_FOR_EACH(NAMESPACE, ENUM_NAME, __VA_ARGS__) }                                                        \
            };                                                                                                                  \
            static constexpr std::string_view to_sv(enum_type value)                                                            \
            {                                                                                                                   \
                for (const auto& [key, name] : values)                                                                          \
                {                                                                                                               \
                    if (key == static_cast<underlying_type>(value))                                                             \
                        return name;                                                                                            \
                }                                                                                                               \
                return {};                                                                                                      \
            }                                                                                                                   \
            static constexpr std::string to_string(enum_type value)                                                             \
            {                                                                                                                   \
                for (const auto& [key, name] : values)                                                                          \
                {                                                                                                               \
                    if (key == static_cast<underlying_type>(value))                                                             \
                        return std::string{name};                                                                               \
                }                                                                                                               \
                return {};                                                                                                      \
            }                                                                                                                   \
            static constexpr std::optional<enum_type> from_sv(std::string_view sv)                                              \
            {                                                                                                                   \
                for (const auto& [key, name] : values)                                                                          \
                {                                                                                                               \
                    if (name == sv)                                                                                             \
                        return (enum_type)key;                                                                                  \
                }                                                                                                               \
                return std::nullopt;                                                                                            \
            }                                                                                                                   \
            static constexpr std::optional<enum_type> from_string(const std::string& s)                                         \
            {                                                                                                                   \
                for (const auto& [key, name] : values)                                                                          \
                {                                                                                                               \
                    if (std::string{name} == s)                                                                                 \
                        return (enum_type)key;                                                                                  \
                }                                                                                                               \
                return std::nullopt;                                                                                            \
            }                                                                                                                   \
            static constexpr std::optional<size_t> to_index(enum_type value)                                                    \
            {                                                                                                                   \
                for (size_t i = 0; i < values.size(); ++i)                                                                      \
                {                                                                                                               \
                    if (values[i].first == static_cast<underlying_type>(value))                                                 \
                        return i;                                                                                               \
                }                                                                                                               \
                return std::nullopt;                                                                                            \
            }                                                                                                                   \
    };




#ifdef Frenum
#undef Frenum
#endif
#define Frenum(ENUM_NAME, UNDERLYING_TYPE, ...)                                                                 \
    __FRENUM_ENUM(enum, ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)                                                \
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_EMPTY_NAMESPACE, ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)                 \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_EMPTY_NAMESPACE, ENUM_NAME)

#ifdef FrenumClass
#undef FrenumClass
#endif
#define FrenumClass(ENUM_NAME, UNDERLYING_TYPE, ...)                                                            \
    __FRENUM_ENUM(enum class, ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)                                          \
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_EMPTY_NAMESPACE, ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)                 \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_EMPTY_NAMESPACE, ENUM_NAME)

#ifdef FrenumInNamespace
#undef FrenumInNamespace
#endif
#define FrenumInNamespace(NAMESPACE, ENUM_NAME, UNDERLYING_TYPE, ...)                                           \
    __FRENUM_ENUM(enum, ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)                                                \
    __FRENUM_NAMESPACE_END                                                                                      \
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)         \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME)                                    \
    __FRENUM_NAMESPACE_BEGIN(NAMESPACE) 

#ifdef FrenumClassInNamespace
#undef FrenumClassInNamespace
#endif
#define FrenumClassInNamespace(NAMESPACE, ENUM_NAME, UNDERLYING_TYPE, ...)                                      \
     __FRENUM_ENUM(enum class, ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)                                         \
    __FRENUM_NAMESPACE_END                                                                                      \
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME, UNDERLYING_TYPE, __VA_ARGS__)         \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME)                                    \
    __FRENUM_NAMESPACE_BEGIN(NAMESPACE) 

#ifdef MakeFrenum
#undef MakeFrenum
#endif
#define MakeFrenum(ENUM_NAME, ...)                                                                              \
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_EMPTY_NAMESPACE, ENUM_NAME, __FRENUM_UTT(ENUM_NAME), __VA_ARGS__)         \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_EMPTY_NAMESPACE, ENUM_NAME)

#ifdef MakeFrenumWithNamespace
#undef MakeFrenumWithNamespace
#endif
#define MakeFrenumWithNamespace(NAMESPACE, ENUM_NAME, ...)                                                                                                  \
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME, __FRENUM_UTT(__FRENUM_AS_NAMESPACE(NAMESPACE) ENUM_NAME), __VA_ARGS__)            \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME)

#ifdef MakeFrenumInNamespace
#undef MakeFrenumInNamespace
#endif
#define MakeFrenumInNamespace(NAMESPACE, ENUM_NAME, ...)                                                                                                    \
    __FRENUM_NAMESPACE_END																																	\
    __FRENUM_ENUM_DATA_CLASS(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME, __FRENUM_UTT(__FRENUM_AS_NAMESPACE(NAMESPACE) ENUM_NAME), __VA_ARGS__)            \
    __FRENUM_REGISTER_ENUM_DATA(__FRENUM_AS_NAMESPACE(NAMESPACE), ENUM_NAME)																				\
    __FRENUM_NAMESPACE_BEGIN(NAMESPACE) 

namespace frenum
{
	namespace detail
	{
		template <class T>
		concept enumerator = std::is_enum_v<T>;

		constexpr bool is_space(int ch)
		{
			return ch == '\n' || ch == '\t' || ch == '\r' || ch == ' ';
		}

		constexpr std::string_view trim(std::string_view sv)
		{
			if (sv.empty())
				return sv;
			size_t lc = 0, rc = 0;
			while (is_space(sv[lc]))
				lc++;
			while (is_space(sv[sv.size() - 1 - rc]))
				rc++;
			return sv.substr(lc, sv.size() - (lc + rc));
		};

		constexpr std::string_view raw_name(std::string_view name_with_value)
		{
			auto pos = name_with_value.find('=');
			if (pos == std::string_view::npos)
				return trim(name_with_value);
			return trim(name_with_value.substr(0, pos));
		}

		template <typename T>
		struct not_assignable_value
		{
			constexpr explicit not_assignable_value(T value) : value(value) {}
			constexpr operator T() const { return value; }
			constexpr const not_assignable_value& operator =(int) const { return *this; }
			T value;
		};
	}

	inline constexpr size_t max_size = __FRENUM_FOR_EACH_MAX;

	template <class T>
	struct enum_data_registry
	{
		using type = void;
	};

	template <class T>
	inline constexpr bool is_frenum_v = !std::is_void_v<typename enum_data_registry<T>::type>;

	template <class T>
	concept frenum_type = is_frenum_v<T>;

	template <detail::enumerator T>
	inline constexpr std::underlying_type_t<T> underlying(T v)
	{
		return static_cast<std::underlying_type_t<T>>(v);
	}

	template <detail::enumerator T>
	inline constexpr std::underlying_type_t<T> value(T v)
	{
		return static_cast<std::underlying_type_t<T>>(v);
	}

	template <frenum_type T>
	inline constexpr std::optional<size_t> index(T v)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::to_index(v);
	}

	template <frenum_type T>
	inline constexpr std::string_view name()
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::name;
	}

	template <frenum_type T>
	inline constexpr std::string to_string(T v)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::to_string(v);
	}

	template <frenum_type T>
	inline constexpr std::string_view to_string_view(T v)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::to_sv(v);
	}

	template <frenum_type T>
	inline constexpr std::optional<T> cast(std::string_view view)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::from_sv(view);
	}

	template <frenum_type T>
	inline constexpr bool contains(const std::string& str)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::from_string(str).has_value();
	}

	template <frenum_type T>
	inline constexpr bool contains(std::string_view view)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::from_sv(view).has_value();
	}

	template <detail::enumerator T>
	inline constexpr bool contains(T v)
	{
		using enum_data = typename enum_data_registry<T>::type;
		return frenum::index<T>(v).has_value();
	}

	template <frenum_type T>
	inline constexpr size_t size()
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::values.size();
	}

	template <frenum_type T>
	inline constexpr auto begin() -> decltype(enum_data_registry<T>::type::values.begin())
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::values.begin();
	}

	template <frenum_type T>
	inline constexpr auto end() -> decltype(enum_data_registry<T>::type::values.end())
	{
		using enum_data = typename enum_data_registry<T>::type;
		return enum_data::values.end();
	}
}

#endif // !_FRENUM_DEFINED_