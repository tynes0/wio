#include "std_regex.h"

#include <regex>

namespace wio::runtime::std_regex
{
    bool PatternLooksSafe(const std::string_view pattern) noexcept
    {
        if (pattern.size() > 4096u) return false;
        struct GroupState
        {
            bool hasQuantifier = false;
            bool hasAlternation = false;
        };

        std::vector<GroupState> groups(1u);
        bool hasAtom = false;
        bool previousWasQuantifier = false;
        bool previousAtomWasGroup = false;
        bool previousGroupIsRisky = false;

        for (std::size_t index = 0; index < pattern.size(); ++index)
        {
            const char ch = pattern[index];
            if (ch == '\\')
            {
                if (++index >= pattern.size()) return false;
                const char escaped = pattern[index];
                if (escaped >= '1' && escaped <= '9') return false;
                hasAtom = true;
                previousWasQuantifier = false;
                previousAtomWasGroup = false;
                previousGroupIsRisky = false;
                continue;
            }
            if (ch == '[')
            {
                bool closed = false;
                for (++index; index < pattern.size(); ++index)
                {
                    if (pattern[index] == '\\')
                    {
                        if (++index >= pattern.size()) return false;
                        continue;
                    }
                    if (pattern[index] == ']') { closed = true; break; }
                }
                if (!closed) return false;
                hasAtom = true;
                previousWasQuantifier = false;
                previousAtomWasGroup = false;
                previousGroupIsRisky = false;
                continue;
            }
            if (ch == '(')
            {
                if (index + 1u < pattern.size() && pattern[index + 1u] == '?') return false;
                groups.push_back({});
                hasAtom = false;
                previousWasQuantifier = false;
                previousAtomWasGroup = false;
                previousGroupIsRisky = false;
                continue;
            }
            if (ch == ')')
            {
                if (groups.size() == 1u) return false;
                const GroupState closed = groups.back();
                groups.pop_back();
                groups.back().hasQuantifier = groups.back().hasQuantifier || closed.hasQuantifier;
                hasAtom = true;
                previousWasQuantifier = false;
                previousAtomWasGroup = true;
                previousGroupIsRisky = closed.hasQuantifier || closed.hasAlternation;
                continue;
            }
            if (ch == '|')
            {
                groups.back().hasAlternation = true;
                hasAtom = false;
                previousWasQuantifier = false;
                previousAtomWasGroup = false;
                previousGroupIsRisky = false;
                continue;
            }

            bool quantifier = ch == '*' || ch == '+' || ch == '?';
            bool repeated = ch == '*' || ch == '+';
            if (ch == '{')
            {
                const std::size_t end = pattern.find('}', index + 1u);
                if (end == std::string_view::npos) return false;
                const auto body = pattern.substr(index + 1u, end - index - 1u);
                if (body.empty()) return false;
                bool comma = false;
                bool digit = false;
                for (const char part : body)
                {
                    if (part == ',')
                    {
                        if (comma) return false;
                        comma = true;
                    }
                    else if (part >= '0' && part <= '9') digit = true;
                    else return false;
                }
                if (!digit) return false;
                quantifier = true;
                repeated = comma || (body != "0" && body != "1");
                index = end;
            }
            if (quantifier)
            {
                if (!hasAtom || previousWasQuantifier) return false;
                if (repeated && previousAtomWasGroup && previousGroupIsRisky) return false;
                groups.back().hasQuantifier = true;
                previousWasQuantifier = true;
                continue;
            }

            if (ch == '^' || ch == '$')
            {
                hasAtom = false;
                previousWasQuantifier = false;
                continue;
            }
            hasAtom = true;
            previousWasQuantifier = false;
            previousAtomWasGroup = false;
            previousGroupIsRisky = false;
        }
        return groups.size() == 1u;
    }

    namespace
    {
        struct LimitError final {};

        std::regex compile(const std::string_view pattern, const bool ignoreCase)
        {
            auto flags = std::regex_constants::ECMAScript;
            if (ignoreCase)
                flags |= std::regex_constants::icase;
            return std::regex(std::string(pattern), flags);
        }

        template <typename TAction>
        bool guarded(TAction&& action, RegexError& error, std::string& message) noexcept
        {
            error = RegexError::none;
            message.clear();
            try
            {
                action();
                return true;
            }
            catch (const LimitError&)
            {
                return false;
            }
            catch (const std::regex_error& exception)
            {
                error = RegexError::invalid_pattern;
                message = exception.what();
                return false;
            }
            catch (const std::exception& exception)
            {
                error = RegexError::runtime_error;
                message = exception.what();
                return false;
            }
            catch (...)
            {
                error = RegexError::runtime_error;
                message = "unknown regular expression error";
                return false;
            }
        }
    }

    const char* ToString(const RegexError error) noexcept
    {
        switch (error)
        {
        case RegexError::none: return "none";
        case RegexError::invalid_pattern: return "invalid_pattern";
        case RegexError::runtime_error: return "runtime_error";
        case RegexError::limit_exceeded: return "limit_exceeded";
        }
        return "runtime_error";
    }

    int ErrorValue(const RegexError error) noexcept
    {
        return static_cast<int>(error);
    }

    bool TryIsMatch(
        const std::string_view input,
        const std::string_view pattern,
        const bool ignoreCase,
        bool& matched,
        RegexError& error,
        std::string& message) noexcept
    {
        matched = false;
        return guarded([&]
        {
            const auto expression = compile(pattern, ignoreCase);
            matched = std::regex_search(input.begin(), input.end(), expression);
        }, error, message);
    }

