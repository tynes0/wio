#pragma once

#include <cstdint>
#include <type_traits>

#define DEFINE_INT_BODIES_1(FUNC_NAME)          \
    std::int8_t FUNC_NAME##I8(std::int8_t);     \
    std::int16_t FUNC_NAME##I16(std::int16_t);  \
    std::int32_t FUNC_NAME##I32(std::int32_t);  \
    std::int64_t FUNC_NAME##I64(std::int64_t);  

#define DEFINE_INT_BODIES_2(FUNC_NAME)                      \
    std::int8_t FUNC_NAME##I8(std::int8_t, std::int8_t);    \
    std::int16_t FUNC_NAME##I16(std::int16_t, std::int16_t);\
    std::int32_t FUNC_NAME##I32(std::int32_t, std::int32_t);\
    std::int64_t FUNC_NAME##I64(std::int64_t, std::int64_t);  

#define DEFINE_UINT_BODIES_1(FUNC_NAME)         \
    std::uint8_t FUNC_NAME##U8(std::uint8_t);   \
    std::uint16_t FUNC_NAME##U16(std::uint16_t);\
    std::uint32_t FUNC_NAME##U32(std::uint32_t);\
    std::uint64_t FUNC_NAME##U64(std::uint64_t);  

#define DEFINE_UINT_BODIES_2(FUNC_NAME)                         \
    std::uint8_t FUNC_NAME##U8(std::uint8_t, std::uint8_t);     \
    std::uint16_t FUNC_NAME##U16(std::uint16_t, std::uint16_t); \
    std::uint32_t FUNC_NAME##U32(std::uint32_t, std::uint32_t); \
    std::uint64_t FUNC_NAME##U64(std::uint64_t, std::uint64_t);  

    
#define DEFINE_FLOAT_BODIES_1(FUNC_NAME)\
    float FUNC_NAME##F32(float);        \
    double FUNC_NAME##F64(double);  

#define DEFINE_FLOAT_BODIES_2(FUNC_NAME)        \
    std::int32_t FUNC_NAME##I32(float, float);  \
    std::int64_t FUNC_NAME##I64(double, double);  

#define INT_BODY_IMPL(FUNC_NAME, ...)               \
    if constexpr (std::is_same_v<T, std::int8_t>)   \
        return FUNC_NAME##I8(__VA_ARGS__);          \
    if constexpr (std::is_same_v<T, std::int16_t>)  \
        return FUNC_NAME##I16(__VA_ARGS__);         \
    if constexpr (std::is_same_v<T, std::int32_t>)  \
        return FUNC_NAME##I32(__VA_ARGS__);         \
    if constexpr (std::is_same_v<T, std::int64_t>)  \
        return FUNC_NAME##I64(__VA_ARGS__); 

#define UINT_BODY_IMPL(FUNC_NAME, ...)              \
    if constexpr (std::is_same_v<T, std::uint8_t>)  \
        return FUNC_NAME##U8(__VA_ARGS__);          \
    if constexpr (std::is_same_v<T, std::uint16_t>) \
        return FUNC_NAME##U16(__VA_ARGS__);         \
    if constexpr (std::is_same_v<T, std::uint32_t>) \
        return FUNC_NAME##U32(__VA_ARGS__);         \
    if constexpr (std::is_same_v<T, std::uint64_t>) \
        return FUNC_NAME##U64(__VA_ARGS__); 

#define FLOAT_BODY_IMPL(FUNC_NAME, ...)     \
    if constexpr (std::is_same_v<T, float>) \
        return FUNC_NAME##F32(__VA_ARGS__); \
    if constexpr (std::is_same_v<T, double>)\
        return FUNC_NAME##F64(__VA_ARGS__); 

