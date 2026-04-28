#include "std_math.h"

#include <bit>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>

namespace wio::runtime::std_math
{
    namespace
    {
        template <class T> constexpr T min_v() noexcept { return (std::numeric_limits<T>::min)(); }
        template <class T> constexpr T max_v() noexcept { return (std::numeric_limits<T>::max)(); }

        template <class T>
        constexpr T clamp_impl(T v, T lo, T hi) noexcept
        {
            if (lo > hi) { const T tmp = lo; lo = hi; hi = tmp; }
            return v < lo ? lo : (hi < v ? hi : v);
        }

        template <class T>
        constexpr T abs_signed_saturating_impl(T v) noexcept
        {
            return v == min_v<T>() ? max_v<T>() : (v < T(0) ? T(-v) : v);
        }

        template <class T>
        T lerp_impl(T a, T b, T t) noexcept
        {
            if (std::isnan(a) || std::isnan(b) || std::isnan(t)) return std::numeric_limits<T>::quiet_NaN();
            if (t == T(0)) return a;  // NOLINT(clang-diagnostic-float-equal)
            if (t == T(1)) return b;  // NOLINT(clang-diagnostic-float-equal)
            if (a == b) return a;  // NOLINT(clang-diagnostic-float-equal)
            if (std::isinf(a) || std::isinf(b))
            {
                if (std::isinf(a) && std::isinf(b) && std::signbit(a) != std::signbit(b))
                    return std::numeric_limits<T>::quiet_NaN();
                return a + t * (b - a);
            }
            return (t > T(0) && t < T(1)) ? a + t * (b - a) : t * b + (T(1) - t) * a;
        }

        template <class T>
        T inv_lerp_impl(T a, T b, T value) noexcept
        {
            if (std::isnan(a) || std::isnan(b) || std::isnan(value)) return std::numeric_limits<T>::quiet_NaN();
            if (a == b) return T(0);  // NOLINT(clang-diagnostic-float-equal)
            return (value - a) / (b - a);
        }

        template <class T>
        bool approx_eq_impl(T a, T b, T eps) noexcept
        {
            if (a == b) return true;  // NOLINT(clang-diagnostic-float-equal)
            if (std::isnan(a) || std::isnan(b) || std::isnan(eps)) return false;
            eps = std::fabs(eps);
            const T diff = std::fabs(a - b);
            const T scale = (std::max)(T(1), (std::max)(std::fabs(a), std::fabs(b)));
            return diff <= eps * scale;
        }

        template <class T>
        T smoothstep_impl(T e0, T e1, T x) noexcept
        {
            const T t = clamp_impl(inv_lerp_impl(e0, e1, x), T(0), T(1));
            return t * t * (T(3) - T(2) * t);
        }

        template <class T>
        T smootherstep_impl(T e0, T e1, T x) noexcept
        {
            const T t = clamp_impl(inv_lerp_impl(e0, e1, x), T(0), T(1));
            return t * t * t * (t * (t * T(6) - T(15)) + T(10));
        }

        template <class T>
        T wrap_impl(T value, T period) noexcept
        {
            if (period == T(0) || std::isnan(value) || std::isnan(period)) return std::numeric_limits<T>::quiet_NaN();  // NOLINT(clang-diagnostic-float-equal)
            const T p = std::fabs(period);
            T r = std::fmod(value, p);
            if (r < T(0)) r += p;
            return r;
        }

        template <class T>
        T normalize_angle_rad_impl(T a) noexcept { return wrap_impl(a + T(Pi), T(Tau)) - T(Pi); }
        template <class T>
        T normalize_angle_deg_impl(T a) noexcept { return wrap_impl(a + T(180), T(360)) - T(180); }

        template <class T>
        T fast_rsqrt_impl(T x) noexcept
        {
            if (std::isnan(x)) return std::numeric_limits<T>::quiet_NaN();
            if (x < T(0)) return std::numeric_limits<T>::quiet_NaN();
            if (x == T(0)) return std::numeric_limits<T>::infinity();  // NOLINT(clang-diagnostic-float-equal)
            if (std::isinf(x)) return T(0);
            if constexpr (std::is_same_v<T, float>)
            {
                const float half = 0.5f * x;
                std::uint32_t i = std::bit_cast<std::uint32_t>(x);
                i = 0x5f3759dfu - (i >> 1);
                float y = std::bit_cast<float>(i);
                y = y * (1.5f - half * y * y);
                return y;
            }
            else
            {
                return T(1) / std::sqrt(x);
            }
        }

