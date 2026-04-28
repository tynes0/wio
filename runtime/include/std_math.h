#pragma once

#include <cstdint>

namespace wio::runtime::std_math
{
    template <class T>
    struct CheckedResult
    {
        T value{};
        bool overflow{};
    };

    inline constexpr double Pi        = 3.141592653589793238462643383279502884;
    inline constexpr double Tau       = 6.283185307179586476925286766559005768;
    inline constexpr double HalfPi    = 1.570796326794896619231321691639751442;
    inline constexpr double E         = 2.718281828459045235360287471352662498;
    inline constexpr double Sqrt2     = 1.414213562373095048801688724209698079;
    inline constexpr double Ln2       = 0.693147180559945309417232121458176568;
    inline constexpr double Ln10      = 2.302585092994045684017991454684364208;
    inline constexpr double DegToRadC = Pi / 180.0;
    inline constexpr double RadToDegC = 180.0 / Pi;

#define WIO_I1(name)                                \
    std::int8_t name##I8(std::int8_t) noexcept;    \
    std::int16_t name##I16(std::int16_t) noexcept; \
    std::int32_t name##I32(std::int32_t) noexcept; \
    std::int64_t name##I64(std::int64_t) noexcept
#define WIO_U1(name)                                  \
    std::uint8_t name##U8(std::uint8_t) noexcept;     \
    std::uint16_t name##U16(std::uint16_t) noexcept;  \
    std::uint32_t name##U32(std::uint32_t) noexcept;  \
    std::uint64_t name##U64(std::uint64_t) noexcept
#define WIO_F1(name)                 \
    float name##F32(float) noexcept; \
    double name##F64(double) noexcept
#define WIO_BF1(name)              \
    bool name##F32(float) noexcept; \
    bool name##F64(double) noexcept
#define WIO_I2(name)                                             \
    std::int8_t name##I8(std::int8_t, std::int8_t) noexcept;     \
    std::int16_t name##I16(std::int16_t, std::int16_t) noexcept; \
    std::int32_t name##I32(std::int32_t, std::int32_t) noexcept; \
    std::int64_t name##I64(std::int64_t, std::int64_t) noexcept
#define WIO_U2(name)                                               \
    std::uint8_t name##U8(std::uint8_t, std::uint8_t) noexcept;    \
    std::uint16_t name##U16(std::uint16_t, std::uint16_t) noexcept; \
    std::uint32_t name##U32(std::uint32_t, std::uint32_t) noexcept; \
    std::uint64_t name##U64(std::uint64_t, std::uint64_t) noexcept
#define WIO_F2(name)                       \
    float name##F32(float, float) noexcept; \
    double name##F64(double, double) noexcept
#define WIO_BF2(name)                    \
    bool name##F32(float, float) noexcept; \
    bool name##F64(double, double) noexcept
#define WIO_I3(name)                                                           \
    std::int8_t name##I8(std::int8_t, std::int8_t, std::int8_t) noexcept;     \
    std::int16_t name##I16(std::int16_t, std::int16_t, std::int16_t) noexcept; \
    std::int32_t name##I32(std::int32_t, std::int32_t, std::int32_t) noexcept; \
    std::int64_t name##I64(std::int64_t, std::int64_t, std::int64_t) noexcept
#define WIO_U3(name)                                                             \
    std::uint8_t name##U8(std::uint8_t, std::uint8_t, std::uint8_t) noexcept;    \
    std::uint16_t name##U16(std::uint16_t, std::uint16_t, std::uint16_t) noexcept; \
    std::uint32_t name##U32(std::uint32_t, std::uint32_t, std::uint32_t) noexcept; \
    std::uint64_t name##U64(std::uint64_t, std::uint64_t, std::uint64_t) noexcept
#define WIO_F3(name)                                  \
    float name##F32(float, float, float) noexcept;    \
    double name##F64(double, double, double) noexcept
#define WIO_BF3(name)                              \
    bool name##F32(float, float, float) noexcept;  \
    bool name##F64(double, double, double) noexcept

