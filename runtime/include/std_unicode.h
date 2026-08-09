#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_unicode
{
    namespace detail
    {
        inline bool decodeOne(std::string_view input, std::size_t& offset, std::uint32_t& codePoint) noexcept
        {
            if (offset >= input.size()) return false;
            const auto first = static_cast<std::uint8_t>(input[offset]);
            std::size_t length = 0;
            std::uint32_t value = 0;
            std::uint32_t minimum = 0;
            if (first < 0x80u) { length = 1; value = first; minimum = 0; }
            else if ((first & 0xe0u) == 0xc0u) { length = 2; value = first & 0x1fu; minimum = 0x80u; }
            else if ((first & 0xf0u) == 0xe0u) { length = 3; value = first & 0x0fu; minimum = 0x800u; }
            else if ((first & 0xf8u) == 0xf0u) { length = 4; value = first & 0x07u; minimum = 0x10000u; }
            else return false;
            if (offset + length > input.size()) return false;
            for (std::size_t index = 1; index < length; ++index)
            {
                const auto next = static_cast<std::uint8_t>(input[offset + index]);
                if ((next & 0xc0u) != 0x80u) return false;
                value = (value << 6u) | (next & 0x3fu);
            }
            if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
                return false;
            offset += length;
            codePoint = value;
            return true;
        }

        inline bool appendOne(std::string& output, const std::uint32_t codePoint) noexcept
        {
            if (codePoint > 0x10ffffu || (codePoint >= 0xd800u && codePoint <= 0xdfffu)) return false;
            if (codePoint <= 0x7fu) output.push_back(static_cast<char>(codePoint));
            else if (codePoint <= 0x7ffu)
            {
                output.push_back(static_cast<char>(0xc0u | (codePoint >> 6u)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            else if (codePoint <= 0xffffu)
            {
                output.push_back(static_cast<char>(0xe0u | (codePoint >> 12u)));
                output.push_back(static_cast<char>(0x80u | ((codePoint >> 6u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            else
            {
                output.push_back(static_cast<char>(0xf0u | (codePoint >> 18u)));
                output.push_back(static_cast<char>(0x80u | ((codePoint >> 12u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | ((codePoint >> 6u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            return true;
        }
    }

    inline bool TryDecode(
        const std::string_view input,
        std::vector<std::uint32_t>& output,
        std::size_t& errorOffset) noexcept
    {
        output.clear();
        errorOffset = 0;
        std::size_t offset = 0;
        while (offset < input.size())
        {
            const std::size_t start = offset;
            std::uint32_t codePoint = 0;
            if (!detail::decodeOne(input, offset, codePoint))
            {
                output.clear();
                errorOffset = start;
                return false;
            }
            output.push_back(codePoint);
        }
        return true;
    }

    inline bool IsValidUtf8(const std::string_view input) noexcept
    {
        std::vector<std::uint32_t> ignored;
        std::size_t errorOffset = 0;
        return TryDecode(input, ignored, errorOffset);
    }

    inline std::size_t CodePointCount(const std::string_view input) noexcept
    {
        std::size_t offset = 0;
        std::size_t count = 0;
        while (offset < input.size())
        {
            std::uint32_t codePoint = 0;
            if (!detail::decodeOne(input, offset, codePoint)) return 0;
            ++count;
        }
        return count;
    }

    inline bool TryEncode(
        const std::vector<std::uint32_t>& input,
        std::string& output,
        std::size_t& errorIndex) noexcept
    {
        output.clear();
        errorIndex = 0;
        for (std::size_t index = 0; index < input.size(); ++index)
        {
            if (!detail::appendOne(output, input[index]))
            {
                output.clear();
                errorIndex = index;
                return false;
            }
        }
        return true;
    }

    inline bool TryAppendCodePoint(std::string& output, const std::uint32_t codePoint) noexcept
    {
        return detail::appendOne(output, codePoint);
    }

    inline void AppendByteCharacter(std::string& output, const char value)
    {
        output.push_back(value);
    }

    inline std::uint8_t ByteAt(const std::string_view input, const std::size_t index)
    {
        return static_cast<std::uint8_t>(input.at(index));
    }

    inline void AppendByte(std::string& output, const std::uint8_t value)
    {
        output.push_back(static_cast<char>(value));
    }

    inline std::string SliceCodePoints(
        const std::string_view input,
        const std::size_t start,
        const std::size_t count) noexcept
    {
        std::size_t offset = 0;
        std::size_t index = 0;
        std::size_t byteStart = input.size();
        std::size_t byteEnd = input.size();
        while (offset < input.size())
        {
            if (index == start) byteStart = offset;
            if (index == start + count) { byteEnd = offset; break; }
            std::uint32_t codePoint = 0;
            if (!detail::decodeOne(input, offset, codePoint)) return {};
            ++index;
        }
        if (index == start) byteStart = offset;
        if (index <= start + count) byteEnd = offset;
        if (byteStart > input.size() || start > index) return {};
        return std::string(input.substr(byteStart, byteEnd - byteStart));
    }

    inline bool IsWhitespace(const std::uint32_t cp) noexcept
    {
        return cp == 0x20u || (cp >= 0x09u && cp <= 0x0du) || cp == 0x85u ||
               cp == 0xa0u || cp == 0x1680u || (cp >= 0x2000u && cp <= 0x200au) ||
               cp == 0x2028u || cp == 0x2029u || cp == 0x202fu || cp == 0x205fu || cp == 0x3000u;
    }

    inline bool IsAscii(const std::uint32_t cp) noexcept { return cp <= 0x7fu; }
    inline bool IsDigit(const std::uint32_t cp) noexcept { return cp >= '0' && cp <= '9'; }
    inline bool IsLetter(const std::uint32_t cp) noexcept
    {
        return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
               (cp >= 0xc0u && cp <= 0x2afu);
    }
    inline std::uint32_t ToLower(const std::uint32_t cp) noexcept
    {
        if (cp >= 'A' && cp <= 'Z') return cp + 0x20u;
        if ((cp >= 0xc0u && cp <= 0xd6u) || (cp >= 0xd8u && cp <= 0xdeu)) return cp + 0x20u;
        return cp;
    }
    inline std::uint32_t ToUpper(const std::uint32_t cp) noexcept
    {
        if (cp >= 'a' && cp <= 'z') return cp - 0x20u;
        if ((cp >= 0xe0u && cp <= 0xf6u) || (cp >= 0xf8u && cp <= 0xfeu)) return cp - 0x20u;
        return cp;
    }
}