        template <class T>
        T fast_cbrt_impl(T x) noexcept
        {
            if (std::isnan(x) || std::isinf(x) || x == T(0)) return x;  // NOLINT(clang-diagnostic-float-equal)
            if constexpr (std::is_same_v<T, float>)
            {
                const float ax = std::fabs(x);
                std::uint32_t i = std::bit_cast<std::uint32_t>(ax);
                i = i / 3u + 709921077u;
                float y = std::bit_cast<float>(i);
                y = (2.0f * y + ax / (y * y)) * (1.0f / 3.0f);
                y = (2.0f * y + ax / (y * y)) * (1.0f / 3.0f);
                return std::copysign(y, x);
            }
            else
            {
                return std::cbrt(x);
            }
        }

        template <class T>
        CheckedResult<T> checked_add_impl(T a, T b) noexcept
        {
            if constexpr (std::is_unsigned_v<T>)
            {
                const T r = T(a + b);
                return {r, r < a};
            }
            else
            {
                using Lim = std::numeric_limits<T>;
                const bool ov = (b > 0 && a > Lim::max() - b) || (b < 0 && a < Lim::min() - b);
                return {ov ? (b > 0 ? Lim::max() : Lim::min()) : T(a + b), ov};
            }
        }

        template <class T>
        CheckedResult<T> checked_sub_impl(T a, T b) noexcept
        {
            if constexpr (std::is_unsigned_v<T>)
            {
                return {T(a - b), a < b};
            }
            else
            {
                using Lim = std::numeric_limits<T>;
                const bool ov = (b < 0 && a > Lim::max() + b) || (b > 0 && a < Lim::min() + b);
                return {ov ? (b < 0 ? Lim::max() : Lim::min()) : T(a - b), ov};
            }
        }

        template <class T>
        CheckedResult<T> checked_mul_impl(T a, T b) noexcept
        {
#if defined(__SIZEOF_INT128__)
            if constexpr (std::is_signed_v<T>)
            {
                const __int128 r = static_cast<__int128>(a) * static_cast<__int128>(b);
                const bool ov = r > max_v<T>() || r < min_v<T>();
                return {ov ? (r < 0 ? min_v<T>() : max_v<T>()) : static_cast<T>(r), ov};
            }
            else
            {
                const unsigned __int128 r = static_cast<unsigned __int128>(a) * static_cast<unsigned __int128>(b);
                const bool ov = r > max_v<T>();
                return {ov ? max_v<T>() : static_cast<T>(r), ov};
            }
#else
            if (a == T(0) || b == T(0)) return {T(0), false};
            if constexpr (std::is_unsigned_v<T>)
            {
                const bool ov = a > max_v<T>() / b;
                return {ov ? max_v<T>() : T(a * b), ov};
            }
            else
            {
                long double r = static_cast<long double>(a) * static_cast<long double>(b);
                const bool ov = r > static_cast<long double>(max_v<T>()) || r < static_cast<long double>(min_v<T>());
                return {ov ? (r < 0 ? min_v<T>() : max_v<T>()) : T(a * b), ov};
            }
#endif
        }

        template <class T> T sat_add_impl(T a, T b) noexcept { return checked_add_impl(a, b).value; }
        template <class T> T sat_sub_impl(T a, T b) noexcept { return checked_sub_impl(a, b).value; }
        template <class T> T sat_mul_impl(T a, T b) noexcept { return checked_mul_impl(a, b).value; }

        template <class T>
        T gcd_impl(T a, T b) noexcept
        {
            if constexpr (std::is_signed_v<T>)
            {
                using U = std::make_unsigned_t<T>;
                const U ua = static_cast<U>(a < 0 ? -static_cast<std::intmax_t>(a) : a);
                const U ub = static_cast<U>(b < 0 ? -static_cast<std::intmax_t>(b) : b);
                return static_cast<T>(std::gcd(ua, ub));
            }
            else return std::gcd(a, b);
        }

        template <class T>
        T lcm_impl(T a, T b) noexcept
        {
            if (a == T(0) || b == T(0)) return T(0);
            const auto g = gcd_impl(a, b);
            return sat_mul_impl<T>(T(a / g), b);
        }

