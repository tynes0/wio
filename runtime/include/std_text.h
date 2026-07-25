#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_text
{
    [[nodiscard]] constexpr bool IsAscii(const char value) noexcept
    {
        return static_cast<unsigned char>(value) <= 0x7FU;
    }

    [[nodiscard]] constexpr bool IsControl(const char value) noexcept
    {
        const auto byte = static_cast<unsigned char>(value);
        return byte <= 0x1FU || byte == 0x7FU;
    }

    [[nodiscard]] constexpr bool IsWhitespace(const char value) noexcept
    {
        return value == ' ' || value == '\t' || value == '\n' ||
               value == '\r' || value == '\f' || value == '\v';
    }

    [[nodiscard]] constexpr bool IsDigit(const char value) noexcept
    {
        return value >= '0' && value <= '9';
    }

    [[nodiscard]] constexpr bool IsBinaryDigit(const char value) noexcept
    {
        return value == '0' || value == '1';
    }

    [[nodiscard]] constexpr bool IsOctalDigit(const char value) noexcept
    {
        return value >= '0' && value <= '7';
    }

    [[nodiscard]] constexpr bool IsHexDigit(const char value) noexcept
    {
        return IsDigit(value) ||
               (value >= 'a' && value <= 'f') ||
               (value >= 'A' && value <= 'F');
    }

    [[nodiscard]] constexpr bool IsLower(const char value) noexcept
    {
        return value >= 'a' && value <= 'z';
    }

    [[nodiscard]] constexpr bool IsUpper(const char value) noexcept
    {
        return value >= 'A' && value <= 'Z';
    }

    [[nodiscard]] constexpr bool IsAlpha(const char value) noexcept
    {
        return IsLower(value) || IsUpper(value);
    }

    [[nodiscard]] constexpr bool IsAlphaNumeric(const char value) noexcept
    {
        return IsAlpha(value) || IsDigit(value);
    }

    [[nodiscard]] constexpr char ToLower(const char value) noexcept
    {
        return IsUpper(value)
            ? static_cast<char>(value + ('a' - 'A'))
            : value;
    }

    [[nodiscard]] constexpr char ToUpper(const char value) noexcept
    {
        return IsLower(value)
            ? static_cast<char>(value - ('a' - 'A'))
            : value;
    }

    [[nodiscard]] constexpr std::int32_t DigitValue(const char value) noexcept
    {
        return IsDigit(value) ? static_cast<std::int32_t>(value - '0') : -1;
    }

    [[nodiscard]] constexpr std::int32_t HexValue(const char value) noexcept
    {
        if (IsDigit(value))
            return static_cast<std::int32_t>(value - '0');
        if (value >= 'a' && value <= 'f')
            return static_cast<std::int32_t>(value - 'a' + 10);
        if (value >= 'A' && value <= 'F')
            return static_cast<std::int32_t>(value - 'A' + 10);
        return -1;
    }

    [[nodiscard]] constexpr char FromDigit(
        const std::int32_t value,
        const bool upper) noexcept
    {
        if (value < 0 || value > 35)
            return '\0';
        if (value < 10)
            return static_cast<char>('0' + value);
        return static_cast<char>((upper ? 'A' : 'a') + value - 10);
    }

    [[nodiscard]] inline bool StringIsAscii(const std::string_view value) noexcept
    {
        for (const char ch : value)
        {
            if (!IsAscii(ch))
                return false;
        }
        return true;
    }

    [[nodiscard]] inline bool StringEqualsIgnoreCase(
        const std::string_view left,
        const std::string_view right) noexcept
    {
        if (left.size() != right.size())
            return false;
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (ToLower(left[index]) != ToLower(right[index]))
                return false;
        }
        return true;
    }

    [[nodiscard]] inline std::int32_t StringCompare(
        const std::string_view left,
        const std::string_view right) noexcept
    {
        const int result = left.compare(right);
        return result < 0 ? -1 : (result > 0 ? 1 : 0);
    }

    [[nodiscard]] inline std::int32_t StringCompareIgnoreCase(
        const std::string_view left,
        const std::string_view right) noexcept
    {
        const std::size_t common = left.size() < right.size() ? left.size() : right.size();
        for (std::size_t index = 0; index < common; ++index)
        {
            const char leftChar = ToLower(left[index]);
            const char rightChar = ToLower(right[index]);
            if (leftChar < rightChar)
                return -1;
            if (leftChar > rightChar)
                return 1;
        }
        return left.size() < right.size() ? -1 : (left.size() > right.size() ? 1 : 0);
    }

    [[nodiscard]] inline bool StringContainsIgnoreCase(
        const std::string_view value,
        const std::string_view needle) noexcept
    {
        if (needle.empty())
            return true;
        if (needle.size() > value.size())
            return false;

        for (std::size_t start = 0; start + needle.size() <= value.size(); ++start)
        {
            if (StringEqualsIgnoreCase(value.substr(start, needle.size()), needle))
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool StringStartsWithIgnoreCase(
        const std::string_view value,
        const std::string_view prefix) noexcept
    {
        return prefix.size() <= value.size() &&
               StringEqualsIgnoreCase(value.substr(0, prefix.size()), prefix);
    }

    [[nodiscard]] inline bool StringEndsWithIgnoreCase(
        const std::string_view value,
        const std::string_view suffix) noexcept
    {
        return suffix.size() <= value.size() &&
               StringEqualsIgnoreCase(value.substr(value.size() - suffix.size()), suffix);
    }

    [[nodiscard]] inline std::size_t StringCountOccurrences(
        const std::string_view value,
        const std::string_view needle) noexcept
    {
        if (needle.empty())
            return 0;

        std::size_t count = 0;
        std::size_t start = 0;
        while (start <= value.size())
        {
            const std::size_t found = value.find(needle, start);
            if (found == std::string_view::npos)
                break;
            ++count;
            start = found + needle.size();
        }
        return count;
    }

    [[nodiscard]] inline std::vector<std::string> StringSplitWhitespace(
        const std::string_view value)
    {
        std::vector<std::string> result;
        std::size_t index = 0;
        while (index < value.size())
        {
            while (index < value.size() && IsWhitespace(value[index]))
                ++index;
            const std::size_t start = index;
            while (index < value.size() && !IsWhitespace(value[index]))
                ++index;
            if (start != index)
                result.emplace_back(value.substr(start, index - start));
        }
        return result;
    }

    [[nodiscard]] inline std::string StringCollapseWhitespace(
        const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        bool pendingSpace = false;

        for (const char ch : value)
        {
            if (IsWhitespace(ch))
            {
                pendingSpace = !result.empty();
                continue;
            }
            if (pendingSpace)
            {
                result.push_back(' ');
                pendingSpace = false;
            }
            result.push_back(ch);
        }
        return result;
    }
}
