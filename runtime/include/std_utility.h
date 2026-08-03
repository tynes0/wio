#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <string_view>

namespace wio::runtime::std_numeric
{
    inline bool CheckedAddI64(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept
    {
        if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
            (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
            return false;
        out = a + b;
        return true;
    }

    inline bool CheckedSubI64(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept
    {
        if ((b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) ||
            (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b))
            return false;
        out = a - b;
        return true;
    }

    inline bool CheckedMulI64(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept
    {
        if (a == 0 || b == 0) { out = 0; return true; }
        constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
        constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
        if (a > 0)
        {
            if ((b > 0 && a > maximum / b) || (b < 0 && b < minimum / a)) return false;
        }
        else
        {
            if ((b > 0 && a < minimum / b) || (b < 0 && a < maximum / b)) return false;
        }
        out = a * b;
        return true;
    }

    inline bool CheckedAddU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
    {
        if (a > std::numeric_limits<std::uint64_t>::max() - b) return false;
        out = a + b;
        return true;
    }

    inline bool CheckedSubU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
    {
        if (a < b) return false;
        out = a - b;
        return true;
    }

    inline bool CheckedMulU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
    {
        if (b != 0 && a > std::numeric_limits<std::uint64_t>::max() / b) return false;
        out = a * b;
        return true;
    }

    inline std::int64_t SaturatingAddI64(std::int64_t a, std::int64_t b) noexcept
    {
        std::int64_t out{};
        if (CheckedAddI64(a, b, out)) return out;
        return b > 0 ? std::numeric_limits<std::int64_t>::max() : std::numeric_limits<std::int64_t>::min();
    }

    inline std::int64_t SaturatingSubI64(std::int64_t a, std::int64_t b) noexcept
    {
        std::int64_t out{};
        if (CheckedSubI64(a, b, out)) return out;
        return b < 0 ? std::numeric_limits<std::int64_t>::max() : std::numeric_limits<std::int64_t>::min();
    }

    inline std::uint64_t SaturatingAddU64(std::uint64_t a, std::uint64_t b) noexcept
    {
        std::uint64_t out{};
        return CheckedAddU64(a, b, out) ? out : std::numeric_limits<std::uint64_t>::max();
    }

    inline std::uint64_t SaturatingSubU64(std::uint64_t a, std::uint64_t b) noexcept
    {
        std::uint64_t out{};
        return CheckedSubU64(a, b, out) ? out : 0;
    }

    inline std::int64_t SaturatingMulI64(std::int64_t a, std::int64_t b) noexcept
    {
        std::int64_t out{};
        if (CheckedMulI64(a, b, out)) return out;
        return (a < 0) != (b < 0)
            ? std::numeric_limits<std::int64_t>::min()
            : std::numeric_limits<std::int64_t>::max();
    }

    inline std::uint64_t SaturatingMulU64(std::uint64_t a, std::uint64_t b) noexcept
    {
        std::uint64_t out{};
        return CheckedMulU64(a, b, out) ? out : std::numeric_limits<std::uint64_t>::max();
    }

    template <typename T>
    inline bool CheckedAddSmallSigned(T a, T b, T& out) noexcept
    {
        const std::int64_t value = static_cast<std::int64_t>(a) + static_cast<std::int64_t>(b);
        if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
            value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(value);
        return true;
    }

    template <typename T>
    inline bool CheckedSubSmallSigned(T a, T b, T& out) noexcept
    {
        const std::int64_t value = static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b);
        if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
            value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(value);
        return true;
    }

    template <typename T>
    inline bool CheckedMulSmallSigned(T a, T b, T& out) noexcept
    {
        const std::int64_t value = static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b);
        if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
            value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(value);
        return true;
    }

    template <typename T>
    inline bool CheckedAddSmallUnsigned(T a, T b, T& out) noexcept
    {
        const std::uint64_t value = static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b);
        if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(value);
        return true;
    }

    template <typename T>
    inline bool CheckedSubSmallUnsigned(T a, T b, T& out) noexcept
    {
        if (a < b) return false;
        out = static_cast<T>(a - b);
        return true;
    }

    template <typename T>
    inline bool CheckedMulSmallUnsigned(T a, T b, T& out) noexcept
    {
        const std::uint64_t value = static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b);
        if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) return false;
        out = static_cast<T>(value);
        return true;
    }