        template <class T>
        T next_pow2_impl(T v) noexcept
        {
            if (v <= T(1)) return T(1);
            if (v > (T(1) << (sizeof(T) * 8 - 1))) return T(0);
            --v;
            for (std::size_t s = 1; s < sizeof(T) * 8; s <<= 1) v = T(v | (v >> s));
            return T(v + 1);
        }

        template <class T>
        T prev_pow2_impl(T v) noexcept
        {
            if (v == T(0)) return T(0);
            for (std::size_t s = 1; s < sizeof(T) * 8; s <<= 1) v = T(v | (v >> s));
            return T(v - (v >> 1));
        }
    }

#define WIO_I1_IMPL(name, expr)                                           \
    std::int8_t name##I8(std::int8_t v) noexcept                         \
    {                                                                     \
        return static_cast<std::int8_t>(expr);                            \
    }                                                                     \
    std::int16_t name##I16(std::int16_t v) noexcept                       \
    {                                                                     \
        return static_cast<std::int16_t>(expr);                           \
    }                                                                     \
    std::int32_t name##I32(std::int32_t v) noexcept                       \
    {                                                                     \
        return static_cast<std::int32_t>(expr);                           \
    }                                                                     \
    std::int64_t name##I64(std::int64_t v) noexcept                       \
    {                                                                     \
        return static_cast<std::int64_t>(expr);                           \
    }
#define WIO_U1_IMPL(name, expr)                                             \
    std::uint8_t name##U8(std::uint8_t v) noexcept                          \
    {                                                                       \
        return static_cast<std::uint8_t>(expr);                             \
    }                                                                       \
    std::uint16_t name##U16(std::uint16_t v) noexcept                       \
    {                                                                       \
        return static_cast<std::uint16_t>(expr);                            \
    }                                                                       \
    std::uint32_t name##U32(std::uint32_t v) noexcept                       \
    {                                                                       \
        return static_cast<std::uint32_t>(expr);                            \
    }                                                                       \
    std::uint64_t name##U64(std::uint64_t v) noexcept                       \
    {                                                                       \
        return static_cast<std::uint64_t>(expr);                            \
    }
#define WIO_F1_IMPL(name, fexpr, dexpr)            \
    float name##F32(float v) noexcept              \
    {                                              \
        return fexpr;                              \
    }                                              \
    double name##F64(double v) noexcept            \
    {                                              \
        return dexpr;                              \
    }
#define WIO_I2_IMPL(name, expr)                                                   \
    std::int8_t name##I8(std::int8_t a, std::int8_t b) noexcept                  \
    {                                                                            \
        return static_cast<std::int8_t>(expr);                                   \
    }                                                                            \
    std::int16_t name##I16(std::int16_t a, std::int16_t b) noexcept              \
    {                                                                            \
        return static_cast<std::int16_t>(expr);                                  \
    }                                                                            \
    std::int32_t name##I32(std::int32_t a, std::int32_t b) noexcept              \
    {                                                                            \
        return static_cast<std::int32_t>(expr);                                  \
    }                                                                            \
    std::int64_t name##I64(std::int64_t a, std::int64_t b) noexcept              \
    {                                                                            \
        return static_cast<std::int64_t>(expr);                                  \
    }
#define WIO_U2_IMPL(name, expr)                                                     \
    std::uint8_t name##U8(std::uint8_t a, std::uint8_t b) noexcept                  \
    {                                                                              \
        return static_cast<std::uint8_t>(expr);                                    \
    }                                                                              \
    std::uint16_t name##U16(std::uint16_t a, std::uint16_t b) noexcept             \
    {                                                                              \
        return static_cast<std::uint16_t>(expr);                                   \
    }                                                                              \
    std::uint32_t name##U32(std::uint32_t a, std::uint32_t b) noexcept             \
    {                                                                              \
        return static_cast<std::uint32_t>(expr);                                   \
    }                                                                              \
    std::uint64_t name##U64(std::uint64_t a, std::uint64_t b) noexcept             \
    {                                                                              \
        return static_cast<std::uint64_t>(expr);                                   \
    }
#define WIO_F2_IMPL(name, fexpr, dexpr)            \
    float name##F32(float a, float b) noexcept     \
    {                                              \
        return fexpr;                              \
    }                                              \
    double name##F64(double a, double b) noexcept  \
    {                                              \
        return dexpr;                              \
    }
#define WIO_I3_IMPL(name, expr)                                                             \
    std::int8_t name##I8(std::int8_t a, std::int8_t b, std::int8_t c) noexcept              \
    {                                                                                       \
        return static_cast<std::int8_t>(expr);                                              \
    }                                                                                       \
    std::int16_t name##I16(std::int16_t a, std::int16_t b, std::int16_t c) noexcept         \
    {                                                                                       \
        return static_cast<std::int16_t>(expr);                                             \
    }                                                                                       \
    std::int32_t name##I32(std::int32_t a, std::int32_t b, std::int32_t c) noexcept         \
    {                                                                                       \
        return static_cast<std::int32_t>(expr);                                             \
    }                                                                                       \
    std::int64_t name##I64(std::int64_t a, std::int64_t b, std::int64_t c) noexcept         \
    {                                                                                       \
        return static_cast<std::int64_t>(expr);                                             \
    }
#define WIO_U3_IMPL(name, expr)                                                               \
    std::uint8_t name##U8(std::uint8_t a, std::uint8_t b, std::uint8_t c) noexcept            \
    {                                                                                         \
        return static_cast<std::uint8_t>(expr);                                               \
    }                                                                                         \
    std::uint16_t name##U16(std::uint16_t a, std::uint16_t b, std::uint16_t c) noexcept       \
    {                                                                                         \
        return static_cast<std::uint16_t>(expr);                                              \
    }                                                                                         \
    std::uint32_t name##U32(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept       \
    {                                                                                         \
        return static_cast<std::uint32_t>(expr);                                              \
    }                                                                                         \
    std::uint64_t name##U64(std::uint64_t a, std::uint64_t b, std::uint64_t c) noexcept       \
    {                                                                                         \
        return static_cast<std::uint64_t>(expr);                                              \
    }
#define WIO_F3_IMPL(name, fexpr, dexpr)                     \
    float name##F32(float a, float b, float c) noexcept     \
    {                                                       \
        return fexpr;                                       \
    }                                                       \
    double name##F64(double a, double b, double c) noexcept \
    {                                                       \
        return dexpr;                                       \
    }

