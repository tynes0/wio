#include "wio/codegen/cpp_identifier.h"

#include <cctype>
#include <unordered_set>

namespace wio::codegen::cpp_identifier
{
    namespace
    {
        const std::unordered_set<std::string_view>& reservedIdentifiers()
        {
            static const std::unordered_set<std::string_view> identifiers = {
                "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
                "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept",
                "const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
                "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast",
                "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
                "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
                "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "reflexpr", "register",
                "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
                "static_cast", "struct", "switch", "template", "this", "thread_local", "throw", "true", "try",
                "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile",
                "wchar_t", "while", "xor", "xor_eq"
            };
            return identifiers;
        }

        bool isIdentifierCharacter(const char character)
        {
            const auto value = static_cast<unsigned char>(character);
            return std::isalnum(value) != 0 || character == '_';
        }
    }

    bool isCppReservedIdentifier(const std::string_view identifier)
    {
        return reservedIdentifiers().contains(identifier);
    }

    bool isValidCppIdentifier(const std::string_view identifier)
    {
        if (identifier.empty() || isCppReservedIdentifier(identifier))
            return false;
        const auto first = static_cast<unsigned char>(identifier.front());
        if (std::isalpha(first) == 0 && identifier.front() != '_')
            return false;
        for (const char character : identifier)
        {
            if (!isIdentifierCharacter(character))
                return false;
        }
        return true;
    }

    bool isValidCppSymbolPath(const std::string_view symbolPath, const bool allowQualified)
    {
        if (symbolPath.empty())
            return false;

        std::size_t start = 0;
        while (start <= symbolPath.size())
        {
            const std::size_t separator = symbolPath.find("::", start);
            const std::string_view segment = separator == std::string_view::npos
                ? symbolPath.substr(start)
                : symbolPath.substr(start, separator - start);
            if (!isValidCppIdentifier(segment))
                return false;
            if (separator == std::string_view::npos)
                return true;
            if (!allowQualified)
                return false;
            start = separator + 2;
        }
        return false;
    }

    std::string sanitizeCppIdentifier(const std::string_view identifier)
    {
        if (identifier.empty())
            return "_wio_empty";
        return isCppReservedIdentifier(identifier) ? "_wio_" + std::string(identifier) : std::string(identifier);
    }

    void replaceCppIdentifier(std::string& value, const std::string_view from, const std::string_view to)
    {
        if (from.empty() || from == to)
            return;

        std::size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos)
        {
            const bool startsAtBoundary = position == 0 || !isIdentifierCharacter(value[position - 1]);
            const std::size_t end = position + from.size();
            const bool endsAtBoundary = end == value.size() || !isIdentifierCharacter(value[end]);
            if (startsAtBoundary && endsAtBoundary)
            {
                value.replace(position, from.size(), to);
                position += to.size();
            }
            else
            {
                position = end;
            }
        }
    }
}
