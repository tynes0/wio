#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace wio::runtime::std_convert
{
    enum class ParseError : std::uint8_t
    {
        none = 0,
        empty = 1,
        invalid_format = 2,
        trailing_characters = 3,
        out_of_range = 4,
        invalid_base = 5
    };

    [[nodiscard]] const char* ToString(ParseError error) noexcept;
    [[nodiscard]] int ParseErrorValue(ParseError error) noexcept;

#define WIO_DECLARE_INTEGER_PARSE(Name, Type)                                      \
    [[nodiscard]] bool TryParse##Name(                                              \
        std::string_view text, Type& value, ParseError& error, int base = 10) noexcept; \
    [[nodiscard]] Type Parse##Name##OrThrow(std::string_view text, int base = 10);

    WIO_DECLARE_INTEGER_PARSE(I8, std::int8_t)
    WIO_DECLARE_INTEGER_PARSE(I16, std::int16_t)
    WIO_DECLARE_INTEGER_PARSE(I32, std::int32_t)
    WIO_DECLARE_INTEGER_PARSE(I64, std::int64_t)
    WIO_DECLARE_INTEGER_PARSE(U8, std::uint8_t)
    WIO_DECLARE_INTEGER_PARSE(U16, std::uint16_t)
    WIO_DECLARE_INTEGER_PARSE(U32, std::uint32_t)
    WIO_DECLARE_INTEGER_PARSE(U64, std::uint64_t)
    WIO_DECLARE_INTEGER_PARSE(ISize, std::ptrdiff_t)
    WIO_DECLARE_INTEGER_PARSE(USize, std::size_t)

#undef WIO_DECLARE_INTEGER_PARSE

    [[nodiscard]] bool TryParseF32(
        std::string_view text, float& value, ParseError& error) noexcept;
    [[nodiscard]] bool TryParseF64(
        std::string_view text, double& value, ParseError& error) noexcept;
    [[nodiscard]] bool TryParseBool(
        std::string_view text, bool& value, ParseError& error) noexcept;

    [[nodiscard]] float ParseF32OrThrow(std::string_view text);
    [[nodiscard]] double ParseF64OrThrow(std::string_view text);
    [[nodiscard]] bool ParseBoolOrThrow(std::string_view text);

    [[nodiscard]] std::string ToBaseString(
        std::uint64_t value, int base, bool upper = false);
    [[nodiscard]] std::string ToBaseString(
        std::int64_t value, int base, bool upper = false);
}