    WIO_I1_IMPL(Abs, abs_signed_saturating_impl(v))
    WIO_F1_IMPL(Abs, std::fabs(v), std::fabs(v))
    WIO_I1_IMPL(SaturatingAbs, abs_signed_saturating_impl(v))

    WIO_I2_IMPL(Min, b < a ? b : a)
    WIO_U2_IMPL(Min, b < a ? b : a)
    WIO_F2_IMPL(Min, b < a ? b : a, b < a ? b : a)
    WIO_I2_IMPL(Max, a < b ? b : a)
    WIO_U2_IMPL(Max, a < b ? b : a)
    WIO_F2_IMPL(Max, a < b ? b : a, a < b ? b : a)
    WIO_I3_IMPL(Clamp, clamp_impl(a, b, c))
    WIO_U3_IMPL(Clamp, clamp_impl(a, b, c))
    WIO_F3_IMPL(Clamp, clamp_impl(a, b, c), clamp_impl(a, b, c))

    WIO_F1_IMPL(Saturate, clamp_impl(v, 0.0f, 1.0f), clamp_impl(v, 0.0, 1.0))
    WIO_I1_IMPL(Sign, (v > 0) - (v < 0))
    WIO_U1_IMPL(Sign, v == 0 ? 0 : 1)
    WIO_F1_IMPL(Sign, (v > 0.0f) - (v < 0.0f), (v > 0.0) - (v < 0.0))
    WIO_F1_IMPL(SignNonZero, std::signbit(v) ? -1.0f : 1.0f, std::signbit(v) ? -1.0 : 1.0)

    bool IsNanF32(float v) noexcept { return std::isnan(v); }

    bool IsNanF64(double v) noexcept { return std::isnan(v); }
    bool IsInfF32(float v) noexcept { return std::isinf(v); }

    bool IsInfF64(double v) noexcept { return std::isinf(v); }
    bool IsFiniteF32(float v) noexcept { return std::isfinite(v); }

    bool IsFiniteF64(double v) noexcept { return std::isfinite(v); }
    bool IsNormalF32(float v) noexcept { return std::isnormal(v); }