    bool TryFind(
        const std::string_view input,
        const std::string_view pattern,
        const bool ignoreCase,
        bool& found,
        std::size_t& index,
        std::size_t& length,
        std::string& value,
        std::vector<std::string>& groups,
        RegexError& error,
        std::string& message) noexcept
    {
        found = false;
        index = 0u;
        length = 0u;
        value.clear();
        groups.clear();
        return guarded([&]
        {
            const auto expression = compile(pattern, ignoreCase);
            std::match_results<std::string_view::const_iterator> match;
            found = std::regex_search(input.begin(), input.end(), match, expression);
            if (!found)
                return;

            index = static_cast<std::size_t>(match.position());
            length = static_cast<std::size_t>(match.length());
            value = match.str();
            groups.reserve(match.size() > 0u ? match.size() - 1u : 0u);
            for (std::size_t i = 1u; i < match.size(); ++i)
                groups.push_back(match[i].matched ? match[i].str() : std::string{});
        }, error, message);
    }

    bool TryFindAll(
        const std::string_view input,
        const std::string_view pattern,
        const bool ignoreCase,
        std::vector<std::string>& matches,
        RegexError& error,
        std::string& message) noexcept
    {
        matches.clear();
        return guarded([&]
        {
            const auto expression = compile(pattern, ignoreCase);
            const std::regex_iterator begin(input.begin(), input.end(), expression);
            const std::regex_iterator<std::string_view::const_iterator> end;
            for (auto it = begin; it != end; ++it)
                matches.push_back(it->str());
        }, error, message);
    }

    bool TryFindAllDetailed(
        const std::string_view input,
        const std::string_view pattern,
        const bool ignoreCase,
        const std::size_t maxMatches,
        const std::size_t maxCaptureGroups,
        std::vector<std::string>& values,
        std::vector<std::size_t>& indices,
        std::vector<std::size_t>& lengths,
        std::vector<std::string>& captureValues,
        std::vector<std::size_t>& captureIndices,
        std::vector<std::size_t>& captureLengths,
        std::vector<std::uint8_t>& captureMatched,
        std::vector<std::size_t>& captureCounts,
        RegexError& error,
        std::string& message) noexcept
    {
        values.clear();
        indices.clear();
        lengths.clear();
        captureValues.clear();
        captureIndices.clear();
        captureLengths.clear();
        captureMatched.clear();
        captureCounts.clear();
        return guarded([&]
        {
            const auto expression = compile(pattern, ignoreCase);
            const std::regex_iterator begin(input.begin(), input.end(), expression);
            const std::regex_iterator<std::string_view::const_iterator> end;
            for (auto it = begin; it != end; ++it)
            {
                if (values.size() >= maxMatches)
                {
                    error = RegexError::limit_exceeded;
                    message = "regular-expression match limit exceeded";
                    throw LimitError{};
                }
                const std::size_t groupCount = it->size() > 0u ? it->size() - 1u : 0u;
                if (groupCount > maxCaptureGroups)
                {
                    error = RegexError::limit_exceeded;
                    message = "regular-expression capture-group limit exceeded";
                    throw LimitError{};
                }

                values.push_back(it->str());
                indices.push_back(static_cast<std::size_t>(
                    std::distance(input.begin(), (*it)[0].first)));
                lengths.push_back(static_cast<std::size_t>((*it)[0].length()));
                captureCounts.push_back(groupCount);
                for (std::size_t groupIndex = 1u; groupIndex < it->size(); ++groupIndex)
                {
                    const auto& capture = (*it)[groupIndex];
                    captureMatched.push_back(capture.matched ? 1u : 0u);
                    captureValues.push_back(capture.matched ? capture.str() : std::string{});
                    captureIndices.push_back(capture.matched
                        ? static_cast<std::size_t>(std::distance(input.begin(), capture.first))
                        : 0u);
                    captureLengths.push_back(capture.matched
                        ? static_cast<std::size_t>(capture.length())
                        : 0u);
                }
            }
        }, error, message);
    }

    bool TryReplace(
        const std::string_view input,
        const std::string_view pattern,
        const std::string_view replacement,
        const bool ignoreCase,
        std::string& output,
        RegexError& error,
        std::string& message) noexcept
    {
        output.clear();
        return guarded([&]
        {
            const auto expression = compile(pattern, ignoreCase);
            output = std::regex_replace(
                std::string(input),
                expression,
                std::string(replacement));
        }, error, message);
    }

    bool TrySplit(
        const std::string_view input,
        const std::string_view pattern,
        const bool ignoreCase,
        std::vector<std::string>& output,
        RegexError& error,
        std::string& message) noexcept
    {
        output.clear();
        return guarded([&]
        {
            const auto expression = compile(pattern, ignoreCase);
            const std::cregex_token_iterator begin(
                input.data(),
                input.data() + input.size(),
                expression,
                -1);
            const std::cregex_token_iterator end;
            for (auto it = begin; it != end; ++it)
                output.push_back(it->str());
        }, error, message);
    }

    std::string Escape(const std::string_view value)
    {
        static constexpr std::string_view metacharacters = R"(\.^$|()[]{}*+?)";
        std::string output;
        output.reserve(value.size());
        for (const char ch : value)
        {
            if (metacharacters.find(ch) != std::string_view::npos)
                output.push_back('\\');
            output.push_back(ch);
        }
        return output;
    }
}
