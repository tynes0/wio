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
        runtime_error = 2,
        limit_exceeded = 3
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

    [[nodiscard]] bool TryFindAllDetailed(
        std::string_view input,
        std::string_view pattern,
        bool ignoreCase,
        std::size_t maxMatches,
        std::size_t maxCaptureGroups,
        std::vector<std::string>& values,
        std::vector<std::size_t>& indices,
        std::vector<std::size_t>& lengths,
        std::vector<std::string>& captureValues,
        std::vector<std::size_t>& captureIndices,
        std::vector<std::size_t>& captureLengths,
        std::vector<std::uint8_t>& captureMatched,
        std::vector<std::size_t>& captureCounts,
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
