#pragma once

#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <string>
#include <charconv>

#include "helpers/io_helpers.h"

namespace wio::runtime::std_console
{
    template <typename T>
    std::int32_t WriteValue(const T& value)
    {
        FILE* file = stdout;

        using U = std::decay_t<T>;

        helpers::io_helpers::WriteResult result{};

        if constexpr (std::is_same_v<U, bool>)
        {
            result = helpers::io_helpers::Write(file, value ? "true" : "false");
        }
        else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>)
        {
            result = helpers::io_helpers::Write(file, value != nullptr ? value : "(null)");
        }
        else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>)
        {
            result = helpers::io_helpers::Write(file, value);
        }
        else if constexpr (std::is_integral_v<U>)
        {
            using Casted =
                std::conditional_t<std::is_same_v<U, std::int8_t>, std::int32_t,
                std::conditional_t<std::is_same_v<U, std::uint8_t>, std::uint32_t, U>>;

            char buffer[64];
            const auto v = static_cast<Casted>(value);

            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), v);

            if (ec != std::errc{})
            {
                return -1;
            }

            result = helpers::io_helpers::Write(file, buffer, static_cast<std::size_t>(ptr - buffer));
        }
        else if constexpr (std::is_floating_point_v<U>)
        {
            char buffer[64];

            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);

            if (ec != std::errc{})
            {
                return -1;
            }

            result = helpers::io_helpers::Write(file, buffer, static_cast<std::size_t>(ptr - buffer));
        }
        else
        {
            static_assert(sizeof(T) == 0, "WriteValue: Unsupported type");
        }

        return result.Ok() ? 0 : -1;
    }
    
    inline std::string Input(const std::string& prompt = "")
    {
        WriteValue(prompt);
        std::string result;
        (void)helpers::io_helpers::InputLine(result);
        return result;
    }

    inline std::string InputN(const std::size_t count)
    {
        std::string result;
        (void)helpers::io_helpers::InputCount(result, count);
        return result;
    }

    inline char InputChar(bool isHidden = false)
    {
        char result = '\0';
        if (!isHidden)
            (void)helpers::io_helpers::InputChar(result);
        else
            (void)helpers::io_helpers::InputCharHidden(result);
        return result;
    }

    inline std::string InputWord()
    {
        std::string result;
        (void)helpers::io_helpers::InputWord(result);
        return result;
    }

    inline std::string InputUntil(const char delimiter, const bool includeDelimiter = false)
    {
        std::string result;
        (void)helpers::io_helpers::InputUntil(result, delimiter, includeDelimiter);
        return result;
    }
}