    bool IsNormalF64(double v) noexcept { return std::isnormal(v); }
    bool SignBitF32(float v) noexcept { return std::signbit(v); }

    bool SignBitF64(double v) noexcept { return std::signbit(v); }
    bool ApproxEqF32(float a, float b, float eps) noexcept { return approx_eq_impl(a, b, eps); }

    bool ApproxEqF64(double a, double b, double eps) noexcept { return approx_eq_impl(a, b, eps); }
    bool ApproxZeroF32(float v) noexcept { return approx_eq_impl(v, 0.0f, std::numeric_limits<float>::epsilon() * 8.0f); }

    bool ApproxZeroF64(double v) noexcept { return approx_eq_impl(v, 0.0, std::numeric_limits<double>::epsilon() * 8.0); }
    WIO_F2_IMPL(CopySign, std::copysign(a, b), std::copysign(a, b))
    WIO_F2_IMPL(NextAfter, std::nextafter(a, b), std::nextafter(a, b))

    WIO_F1_IMPL(Sin, std::sin(v), std::sin(v))
    WIO_F1_IMPL(Cos, std::cos(v), std::cos(v))
    WIO_F1_IMPL(Tan, std::tan(v), std::tan(v))
    WIO_F1_IMPL(Asin, std::asin(v), std::asin(v))
    WIO_F1_IMPL(Acos, std::acos(v), std::acos(v))
    WIO_F1_IMPL(Atan, std::atan(v), std::atan(v))
    WIO_F2_IMPL(Atan2, std::atan2(a, b), std::atan2(a, b))
    WIO_F1_IMPL(Sinh, std::sinh(v), std::sinh(v))
    WIO_F1_IMPL(Cosh, std::cosh(v), std::cosh(v))
    WIO_F1_IMPL(Tanh, std::tanh(v), std::tanh(v))
    WIO_F1_IMPL(Asinh, std::asinh(v), std::asinh(v))
    WIO_F1_IMPL(Acosh, std::acosh(v), std::acosh(v))
    WIO_F1_IMPL(Atanh, std::atanh(v), std::atanh(v))

    WIO_F1_IMPL(Exp, std::exp(v), std::exp(v))
    WIO_F1_IMPL(Exp2, std::exp2(v), std::exp2(v))
    WIO_F1_IMPL(Expm1, std::expm1(v), std::expm1(v))
    WIO_F1_IMPL(Log, std::log(v), std::log(v))
    WIO_F1_IMPL(Log2, std::log2(v), std::log2(v))
    WIO_F1_IMPL(Log10, std::log10(v), std::log10(v))
    WIO_F1_IMPL(Log1p, std::log1p(v), std::log1p(v))
    WIO_F2_IMPL(Pow, std::pow(a, b), std::pow(a, b))
    WIO_F1_IMPL(Sqrt, std::sqrt(v), std::sqrt(v))
    WIO_F1_IMPL(Cbrt, std::cbrt(v), std::cbrt(v))
    WIO_F2_IMPL(Hypot, std::hypot(a, b), std::hypot(a, b))
    WIO_F1_IMPL(Rsqrt, 1.0f / std::sqrt(v), 1.0 / std::sqrt(v))
    WIO_F1_IMPL(FastRsqrt, fast_rsqrt_impl(v), fast_rsqrt_impl(v))
    WIO_F1_IMPL(FastSqrt, v * fast_rsqrt_impl(v), v * fast_rsqrt_impl(v))
    WIO_F1_IMPL(FastCbrt, fast_cbrt_impl(v), fast_cbrt_impl(v))

    WIO_F1_IMPL(Floor, std::floor(v), std::floor(v))
    WIO_F1_IMPL(Ceil, std::ceil(v), std::ceil(v))
    WIO_F1_IMPL(Trunc, std::trunc(v), std::trunc(v))
    WIO_F1_IMPL(Round, std::round(v), std::round(v))
    WIO_F1_IMPL(NearbyInt, std::nearbyint(v), std::nearbyint(v))
    WIO_F2_IMPL(Fmod, std::fmod(a, b), std::fmod(a, b))
    WIO_F2_IMPL(Remainder, std::remainder(a, b), std::remainder(a, b))
    WIO_F2_IMPL(Fdim, std::fdim(a, b), std::fdim(a, b))
    WIO_F3_IMPL(Fma, std::fma(a, b, c), std::fma(a, b, c))
    WIO_F1_IMPL(Fract, v - std::floor(v), v - std::floor(v))