    // Basic numeric ops
    WIO_I1(Abs);
    WIO_F1(Abs);
    WIO_I1(SaturatingAbs);
    WIO_I2(Min);
    WIO_U2(Min);
    WIO_F2(Min);
    WIO_I2(Max);
    WIO_U2(Max);
    WIO_F2(Max);
    WIO_I3(Clamp);
    WIO_U3(Clamp);
    WIO_F3(Clamp);
    WIO_F1(Saturate);
    WIO_I1(Sign);
    WIO_U1(Sign);
    WIO_F1(Sign);
    WIO_F1(SignNonZero);

    // Floating point predicates and helpers
    WIO_BF1(IsNan);
    WIO_BF1(IsInf);
    WIO_BF1(IsFinite);
    WIO_BF1(IsNormal);
    WIO_BF1(SignBit);
    WIO_BF3(ApproxEq); // value, other, epsilon
    WIO_BF1(ApproxZero);
    WIO_F2(CopySign);
    WIO_F2(NextAfter);

    // Trigonometry
    WIO_F1(Sin);
    WIO_F1(Cos);
    WIO_F1(Tan);
    WIO_F1(Asin);
    WIO_F1(Acos);
    WIO_F1(Atan);
    WIO_F2(Atan2);
    WIO_F1(Sinh);
    WIO_F1(Cosh);
    WIO_F1(Tanh);
    WIO_F1(Asinh);
    WIO_F1(Acosh);
    WIO_F1(Atanh);

    // Exponential / logarithmic / power
    WIO_F1(Exp);
    WIO_F1(Exp2);
    WIO_F1(Expm1);
    WIO_F1(Log);
    WIO_F1(Log2);
    WIO_F1(Log10);
    WIO_F1(Log1p);
    WIO_F2(Pow);
    WIO_F1(Sqrt);
    WIO_F1(Cbrt);
    WIO_F2(Hypot);
    WIO_F1(Rsqrt);
    WIO_F1(FastRsqrt);
    WIO_F1(FastSqrt);
    WIO_F1(FastCbrt);

    // Rounding / remainder / FMA
    WIO_F1(Floor);
    WIO_F1(Ceil);
    WIO_F1(Trunc);
    WIO_F1(Round);
    WIO_F1(NearbyInt);
    WIO_F2(Fmod);
    WIO_F2(Remainder);
    WIO_F2(Fdim);
    WIO_F3(Fma);
    WIO_F1(Fract);

    // Interpolation / remap / easing
    WIO_F3(Lerp);
    WIO_F3(LerpClamped);
    WIO_F3(InvLerp);
    WIO_F3(InvLerpClamped);
    float RemapF32(float value, float in_min, float in_max, float out_min, float out_max) noexcept;
    double RemapF64(double value, double in_min, double in_max, double out_min, double out_max) noexcept;
    float RemapClampedF32(float value, float in_min, float in_max, float out_min, float out_max) noexcept;
    double RemapClampedF64(double value, double in_min, double in_max, double out_min, double out_max) noexcept;
    WIO_F3(SmoothStep);
    WIO_F3(SmootherStep);
    WIO_F2(Step);

    // Angles / wrapping
    WIO_F1(DegToRad);
    WIO_F1(RadToDeg);
    WIO_F1(NormalizeAngleRad);
    WIO_F1(NormalizeAngleDeg);
    WIO_F2(DeltaAngleRad);
    WIO_F2(DeltaAngleDeg);
    WIO_F2(Wrap);
    WIO_F2(Repeat);
    WIO_F2(PingPong);