#define DEF_FUNC_I(FUNCTION_NAME) DEFINE_INT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { INT_BODY_IMPL(FUNCTION_NAME, v) return v; }
#define DEF_FUNC_U(FUNCTION_NAME) DEFINE_UINT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { UINT_BODY_IMPL(FUNCTION_NAME, v) return v; }
#define DEF_FUNC_F(FUNCTION_NAME) DEFINE_FLOAT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { FLOAT_BODY_IMPL(FUNCTION_NAME, v) return v; }
#define DEF_FUNC_IF(FUNCTION_NAME) DEFINE_INT_BODIES_1(FUNCTION_NAME) DEFINE_FLOAT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { INT_BODY_IMPL(FUNCTION_NAME, v) FLOAT_BODY_IMPL(FUNCTION_NAME, v) return v; }
#define DEF_FUNC_IU(FUNCTION_NAME) DEFINE_INT_BODIES_1(FUNCTION_NAME) DEFINE_UINT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { INT_BODY_IMPL(FUNCTION_NAME, v) UINT_BODY_IMPL(FUNCTION_NAME, v) return v; }
#define DEF_FUNC_UF(FUNCTION_NAME) DEFINE_UINT_BODIES_1(FUNCTION_NAME) DEFINE_FLOAT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { UINT_BODY_IMPL(FUNCTION_NAME, v) FLOAT_BODY_IMPL(FUNCTION_NAME, v) return v; }
#define DEF_FUNC_IUF(FUNCTION_NAME) DEFINE_INT_BODIES_1(FUNCTION_NAME) DEFINE_UINT_BODIES_1(FUNCTION_NAME) DEFINE_FLOAT_BODIES_1(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v) { INT_BODY_IMPL(FUNCTION_NAME, v) UINT_BODY_IMPL(FUNCTION_NAME, v) FLOAT_BODY_IMPL((FUNCTION_NAME), v), return v; }

#define DEF_FUNC2_I(FUNCTION_NAME) DEFINE_INT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { INT_BODY_IMPL(FUNCTION_NAME, v1, v2) return v1; }
#define DEF_FUNC2_U(FUNCTION_NAME) DEFINE_UINT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { UINT_BODY_IMPL(FUNCTION_NAME, v1, v2) return v1; }
#define DEF_FUNC2_F(FUNCTION_NAME) DEFINE_FLOAT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { FLOAT_BODY_IMPL(FUNCTION_NAME, v1, v2) return v1; }
#define DEF_FUNC2_IF(FUNCTION_NAME) DEFINE_INT_BODIES_2(FUNCTION_NAME) DEFINE_FLOAT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { INT_BODY_IMPL(FUNCTION_NAME, v1, v2) FLOAT_BODY_IMPL(FUNCTION_NAME, v1, v2) return v1; }
#define DEF_FUNC2_IU(FUNCTION_NAME) DEFINE_INT_BODIES_2(FUNCTION_NAME) DEFINE_UINT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { INT_BODY_IMPL(FUNCTION_NAME, v1, v2) UINT_BODY_IMPL(FUNCTION_NAME, v1, v2) return v1; }
#define DEF_FUNC2_UF(FUNCTION_NAME) DEFINE_UINT_BODIES_2(FUNCTION_NAME) DEFINE_FLOAT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { UINT_BODY_IMPL(FUNCTION_NAME, v1, v2) FLOAT_BODY_IMPL(FUNCTION_NAME, v1, v2) return v1; }
#define DEF_FUNC2_IUF(FUNCTION_NAME) DEFINE_INT_BODIES_2(FUNCTION_NAME) DEFINE_UINT_BODIES_2(FUNCTION_NAME) DEFINE_FLOAT_BODIES_2(FUNCTION_NAME) template <typename T> T FUNCTION_NAME(T v1, T v2) { INT_BODY_IMPL(FUNCTION_NAME, v1, v2) UINT_BODY_IMPL(FUNCTION_NAME, v1, v2) FLOAT_BODY_IMPL(FUNCTION_NAME), v1, v2, return v1; }

namespace wio::runtime::std_math
{
    DEF_FUNC_IF(Abs);
    DEF_FUNC_F(Asin);
}

#undef DEFINE_INT_BODIES_1
#undef DEFINE_INT_BODIES_2
#undef DEFINE_UINT_BODIES_1
#undef DEFINE_UINT_BODIES_2
#undef DEFINE_FLOAT_BODIES_1
#undef DEFINE_FLOAT_BODIES_2
#undef INT_BODY_IMPL
#undef UINT_BODY_IMPL
#undef FLOAT_BODY_IMPL
#undef DEF_FUNC_I
#undef DEF_FUNC_U
#undef DEF_FUNC_F
#undef DEF_FUNC_IF
#undef DEF_FUNC_IU
#undef DEF_FUNC_UF
#undef DEF_FUNC_IUF
#undef DEF_FUNC2_I
#undef DEF_FUNC2_U
#undef DEF_FUNC2_F
#undef DEF_FUNC2_IF
#undef DEF_FUNC2_IU
#undef DEF_FUNC2_UF
#undef DEF_FUNC2_IUF