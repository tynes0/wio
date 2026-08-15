#pragma once

#include <algorithm>
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

    inline bool TryDecodeUtf16(
        const std::vector<std::uint16_t>& input,
        std::string& output,
        std::size_t& errorIndex) noexcept
    {
        output.clear();
        errorIndex = 0;
        for (std::size_t index = 0; index < input.size(); ++index)
        {
            const std::size_t scalarStart = index;
            std::uint32_t codePoint = input[index];
            if (codePoint >= 0xd800u && codePoint <= 0xdbffu)
            {
                if (index + 1 >= input.size())
                {
                    errorIndex = scalarStart;
                    output.clear();
                    return false;
                }
                const std::uint32_t low = input[++index];
                if (low < 0xdc00u || low > 0xdfffu)
                {
                    errorIndex = scalarStart;
                    output.clear();
                    return false;
                }
                codePoint = 0x10000u + ((codePoint - 0xd800u) << 10u) + (low - 0xdc00u);
            }
            else if (codePoint >= 0xdc00u && codePoint <= 0xdfffu)
            {
                errorIndex = scalarStart;
                output.clear();
                return false;
            }

            if (!detail::appendOne(output, codePoint))
            {
                errorIndex = scalarStart;
                output.clear();
                return false;
            }
        }
        return true;
    }

    inline bool TryEncodeUtf16(
        const std::string_view input,
        std::vector<std::uint16_t>& output,
        std::size_t& errorOffset) noexcept
    {
        output.clear();
        errorOffset = 0;
        std::size_t offset = 0;
        while (offset < input.size())
        {
            const std::size_t scalarStart = offset;
            std::uint32_t codePoint = 0;
            if (!detail::decodeOne(input, offset, codePoint))
            {
                errorOffset = scalarStart;
                output.clear();
                return false;
            }

            if (codePoint <= 0xffffu)
            {
                output.push_back(static_cast<std::uint16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000u;
                output.push_back(static_cast<std::uint16_t>(0xd800u + (codePoint >> 10u)));
                output.push_back(static_cast<std::uint16_t>(0xdc00u + (codePoint & 0x3ffu)));
            }
        }
        return true;
    }

    inline std::vector<std::uint16_t> EncodeUtf16(const std::string_view input)
    {
        std::vector<std::uint16_t> output;
        std::size_t errorOffset = 0;
        if (!TryEncodeUtf16(input, output, errorOffset))
            output.clear();
        return output;
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

    inline bool IsCombiningMark(const std::uint32_t cp) noexcept
    {
        return (cp >= 0x0300u && cp <= 0x036fu) || (cp >= 0x1ab0u && cp <= 0x1affu) ||
               (cp >= 0x1dc0u && cp <= 0x1dffu) || (cp >= 0x20d0u && cp <= 0x20ffu) ||
               (cp >= 0xfe20u && cp <= 0xfe2fu);
    }

    inline bool IsVariationSelector(const std::uint32_t cp) noexcept
    {
        return (cp >= 0xfe00u && cp <= 0xfe0fu) || (cp >= 0xe0100u && cp <= 0xe01efu);
    }

    inline bool IsEmojiModifier(const std::uint32_t cp) noexcept
    {
        return cp >= 0x1f3fbu && cp <= 0x1f3ffu;
    }

    inline bool IsRegionalIndicator(const std::uint32_t cp) noexcept
    {
        return cp >= 0x1f1e6u && cp <= 0x1f1ffu;
    }

    inline std::vector<std::size_t> GraphemeBoundaries(const std::string_view input) noexcept
    {
        std::vector<std::size_t> boundaries{0};
        std::size_t offset = 0;
        std::uint32_t previous = 0;
        std::size_t regionalRun = 0;
        bool first = true;
        while (offset < input.size())
        {
            const std::size_t start = offset;
            std::uint32_t current = 0;
            if (!detail::decodeOne(input, offset, current)) return {};
            bool breaks = !first;
            if (IsCombiningMark(current) || IsVariationSelector(current) || IsEmojiModifier(current) ||
                current == 0x200du || previous == 0x200du)
                breaks = false;
            if (IsRegionalIndicator(current))
            {
                if (IsRegionalIndicator(previous) && (regionalRun % 2u) == 1u) breaks = false;
                ++regionalRun;
            }
            else regionalRun = 0;
            if (breaks) boundaries.push_back(start);
            previous = current;
            first = false;
        }
        boundaries.push_back(input.size());
        return boundaries;
    }

    inline std::size_t GraphemeCount(const std::string_view input) noexcept
    {
        const auto boundaries = GraphemeBoundaries(input);
        return boundaries.empty() ? 0 : boundaries.size() - 1;
    }

    inline std::string SliceGraphemes(const std::string_view input,
                                      const std::size_t start, const std::size_t count) noexcept
    {
        const auto boundaries = GraphemeBoundaries(input);
        if (boundaries.empty() || start >= boundaries.size() - 1) return {};
        const std::size_t end = std::min(start + count, boundaries.size() - 1);
        return std::string(input.substr(boundaries[start], boundaries[end] - boundaries[start]));
    }

    inline std::size_t DisplayWidth(const std::string_view input) noexcept
    {
        std::size_t offset = 0;
        std::size_t width = 0;
        while (offset < input.size())
        {
            std::uint32_t cp = 0;
            if (!detail::decodeOne(input, offset, cp)) return 0;
            if (IsCombiningMark(cp) || IsVariationSelector(cp) || cp == 0x200du || cp < 0x20u ||
                (cp >= 0x7fu && cp < 0xa0u)) continue;
            const bool wide = (cp >= 0x1100u && cp <= 0x115fu) || cp == 0x2329u || cp == 0x232au ||
                (cp >= 0x2e80u && cp <= 0xa4cfu) || (cp >= 0xac00u && cp <= 0xd7a3u) ||
                (cp >= 0xf900u && cp <= 0xfaffu) || (cp >= 0xfe10u && cp <= 0xfe6fu) ||
                (cp >= 0xff00u && cp <= 0xff60u) || (cp >= 0x1f300u && cp <= 0x1faffu) ||
                (cp >= 0x20000u && cp <= 0x3ffffu);
            width += wide ? 2u : 1u;
        }
        return width;
    }

    inline std::string CaseFold(const std::string_view input) noexcept
    {
        std::string output;
        std::size_t offset = 0;
        while (offset < input.size())
        {
            std::uint32_t cp = 0;
            if (!detail::decodeOne(input, offset, cp)) return {};
            if (cp == 0x00dfu || cp == 0x1e9eu) { output += "ss"; continue; }
            if (cp == 0x03c2u) cp = 0x03c3u;
            if (!detail::appendOne(output, ToLower(cp))) return {};
        }
        return output;
    }
}