    // Integer math
    WIO_I2(Gcd);
    WIO_U2(Gcd);
    WIO_I2(Lcm);
    WIO_U2(Lcm);
    WIO_U1(PopCount);
    WIO_U1(CountLeadingZeros);
    WIO_U1(CountTrailingZeros);
    WIO_U2(RotateLeft);
    WIO_U2(RotateRight);
    WIO_U1(NextPowerOfTwo);
    WIO_U1(PrevPowerOfTwo);
    bool IsPowerOfTwoU8(std::uint8_t) noexcept;
    bool IsPowerOfTwoU16(std::uint16_t) noexcept;
    bool IsPowerOfTwoU32(std::uint32_t) noexcept;
    bool IsPowerOfTwoU64(std::uint64_t) noexcept;
    bool IsEvenI8(std::int8_t) noexcept;
    bool IsEvenI16(std::int16_t) noexcept;
    bool IsEvenI32(std::int32_t) noexcept;
    bool IsEvenI64(std::int64_t) noexcept;
    bool IsEvenU8(std::uint8_t) noexcept;
    bool IsEvenU16(std::uint16_t) noexcept;
    bool IsEvenU32(std::uint32_t) noexcept;
    bool IsEvenU64(std::uint64_t) noexcept;
    bool IsOddI8(std::int8_t) noexcept;
    bool IsOddI16(std::int16_t) noexcept;
    bool IsOddI32(std::int32_t) noexcept;
    bool IsOddI64(std::int64_t) noexcept;
    bool IsOddU8(std::uint8_t) noexcept;
    bool IsOddU16(std::uint16_t) noexcept;
    bool IsOddU32(std::uint32_t) noexcept;
    bool IsOddU64(std::uint64_t) noexcept;

    WIO_I2(SaturatingAdd);
    WIO_U2(SaturatingAdd);
    WIO_I2(SaturatingSub);
    WIO_U2(SaturatingSub);
    WIO_I2(SaturatingMul);
    WIO_U2(SaturatingMul);

    CheckedResult<std::int8_t> CheckedAddI8(std::int8_t, std::int8_t) noexcept;
    CheckedResult<std::int16_t> CheckedAddI16(std::int16_t, std::int16_t) noexcept;
    CheckedResult<std::int32_t> CheckedAddI32(std::int32_t, std::int32_t) noexcept;
    CheckedResult<std::int64_t> CheckedAddI64(std::int64_t, std::int64_t) noexcept;
    CheckedResult<std::uint8_t> CheckedAddU8(std::uint8_t, std::uint8_t) noexcept;
    CheckedResult<std::uint16_t> CheckedAddU16(std::uint16_t, std::uint16_t) noexcept;
    CheckedResult<std::uint32_t> CheckedAddU32(std::uint32_t, std::uint32_t) noexcept;
    CheckedResult<std::uint64_t> CheckedAddU64(std::uint64_t, std::uint64_t) noexcept;
    CheckedResult<std::int8_t> CheckedSubI8(std::int8_t, std::int8_t) noexcept;
    CheckedResult<std::int16_t> CheckedSubI16(std::int16_t, std::int16_t) noexcept;
    CheckedResult<std::int32_t> CheckedSubI32(std::int32_t, std::int32_t) noexcept;
    CheckedResult<std::int64_t> CheckedSubI64(std::int64_t, std::int64_t) noexcept;
    CheckedResult<std::uint8_t> CheckedSubU8(std::uint8_t, std::uint8_t) noexcept;
    CheckedResult<std::uint16_t> CheckedSubU16(std::uint16_t, std::uint16_t) noexcept;
    CheckedResult<std::uint32_t> CheckedSubU32(std::uint32_t, std::uint32_t) noexcept;
    CheckedResult<std::uint64_t> CheckedSubU64(std::uint64_t, std::uint64_t) noexcept;
    CheckedResult<std::int8_t> CheckedMulI8(std::int8_t, std::int8_t) noexcept;
    CheckedResult<std::int16_t> CheckedMulI16(std::int16_t, std::int16_t) noexcept;
    CheckedResult<std::int32_t> CheckedMulI32(std::int32_t, std::int32_t) noexcept;
    CheckedResult<std::int64_t> CheckedMulI64(std::int64_t, std::int64_t) noexcept;
    CheckedResult<std::uint8_t> CheckedMulU8(std::uint8_t, std::uint8_t) noexcept;
    CheckedResult<std::uint16_t> CheckedMulU16(std::uint16_t, std::uint16_t) noexcept;
    CheckedResult<std::uint32_t> CheckedMulU32(std::uint32_t, std::uint32_t) noexcept;
    CheckedResult<std::uint64_t> CheckedMulU64(std::uint64_t, std::uint64_t) noexcept;

#undef WIO_I1
#undef WIO_U1
#undef WIO_F1
#undef WIO_BF1
#undef WIO_I2
#undef WIO_U2
#undef WIO_F2
#undef WIO_BF2
#undef WIO_I3
#undef WIO_U3
#undef WIO_F3
#undef WIO_BF3
}