#define WIO_SMALL_SIGNED_WIDTH(Suffix, Type) \
    inline bool CheckedAdd##Suffix(Type a, Type b, Type& out) noexcept { return CheckedAddSmallSigned(a, b, out); } \
    inline bool CheckedSub##Suffix(Type a, Type b, Type& out) noexcept { return CheckedSubSmallSigned(a, b, out); } \
    inline bool CheckedMul##Suffix(Type a, Type b, Type& out) noexcept { return CheckedMulSmallSigned(a, b, out); } \
    inline Type SaturatingAdd##Suffix(Type a, Type b) noexcept { Type out{}; return CheckedAdd##Suffix(a, b, out) ? out : (b > 0 ? std::numeric_limits<Type>::max() : std::numeric_limits<Type>::min()); } \
    inline Type SaturatingSub##Suffix(Type a, Type b) noexcept { Type out{}; return CheckedSub##Suffix(a, b, out) ? out : (b < 0 ? std::numeric_limits<Type>::max() : std::numeric_limits<Type>::min()); } \
    inline Type SaturatingMul##Suffix(Type a, Type b) noexcept { Type out{}; return CheckedMul##Suffix(a, b, out) ? out : ((a < 0) != (b < 0) ? std::numeric_limits<Type>::min() : std::numeric_limits<Type>::max()); }

#define WIO_SMALL_UNSIGNED_WIDTH(Suffix, Type) \
    inline bool CheckedAdd##Suffix(Type a, Type b, Type& out) noexcept { return CheckedAddSmallUnsigned(a, b, out); } \
    inline bool CheckedSub##Suffix(Type a, Type b, Type& out) noexcept { return CheckedSubSmallUnsigned(a, b, out); } \
    inline bool CheckedMul##Suffix(Type a, Type b, Type& out) noexcept { return CheckedMulSmallUnsigned(a, b, out); } \
    inline Type SaturatingAdd##Suffix(Type a, Type b) noexcept { Type out{}; return CheckedAdd##Suffix(a, b, out) ? out : std::numeric_limits<Type>::max(); } \
    inline Type SaturatingSub##Suffix(Type a, Type b) noexcept { Type out{}; return CheckedSub##Suffix(a, b, out) ? out : 0; } \
    inline Type SaturatingMul##Suffix(Type a, Type b) noexcept { Type out{}; return CheckedMul##Suffix(a, b, out) ? out : std::numeric_limits<Type>::max(); }

    WIO_SMALL_SIGNED_WIDTH(I8, std::int8_t)
    WIO_SMALL_SIGNED_WIDTH(I16, std::int16_t)
    WIO_SMALL_SIGNED_WIDTH(I32, std::int32_t)
    WIO_SMALL_UNSIGNED_WIDTH(U8, std::uint8_t)
    WIO_SMALL_UNSIGNED_WIDTH(U16, std::uint16_t)
    WIO_SMALL_UNSIGNED_WIDTH(U32, std::uint32_t)

    inline bool CheckedAddISize(std::ptrdiff_t a, std::ptrdiff_t b, std::ptrdiff_t& out) noexcept
    {
        if ((b > 0 && a > std::numeric_limits<std::ptrdiff_t>::max() - b) ||
            (b < 0 && a < std::numeric_limits<std::ptrdiff_t>::min() - b)) return false;
        out = a + b;
        return true;
    }
    inline bool CheckedSubISize(std::ptrdiff_t a, std::ptrdiff_t b, std::ptrdiff_t& out) noexcept
    {
        if ((b < 0 && a > std::numeric_limits<std::ptrdiff_t>::max() + b) ||
            (b > 0 && a < std::numeric_limits<std::ptrdiff_t>::min() + b)) return false;
        out = a - b;
        return true;
    }
    inline bool CheckedMulISize(std::ptrdiff_t a, std::ptrdiff_t b, std::ptrdiff_t& out) noexcept
    {
        if (a == 0 || b == 0) { out = 0; return true; }
        constexpr auto minimum = std::numeric_limits<std::ptrdiff_t>::min();
        constexpr auto maximum = std::numeric_limits<std::ptrdiff_t>::max();
        if (a > 0) { if ((b > 0 && a > maximum / b) || (b < 0 && b < minimum / a)) return false; }
        else { if ((b > 0 && a < minimum / b) || (b < 0 && a < maximum / b)) return false; }
        out = a * b;
        return true;
    }
    inline bool CheckedAddUSize(std::size_t a, std::size_t b, std::size_t& out) noexcept
    { if (a > std::numeric_limits<std::size_t>::max() - b) return false; out = a + b; return true; }
    inline bool CheckedSubUSize(std::size_t a, std::size_t b, std::size_t& out) noexcept
    { if (a < b) return false; out = a - b; return true; }
    inline bool CheckedMulUSize(std::size_t a, std::size_t b, std::size_t& out) noexcept
    { if (b != 0 && a > std::numeric_limits<std::size_t>::max() / b) return false; out = a * b; return true; }

    inline std::ptrdiff_t SaturatingAddISize(std::ptrdiff_t a, std::ptrdiff_t b) noexcept
    { std::ptrdiff_t out{}; return CheckedAddISize(a, b, out) ? out : (b > 0 ? std::numeric_limits<std::ptrdiff_t>::max() : std::numeric_limits<std::ptrdiff_t>::min()); }
    inline std::ptrdiff_t SaturatingSubISize(std::ptrdiff_t a, std::ptrdiff_t b) noexcept
    { std::ptrdiff_t out{}; return CheckedSubISize(a, b, out) ? out : (b < 0 ? std::numeric_limits<std::ptrdiff_t>::max() : std::numeric_limits<std::ptrdiff_t>::min()); }
    inline std::ptrdiff_t SaturatingMulISize(std::ptrdiff_t a, std::ptrdiff_t b) noexcept
    { std::ptrdiff_t out{}; return CheckedMulISize(a, b, out) ? out : ((a < 0) != (b < 0) ? std::numeric_limits<std::ptrdiff_t>::min() : std::numeric_limits<std::ptrdiff_t>::max()); }
    inline std::size_t SaturatingAddUSize(std::size_t a, std::size_t b) noexcept
    { std::size_t out{}; return CheckedAddUSize(a, b, out) ? out : std::numeric_limits<std::size_t>::max(); }
    inline std::size_t SaturatingSubUSize(std::size_t a, std::size_t b) noexcept
    { std::size_t out{}; return CheckedSubUSize(a, b, out) ? out : 0; }
    inline std::size_t SaturatingMulUSize(std::size_t a, std::size_t b) noexcept
    { std::size_t out{}; return CheckedMulUSize(a, b, out) ? out : std::numeric_limits<std::size_t>::max(); }

