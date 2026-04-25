#include "std_math.h"

#include <cmath>
#include <limits>

namespace wio::runtime::std_math
{
#define ABS_IMPL(FUN, T) T FUN(T v) { if(v == (std::numeric_limits<T>::min)()) return (std::numeric_limits<T>::max)(); return (v > 0) ? v : -v; }
    ABS_IMPL(AbsI8, std::int8_t)
    ABS_IMPL(AbsI16, std::int16_t)
    ABS_IMPL(AbsI32, std::int32_t)
    ABS_IMPL(AbsI64, std::int64_t)
    ABS_IMPL(AbsF32, float)
    ABS_IMPL(AbsF64, double)
#undef ABS_IMPL

#define ASIN_IMPL(FUN, T) T FUN(T v) { return std::asin(v); }
    ASIN_IMPL(AsinF32, float)
    ASIN_IMPL(AsinF64, double)
#undef ASIN_IMPL
}