#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace wio::runtime::std_serialization
{
    [[nodiscard]] inline std::string JsonEscape(std::string_view value)
    {
        static constexpr char hex[] = "0123456789abcdef";
        std::string output;
        for (const unsigned char ch : value)
        {
            switch (ch)
            {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 0x20U)
                {
                    output += "\\u00";
                    output.push_back(hex[ch >> 4]);
                    output.push_back(hex[ch & 15]);
                }
                else
                {
                    output.push_back(static_cast<char>(ch));
                }
                break;
            }
        }
        return output;
    }

    [[nodiscard]] inline std::string JsonQuote(std::string_view value)
    {
        return "\"" + JsonEscape(value) + "\"";
    }

    [[nodiscard]] inline int HexValue(char value) noexcept
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    [[nodiscard]] inline std::string JsonUnquote(std::string_view value)
    {
        if (value.size() < 2 || value.front() != '"' || value.back() != '"')
            throw std::runtime_error("JSON string must begin and end with a quote.");
        std::string output;
        for (std::size_t index = 1; index + 1 < value.size(); ++index)
        {
            char ch = value[index];
            if (ch != '\\')
            {
                output.push_back(ch);
                continue;
            }
            if (++index + 1 >= value.size())
                throw std::runtime_error("JSON string contains an incomplete escape.");
            switch (value[index])
            {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u':
            {
                if (index + 4 >= value.size())
                    throw std::runtime_error("JSON string contains an incomplete unicode escape.");
                int codepoint = 0;
                for (int digit = 0; digit < 4; ++digit)
                {
                    const int decoded = HexValue(value[++index]);
                    if (decoded < 0)
                        throw std::runtime_error("JSON string contains an invalid unicode escape.");
                    codepoint = (codepoint << 4) | decoded;
                }
                if (codepoint <= 0x7F)
                    output.push_back(static_cast<char>(codepoint));
                else if (codepoint <= 0x7FF)
                {
                    output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                }
                else
                {
                    output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                }
                break;
            }
            default: throw std::runtime_error("JSON string contains an invalid escape.");
            }
        }
        return output;
    }
}
