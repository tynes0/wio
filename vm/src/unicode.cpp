#include "wio/vm/unicode.h"

#include <cstdint>
#include <utility>

namespace wio::vm
{
    namespace
    {
        [[nodiscard]] bool isContinuation(const std::uint8_t value) noexcept
        {
            return (value & 0xc0u) == 0x80u;
        }

        [[nodiscard]] Utf8DecodeResult failure(const std::size_t offset, std::string message)
        {
            Utf8DecodeResult result;
            result.error = Utf8Error{offset, std::move(message)};
            return result;
        }
    } // namespace

    Utf8DecodeResult decodeUtf8(const std::string_view input)
    {
        Utf8DecodeResult result;
        result.value.reserve(input.size());
        for (std::size_t offset = 0; offset < input.size();)
        {
            const auto lead = static_cast<std::uint8_t>(input[offset]);
            if (lead <= 0x7fu)
            {
                result.value.push_back(static_cast<char32_t>(lead));
                ++offset;
                continue;
            }

            std::size_t width = 0;
            char32_t scalar = 0;
            char32_t minimum = 0;
            if ((lead & 0xe0u) == 0xc0u)
            {
                width = 2;
                scalar = lead & 0x1fu;
                minimum = 0x80;
            }
            else if ((lead & 0xf0u) == 0xe0u)
            {
                width = 3;
                scalar = lead & 0x0fu;
                minimum = 0x800;
            }
            else if ((lead & 0xf8u) == 0xf0u)
            {
                width = 4;
                scalar = lead & 0x07u;
                minimum = 0x10000;
            }
            else
                return failure(offset, "Invalid UTF-8 leading byte");

            if (offset + width > input.size())
                return failure(offset, "Truncated UTF-8 sequence");
            for (std::size_t index = 1; index < width; ++index)
            {
                const auto continuation = static_cast<std::uint8_t>(input[offset + index]);
                if (!isContinuation(continuation))
                    return failure(offset + index, "Invalid UTF-8 continuation byte");
                scalar = static_cast<char32_t>((scalar << 6u) | (continuation & 0x3fu));
            }
            if (scalar < minimum)
                return failure(offset, "Overlong UTF-8 sequence");
            if (scalar >= 0xd800 && scalar <= 0xdfff)
                return failure(offset, "UTF-8 sequence encodes a surrogate");
            if (scalar > 0x10ffff)
                return failure(offset, "UTF-8 scalar is outside the Unicode range");
            result.value.push_back(scalar);
            offset += width;
        }
        return result;
    }

    std::string encodeUtf8(const std::u32string_view input)
    {
        std::string result;
        result.reserve(input.size());
        for (const char32_t scalar : input)
        {
            if (scalar <= 0x7f)
                result.push_back(static_cast<char>(scalar));
            else if (scalar <= 0x7ff)
            {
                result.push_back(static_cast<char>(0xc0u | (scalar >> 6u)));
                result.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
            }
            else if (scalar <= 0xffff && (scalar < 0xd800 || scalar > 0xdfff))
            {
                result.push_back(static_cast<char>(0xe0u | (scalar >> 12u)));
                result.push_back(static_cast<char>(0x80u | ((scalar >> 6u) & 0x3fu)));
                result.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
            }
            else if (scalar <= 0x10ffff)
            {
                result.push_back(static_cast<char>(0xf0u | (scalar >> 18u)));
                result.push_back(static_cast<char>(0x80u | ((scalar >> 12u) & 0x3fu)));
                result.push_back(static_cast<char>(0x80u | ((scalar >> 6u) & 0x3fu)));
                result.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
            }
        }
        return result;
    }
} // namespace wio::vm