#undef WIO_SMALL_SIGNED_WIDTH
#undef WIO_SMALL_UNSIGNED_WIDTH
}

namespace wio::runtime::std_uuid
{
    inline std::string NewV4()
    {
        thread_local std::mt19937_64 engine{std::random_device{}()};
        std::array<unsigned char, 16> bytes{};
        for (auto& byte : bytes) byte = static_cast<unsigned char>(engine() & 0xFFU);
        bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0FU) | 0x40U);
        bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3FU) | 0x80U);
        static constexpr char hex[] = "0123456789abcdef";
        std::string output;
        output.reserve(36);
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            if (index == 4 || index == 6 || index == 8 || index == 10) output.push_back('-');
            output.push_back(hex[bytes[index] >> 4]);
            output.push_back(hex[bytes[index] & 15]);
        }
        return output;
    }

    inline bool IsValid(std::string_view value) noexcept
    {
        if (value.size() != 36) return false;
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index == 8 || index == 13 || index == 18 || index == 23)
            {
                if (value[index] != '-') return false;
            }
            else if (!((value[index] >= '0' && value[index] <= '9') ||
                       (value[index] >= 'a' && value[index] <= 'f') ||
                       (value[index] >= 'A' && value[index] <= 'F')))
                return false;
        }
        return true;
    }
}

namespace wio::runtime::std_semver
{
    inline bool Parse(std::string_view text, std::int32_t& major, std::int32_t& minor,
                      std::int32_t& patch, std::string& prerelease, std::string& build) noexcept
    {
        const auto plus = text.find('+');
        if (plus != std::string_view::npos)
        {
            build = std::string(text.substr(plus + 1));
            text = text.substr(0, plus);
        }
        const auto dash = text.find('-');
        if (dash != std::string_view::npos)
        {
            prerelease = std::string(text.substr(dash + 1));
            text = text.substr(0, dash);
        }
        const auto first = text.find('.');
        const auto second = first == std::string_view::npos ? first : text.find('.', first + 1);
        if (first == std::string_view::npos || second == std::string_view::npos) return false;
        auto parsePart = [](std::string_view part, std::int32_t& value)
        {
            if (part.empty()) return false;
            const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
            return error == std::errc{} && end == part.data() + part.size() && value >= 0;
        };
        return parsePart(text.substr(0, first), major) &&
               parsePart(text.substr(first + 1, second - first - 1), minor) &&
               parsePart(text.substr(second + 1), patch);
    }

    inline bool ParsePacked(std::string_view text, std::uint64_t& packed) noexcept
    {
        constexpr std::uint64_t partMask = (std::uint64_t{1} << 21U) - 1U;
        std::int32_t major{};
        std::int32_t minor{};
        std::int32_t patch{};
        std::string prerelease;
        std::string build;
        if (!Parse(text, major, minor, patch, prerelease, build) ||
            !prerelease.empty() || !build.empty() ||
            static_cast<std::uint64_t>(major) > partMask ||
            static_cast<std::uint64_t>(minor) > partMask ||
            static_cast<std::uint64_t>(patch) > partMask)
            return false;
        packed = (static_cast<std::uint64_t>(major) << 42U) |
                 (static_cast<std::uint64_t>(minor) << 21U) |
                 static_cast<std::uint64_t>(patch);
        return true;
    }
}
