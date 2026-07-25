#include "std_convert.h"

#include "exception.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>
#include <type_traits>

namespace wio::runtime::std_convert
{
    namespace
    {
        [[nodiscard]] constexpr bool isSpace(const char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\n' ||
                   value == '\r' || value == '\f' || value == '\v';
        }

        [[nodiscard]] std::string_view trim(std::string_view value) noexcept
        {
            while (!value.empty() && isSpace(value.front()))
                value.remove_prefix(1);
            while (!value.empty() && isSpace(value.back()))
                value.remove_suffix(1);
            return value;
        }

        [[nodiscard]] constexpr char lowerAscii(const char value) noexcept
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value + ('a' - 'A'))
                : value;
        }

        [[nodiscard]] bool equalsAsciiIgnoreCase(
            const std::string_view left,
            const std::string_view right) noexcept
        {
            if (left.size() != right.size())
                return false;

            for (std::size_t index = 0; index < left.size(); ++index)
            {
                if (lowerAscii(left[index]) != lowerAscii(right[index]))
                    return false;
            }
            return true;
        }

        struct NormalizedInteger final
        {
            std::string_view digits;
            int base = 10;
            bool negative = false;
            ParseError error = ParseError::none;
        };

        [[nodiscard]] NormalizedInteger normalizeInteger(
            std::string_view text,
            int base) noexcept
        {
            text = trim(text);
            if (text.empty())
                return { .error = ParseError::empty };

            bool negative = false;
            if (text.front() == '+' || text.front() == '-')
            {
                negative = text.front() == '-';
                text.remove_prefix(1);
                if (text.empty())
                    return { .error = ParseError::invalid_format };
            }

            if (base != 0 && (base < 2 || base > 36))
                return { .error = ParseError::invalid_base };

            auto consumePrefix = [&](const char prefix, const int detectedBase)
            {
                if (text.size() >= 2 && text[0] == '0' &&
                    lowerAscii(text[1]) == prefix)
                {
                    text.remove_prefix(2);
                    base = detectedBase;
                    return true;
                }
                return false;
            };

            if (base == 0)
            {
                if (!consumePrefix('x', 16) &&
                    !consumePrefix('b', 2) &&
                    !consumePrefix('o', 8))
                {
                    base = 10;
                }
            }
            else if ((base == 16 && text.size() >= 2 && text[0] == '0' &&
                      lowerAscii(text[1]) == 'x') ||
                     (base == 2 && text.size() >= 2 && text[0] == '0' &&
                      lowerAscii(text[1]) == 'b') ||
                     (base == 8 && text.size() >= 2 && text[0] == '0' &&
                      lowerAscii(text[1]) == 'o'))
            {
                text.remove_prefix(2);
            }

            if (text.empty())
                return { .error = ParseError::invalid_format };

            return {
                .digits = text,
                .base = base,
                .negative = negative,
                .error = ParseError::none
            };
        }

        template <typename T>
        [[nodiscard]] bool tryParseInteger(
            const std::string_view text,
            T& value,
            ParseError& error,
            const int base) noexcept
        {
            static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);

            const NormalizedInteger normalized = normalizeInteger(text, base);
            if (normalized.error != ParseError::none)
            {
                error = normalized.error;
                return false;
            }

            if constexpr (std::is_unsigned_v<T>)
            {
                if (normalized.negative)
                {
                    error = ParseError::out_of_range;
                    return false;
                }
            }

            using Unsigned = std::make_unsigned_t<T>;
            Unsigned magnitude = 0;
            const auto [end, conversionError] = std::from_chars(
                normalized.digits.data(),
                normalized.digits.data() + normalized.digits.size(),
                magnitude,
                normalized.base
            );

            if (conversionError == std::errc::invalid_argument)
            {
                error = ParseError::invalid_format;
                return false;
            }
            if (conversionError == std::errc::result_out_of_range)
            {
                error = ParseError::out_of_range;
                return false;
            }
            if (end != normalized.digits.data() + normalized.digits.size())
            {
                error = ParseError::trailing_characters;
                return false;
            }

            if constexpr (std::is_signed_v<T>)
            {
                const Unsigned maxPositive =
                    static_cast<Unsigned>((std::numeric_limits<T>::max)());
                const Unsigned maxNegativeMagnitude =
                    static_cast<Unsigned>(maxPositive + Unsigned { 1 });

                if (normalized.negative)
                {
                    if (magnitude > maxNegativeMagnitude)
                    {
                        error = ParseError::out_of_range;
                        return false;
                    }
                    value = magnitude == maxNegativeMagnitude
                        ? (std::numeric_limits<T>::min)()
                        : static_cast<T>(-static_cast<T>(magnitude));
                }
                else
                {
                    if (magnitude > maxPositive)
                    {
                        error = ParseError::out_of_range;
                        return false;
                    }
                    value = static_cast<T>(magnitude);
                }
            }
            else
            {
                value = magnitude;
            }

            error = ParseError::none;
            return true;
        }

        template <typename T>
        [[nodiscard]] bool tryParseFloat(
            std::string_view text,
            T& value,
            ParseError& error) noexcept
        {
            text = trim(text);
            if (text.empty())
            {
                error = ParseError::empty;
                return false;
            }

            if (text.front() == '+')
            {
                text.remove_prefix(1);
                if (text.empty())
                {
                    error = ParseError::invalid_format;
                    return false;
                }
            }

            T parsed = 0;
            const auto [end, conversionError] = std::from_chars(
                text.data(),
                text.data() + text.size(),
                parsed,
                std::chars_format::general
            );

            if (conversionError == std::errc::invalid_argument)
            {
                error = ParseError::invalid_format;
                return false;
            }
            if (conversionError == std::errc::result_out_of_range ||
                !std::isfinite(parsed))
            {
                error = ParseError::out_of_range;
                return false;
            }
            if (end != text.data() + text.size())
            {
                error = ParseError::trailing_characters;
                return false;
            }

            value = parsed;
            error = ParseError::none;
            return true;
        }

        [[noreturn]] void throwParseError(
            const std::string_view text,
            const char* target,
            const ParseError error)
        {
            throw RuntimeException(
                "Cannot parse '" + std::string(text) + "' as " + target +
                ": " + ToString(error)
            );
        }

        template <typename T, typename TParser>
        [[nodiscard]] T parseOrThrow(
            const std::string_view text,
            const char* target,
            TParser&& parser)
        {
            T value {};
            ParseError error = ParseError::none;
            if (!parser(text, value, error))
                throwParseError(text, target, error);
            return value;
        }
    }

    const char* ToString(const ParseError error) noexcept
    {
        switch (error)
        {
        case ParseError::none:
            return "none";
        case ParseError::empty:
            return "input is empty";
        case ParseError::invalid_format:
            return "invalid format";
        case ParseError::trailing_characters:
            return "trailing characters";
        case ParseError::out_of_range:
            return "value is out of range";
        case ParseError::invalid_base:
            return "base must be 0 or between 2 and 36";
        }
        return "unknown parse error";
    }

    int ParseErrorValue(const ParseError error) noexcept
    {
        return static_cast<int>(error);
    }