    WIO_F3_IMPL(Lerp, lerp_impl(a, b, c), lerp_impl(a, b, c))
    WIO_F3_IMPL(LerpClamped, lerp_impl(a,b,clamp_impl(c, 0.0f, 1.0f)), lerp_impl(a,b,clamp_impl(c, 0.0, 1.0)))
    WIO_F3_IMPL(InvLerp, inv_lerp_impl(a, b, c), inv_lerp_impl(a, b, c))
    WIO_F3_IMPL(InvLerpClamped, clamp_impl(inv_lerp_impl(a, b, c), 0.0f, 1.0f), clamp_impl(inv_lerp_impl(a, b, c), 0.0, 1.0))
    float RemapF32(
        float value,
        float in_min,
        float in_max,
        float out_min,
        float out_max) noexcept
    {
        return lerp_impl(out_min, out_max, inv_lerp_impl(in_min, in_max, value));
    }
    double RemapF64(
        double value,
        double in_min,
        double in_max,
        double out_min,
        double out_max) noexcept
    {
        return lerp_impl(out_min, out_max, inv_lerp_impl(in_min, in_max, value));
    }
    float RemapClampedF32(
        float value,
        float in_min,
        float in_max,
        float out_min,
        float out_max) noexcept
    {
        return lerp_impl(
            out_min,
            out_max,
            clamp_impl(inv_lerp_impl(in_min, in_max, value), 0.0f, 1.0f));
    }
    double RemapClampedF64(
        double value,
        double in_min,
        double in_max,
        double out_min,
        double out_max) noexcept
    {
        return lerp_impl(
            out_min,
            out_max,
            clamp_impl(inv_lerp_impl(in_min, in_max, value), 0.0, 1.0));
    }
    WIO_F3_IMPL(SmoothStep, smoothstep_impl(a, b, c), smoothstep_impl(a, b, c))
    WIO_F3_IMPL(SmootherStep, smootherstep_impl(a, b, c), smootherstep_impl(a, b, c))
    WIO_F2_IMPL(Step, b < a ? 0.0f : 1.0f, b < a ? 0.0 : 1.0)

    WIO_F1_IMPL(DegToRad, v * static_cast<float>(DegToRadC), v * DegToRadC)
    WIO_F1_IMPL(RadToDeg, v * static_cast<float>(RadToDegC), v * RadToDegC)
    WIO_F1_IMPL(NormalizeAngleRad, normalize_angle_rad_impl(v), normalize_angle_rad_impl(v))
    WIO_F1_IMPL(NormalizeAngleDeg, normalize_angle_deg_impl(v), normalize_angle_deg_impl(v))
    WIO_F2_IMPL(DeltaAngleRad, normalize_angle_rad_impl(b - a), normalize_angle_rad_impl(b - a))
    WIO_F2_IMPL(DeltaAngleDeg, normalize_angle_deg_impl(b - a), normalize_angle_deg_impl(b - a))
    WIO_F2_IMPL(Wrap, wrap_impl(a, b), wrap_impl(a, b))
    WIO_F2_IMPL(Repeat, wrap_impl(a, b), wrap_impl(a, b))
    WIO_F2_IMPL(PingPong, b == 0.0f ? std::numeric_limits<float>::quiet_NaN() : b - std::fabs(wrap_impl(a, b * 2.0f) - b), b == 0.0 ? std::numeric_limits<double>::quiet_NaN() : b - std::fabs(wrap_impl(a, b * 2.0) - b))

    WIO_I2_IMPL(Gcd, gcd_impl(a, b))
    WIO_U2_IMPL(Gcd, gcd_impl(a, b))
    WIO_I2_IMPL(Lcm, lcm_impl(a, b))
    WIO_U2_IMPL(Lcm, lcm_impl(a, b))
    WIO_U1_IMPL(PopCount, std::popcount(v))
    WIO_U1_IMPL(CountLeadingZeros, std::countl_zero(v))
    WIO_U1_IMPL(CountTrailingZeros, std::countr_zero(v))
    WIO_U2_IMPL(RotateLeft, std::rotl(a, static_cast<int>(b)))
    WIO_U2_IMPL(RotateRight, std::rotr(a, static_cast<int>(b)))
    WIO_U1_IMPL(NextPowerOfTwo, next_pow2_impl(v))
    WIO_U1_IMPL(PrevPowerOfTwo, prev_pow2_impl(v))
    bool IsPowerOfTwoU8(std::uint8_t v) noexcept { return v && !(v & (v - 1)); }

