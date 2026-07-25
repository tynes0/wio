#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace wio::runtime::std_encoding
{
    [[nodiscard]] inline std::string HexEncode(std::string_view input, bool upper)
    {
        const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        std::string output;
        output.reserve(input.size() * 2);
        for (const unsigned char value : input)
        {
            output.push_back(digits[value >> 4]);
            output.push_back(digits[value & 0x0F]);
        }
        return output;
    }

    [[nodiscard]] inline int HexValue(char value) noexcept
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    [[nodiscard]] inline std::string HexDecode(std::string_view input)
    {
        if ((input.size() & 1U) != 0U)
            throw std::runtime_error("Hex input must contain an even number of characters.");
        std::string output;
        output.reserve(input.size() / 2);
        for (std::size_t index = 0; index < input.size(); index += 2)
        {
            const int high = HexValue(input[index]);
            const int low = HexValue(input[index + 1]);
            if (high < 0 || low < 0)
                throw std::runtime_error("Hex input contains an invalid character.");
            output.push_back(static_cast<char>((high << 4) | low));
        }
        return output;
    }

    [[nodiscard]] inline std::string Base64Encode(std::string_view input)
    {
        static constexpr std::string_view alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        output.reserve(((input.size() + 2) / 3) * 4);
        for (std::size_t index = 0; index < input.size(); index += 3)
        {
            const std::uint32_t a = static_cast<unsigned char>(input[index]);
            const std::uint32_t b = index + 1 < input.size() ? static_cast<unsigned char>(input[index + 1]) : 0;
            const std::uint32_t c = index + 2 < input.size() ? static_cast<unsigned char>(input[index + 2]) : 0;
            const std::uint32_t block = (a << 16) | (b << 8) | c;
            output.push_back(alphabet[(block >> 18) & 63]);
            output.push_back(alphabet[(block >> 12) & 63]);
            output.push_back(index + 1 < input.size() ? alphabet[(block >> 6) & 63] : '=');
            output.push_back(index + 2 < input.size() ? alphabet[block & 63] : '=');
        }
        return output;
    }

    [[nodiscard]] inline std::string Base64Decode(std::string_view input)
    {
        if ((input.size() & 3U) != 0U)
            throw std::runtime_error("Base64 input length must be divisible by four.");
        auto decode = [](char value) -> int
        {
            if (value >= 'A' && value <= 'Z') return value - 'A';
            if (value >= 'a' && value <= 'z') return value - 'a' + 26;
            if (value >= '0' && value <= '9') return value - '0' + 52;
            if (value == '+') return 62;
            if (value == '/') return 63;
            return -1;
        };
        std::string output;
        output.reserve((input.size() / 4) * 3);
        for (std::size_t index = 0; index < input.size(); index += 4)
        {
            const int a = decode(input[index]);
            const int b = decode(input[index + 1]);
            const int c = input[index + 2] == '=' ? 0 : decode(input[index + 2]);
            const int d = input[index + 3] == '=' ? 0 : decode(input[index + 3]);
            if (a < 0 || b < 0 || c < 0 || d < 0)
                throw std::runtime_error("Base64 input contains an invalid character.");
            const std::uint32_t block =
                (static_cast<std::uint32_t>(a) << 18) |
                (static_cast<std::uint32_t>(b) << 12) |
                (static_cast<std::uint32_t>(c) << 6) |
                static_cast<std::uint32_t>(d);
            output.push_back(static_cast<char>((block >> 16) & 0xFF));
            if (input[index + 2] != '=') output.push_back(static_cast<char>((block >> 8) & 0xFF));
            if (input[index + 3] != '=') output.push_back(static_cast<char>(block & 0xFF));
        }
        return output;
    }

    [[nodiscard]] inline std::string UrlEncode(std::string_view input)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string output;
        for (const unsigned char value : input)
        {
            if (std::isalnum(value) || value == '-' || value == '_' || value == '.' || value == '~')
                output.push_back(static_cast<char>(value));
            else
            {
                output.push_back('%');
                output.push_back(digits[value >> 4]);
                output.push_back(digits[value & 15]);
            }
        }
        return output;
    }

    [[nodiscard]] inline std::string UrlDecode(std::string_view input)
    {
        std::string output;
        for (std::size_t index = 0; index < input.size(); ++index)
        {
            if (input[index] != '%')
            {
                output.push_back(input[index] == '+' ? ' ' : input[index]);
                continue;
            }
            if (index + 2 >= input.size())
                throw std::runtime_error("URL encoding ends with an incomplete escape.");
            const int high = HexValue(input[index + 1]);
            const int low = HexValue(input[index + 2]);
            if (high < 0 || low < 0)
                throw std::runtime_error("URL encoding contains an invalid escape.");
            output.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        }
        return output;
    }
}