#define WIO_DEFINE_INTEGER_PARSE(Name, Type, Label)                              \
    bool TryParse##Name(                                                         \
        const std::string_view text,                                              \
        Type& value,                                                              \
        ParseError& error,                                                        \
        const int base) noexcept                                                  \
    {                                                                             \
        return tryParseInteger(text, value, error, base);                         \
    }                                                                             \
                                                                                  \
    Type Parse##Name##OrThrow(const std::string_view text, const int base)        \
    {                                                                             \
        return parseOrThrow<Type>(                                                \
            text, Label, [base](auto source, auto& value, auto& error)            \
            {                                                                     \
                return tryParseInteger(source, value, error, base);               \
            });                                                                   \
    }

    WIO_DEFINE_INTEGER_PARSE(I8, std::int8_t, "i8")
    WIO_DEFINE_INTEGER_PARSE(I16, std::int16_t, "i16")
    WIO_DEFINE_INTEGER_PARSE(I32, std::int32_t, "i32")
    WIO_DEFINE_INTEGER_PARSE(I64, std::int64_t, "i64")
    WIO_DEFINE_INTEGER_PARSE(U8, std::uint8_t, "u8")
    WIO_DEFINE_INTEGER_PARSE(U16, std::uint16_t, "u16")
    WIO_DEFINE_INTEGER_PARSE(U32, std::uint32_t, "u32")
    WIO_DEFINE_INTEGER_PARSE(U64, std::uint64_t, "u64")
    WIO_DEFINE_INTEGER_PARSE(ISize, std::ptrdiff_t, "isize")
    WIO_DEFINE_INTEGER_PARSE(USize, std::size_t, "usize")