    bool IsPowerOfTwoU16(std::uint16_t v) noexcept { return v && !(v & (v - 1)); }

    bool IsPowerOfTwoU32(std::uint32_t v) noexcept { return v && !(v & (v - 1)); }

    bool IsPowerOfTwoU64(std::uint64_t v) noexcept { return v && !(v & (v - 1)); }
    bool IsEvenI8(std::int8_t v) noexcept { return (v & 1) == 0; }

    bool IsEvenI16(std::int16_t v) noexcept { return (v & 1) == 0; }

    bool IsEvenI32(std::int32_t v) noexcept { return (v & 1) == 0; }

    bool IsEvenI64(std::int64_t v) noexcept { return (v & 1) == 0; }
    bool IsEvenU8(std::uint8_t v) noexcept { return (v & 1u) == 0; }

    bool IsEvenU16(std::uint16_t v) noexcept { return (v & 1u) == 0; }

    bool IsEvenU32(std::uint32_t v) noexcept { return (v & 1u) == 0; }

    bool IsEvenU64(std::uint64_t v) noexcept { return (v & 1u) == 0; }
    bool IsOddI8(std::int8_t v) noexcept { return !IsEvenI8(v); }

    bool IsOddI16(std::int16_t v) noexcept { return !IsEvenI16(v); }

    bool IsOddI32(std::int32_t v) noexcept { return !IsEvenI32(v); }

    bool IsOddI64(std::int64_t v) noexcept { return !IsEvenI64(v); }
    bool IsOddU8(std::uint8_t v) noexcept { return !IsEvenU8(v); }

    bool IsOddU16(std::uint16_t v) noexcept { return !IsEvenU16(v); }

    bool IsOddU32(std::uint32_t v) noexcept { return !IsEvenU32(v); }

    bool IsOddU64(std::uint64_t v) noexcept { return !IsEvenU64(v); }

    WIO_I2_IMPL(SaturatingAdd, sat_add_impl(a, b))
    WIO_U2_IMPL(SaturatingAdd, sat_add_impl(a, b))
    WIO_I2_IMPL(SaturatingSub, sat_sub_impl(a, b))
    WIO_U2_IMPL(SaturatingSub, sat_sub_impl(a, b))
    WIO_I2_IMPL(SaturatingMul, sat_mul_impl(a, b))
    WIO_U2_IMPL(SaturatingMul, sat_mul_impl(a, b))

#define WIO_CHECKED_IMPL(op, name)                                                 \
    CheckedResult<std::int8_t> name##I8(std::int8_t a, std::int8_t b) noexcept     \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::int16_t> name##I16(std::int16_t a, std::int16_t b) noexcept \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::int32_t> name##I32(std::int32_t a, std::int32_t b) noexcept \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::int64_t> name##I64(std::int64_t a, std::int64_t b) noexcept \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::uint8_t> name##U8(std::uint8_t a, std::uint8_t b) noexcept  \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::uint16_t> name##U16(                                        \
        std::uint16_t a,                                                          \
        std::uint16_t b) noexcept                                                 \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::uint32_t> name##U32(                                        \
        std::uint32_t a,                                                          \
        std::uint32_t b) noexcept                                                 \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }                                                                             \
    CheckedResult<std::uint64_t> name##U64(                                        \
        std::uint64_t a,                                                          \
        std::uint64_t b) noexcept                                                 \
    {                                                                             \
        return op##_impl(a, b);                                                    \
    }

    WIO_CHECKED_IMPL(checked_add, CheckedAdd)
    WIO_CHECKED_IMPL(checked_sub, CheckedSub)
    WIO_CHECKED_IMPL(checked_mul, CheckedMul)

#undef WIO_CHECKED_IMPL
#undef WIO_I1_IMPL
#undef WIO_U1_IMPL
#undef WIO_F1_IMPL
#undef WIO_I2_IMPL
#undef WIO_U2_IMPL
#undef WIO_F2_IMPL
#undef WIO_I3_IMPL
#undef WIO_U3_IMPL
#undef WIO_F3_IMPL
}
