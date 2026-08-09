#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_regex
{
    enum class RegexError : std::uint8_t
    {
        none = 0,
        invalid_pattern = 1,
        runtime_error = 2
    };

    [[nodiscard]] const char* ToString(RegexError error) noexcept;
    [[nodiscard]] int ErrorValue(RegexError error) noexcept;

    [[nodiscard]] bool TryIsMatch(
        std::string_view input,
        std::string_view pattern,
        bool ignoreCase,
        bool& matched,
        RegexError& error,
        std::string& message) noexcept;

    [[nodiscard]] bool TryFind(
        std::string_view input,
        std::string_view pattern,
        bool ignoreCase,
        bool& found,
        std::size_t& index,
        std::size_t& length,
        std::string& value,
        std::vector<std::string>& groups,
        RegexError& error,
        std::string& message) noexcept;

    [[nodiscard]] bool TryFindAll(
        std::string_view input,
        std::string_view pattern,
        bool ignoreCase,
        std::vector<std::string>& matches,
        RegexError& error,
        std::string& message) noexcept;

    [[nodiscard]] bool TryReplace(
        std::string_view input,
        std::string_view pattern,
        std::string_view replacement,
        bool ignoreCase,
        std::string& output,
        RegexError& error,
        std::string& message) noexcept;

    [[nodiscard]] bool TrySplit(
        std::string_view input,
        std::string_view pattern,
        bool ignoreCase,
        std::vector<std::string>& output,
        RegexError& error,
        std::string& message) noexcept;

    [[nodiscard]] std::string Escape(std::string_view value);
    [[nodiscard]] bool PatternLooksSafe(std::string_view pattern) noexcept;
}
