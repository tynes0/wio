#include "std_regex.h"

#include <regex>

namespace wio::runtime::std_regex
{
    bool PatternLooksSafe(const std::string_view pattern) noexcept
    {
        if (pattern.size() > 4096u) return false;
        bool escaped = false;
        bool previousQuantifier = false;
        int groupDepth = 0;
        std::vector<bool> groupHasQuantifier;
        for (const char ch : pattern)
        {
            if (escaped) { escaped = false; previousQuantifier = false; continue; }
            if (ch == '\\') { escaped = true; continue; }
            if (ch == '(') { ++groupDepth; groupHasQuantifier.push_back(false); previousQuantifier = false; continue; }
            if (ch == ')') { if (groupDepth > 0) --groupDepth; previousQuantifier = false; continue; }
            const bool quantifier = ch == '*' || ch == '+' || ch == '?';
            if (quantifier)
            {
                if (previousQuantifier) return false;
                if (!groupHasQuantifier.empty()) groupHasQuantifier.back() = true;
            }
            if (quantifier && !groupHasQuantifier.empty() && groupHasQuantifier.back() && ch == '+')
            {
                // Conservative rejection of common nested/repeated forms.
                const auto close = pattern.find(')');
                if (close != std::string_view::npos && close + 1 < pattern.size() &&
                    (pattern[close + 1] == '+' || pattern[close + 1] == '*')) return false;
            }
            previousQuantifier = quantifier;
        }
        return !escaped && groupDepth == 0;
    }

    namespace
    {
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