#undef WIO_DEFINE_INTEGER_PARSE

    bool TryParseF32(
        const std::string_view text,
        float& value,
        ParseError& error) noexcept
    {
        return tryParseFloat(text, value, error);
    }

    bool TryParseF64(
        const std::string_view text,
        double& value,
        ParseError& error) noexcept
    {
        return tryParseFloat(text, value, error);
    }

    bool TryParseBool(
        std::string_view text,
        bool& value,
        ParseError& error) noexcept
    {
        text = trim(text);
        if (text.empty())
        {
            error = ParseError::empty;
            return false;
        }

        if (text == "1" || equalsAsciiIgnoreCase(text, "true") ||
            equalsAsciiIgnoreCase(text, "yes") ||
            equalsAsciiIgnoreCase(text, "on"))
        {
            value = true;
            error = ParseError::none;
            return true;
        }

        if (text == "0" || equalsAsciiIgnoreCase(text, "false") ||
            equalsAsciiIgnoreCase(text, "no") ||
            equalsAsciiIgnoreCase(text, "off"))
        {
            value = false;
            error = ParseError::none;
            return true;
        }

        error = ParseError::invalid_format;
        return false;
    }

    float ParseF32OrThrow(const std::string_view text)
    {
        return parseOrThrow<float>(
            text, "f32", [](auto source, auto& value, auto& error)
            {
                return TryParseF32(source, value, error);
            });
    }

    double ParseF64OrThrow(const std::string_view text)
    {
        return parseOrThrow<double>(
            text, "f64", [](auto source, auto& value, auto& error)
            {
                return TryParseF64(source, value, error);
            });
    }

    bool ParseBoolOrThrow(const std::string_view text)
    {
        return parseOrThrow<bool>(
            text, "bool", [](auto source, auto& value, auto& error)
            {
                return TryParseBool(source, value, error);
            });
    }

    std::string ToBaseString(
        std::uint64_t value,
        const int base,
        const bool upper)
    {
        if (base < 2 || base > 36)
            throw RuntimeException("Integer formatting base must be between 2 and 36.");

        char buffer[65] {};
        const auto [end, error] =
            std::to_chars(buffer, buffer + sizeof(buffer), value, base);
        if (error != std::errc {})
            throw RuntimeException("Integer formatting failed.");

        std::string result(buffer, end);
        if (upper)
        {
            std::transform(result.begin(), result.end(), result.begin(), [](const char ch)
            {
                return ch >= 'a' && ch <= 'z'
                    ? static_cast<char>(ch - ('a' - 'A'))
                    : ch;
            });
        }
        return result;
    }

    std::string ToBaseString(
        const std::int64_t value,
        const int base,
        const bool upper)
    {
        if (value >= 0)
            return ToBaseString(static_cast<std::uint64_t>(value), base, upper);

        const std::uint64_t magnitude =
            static_cast<std::uint64_t>(-(value + 1)) + 1U;
        return "-" + ToBaseString(magnitude, base, upper);
    }
}
