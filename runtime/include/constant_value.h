#pragma once

#include "text.h"

#include <compare>
#include <cstddef>
#include <string>

namespace wio::runtime
{
    // Structural C++20 non-type template values used by Wio string/text const
    // generics. RuntimeValue deliberately returns the ordinary Wio value so
    // source code inside a generic declaration never observes this backend
    // representation.
    template <std::size_t N>
    struct ConstString
    {
        char bytes[N]{};

        consteval ConstString(const char (&value)[N])
        {
            for (std::size_t index = 0; index < N; ++index)
                bytes[index] = value[index];
        }

        [[nodiscard]] std::string RuntimeValue() const
        {
            return std::string(bytes, N > 0 ? N - 1 : 0);
        }

        [[nodiscard]] constexpr auto operator<=>(const ConstString&) const = default;
    };

    template <std::size_t N>
    ConstString(const char (&)[N]) -> ConstString<N>;

    template <std::size_t N>
    struct ConstText
    {
        char bytes[N]{};

        consteval ConstText(const char (&value)[N])
        {
            for (std::size_t index = 0; index < N; ++index)
                bytes[index] = value[index];
        }

        [[nodiscard]] Text RuntimeValue() const
        {
            return Text::FromUtf8(std::string(bytes, N > 0 ? N - 1 : 0));
        }

        [[nodiscard]] constexpr auto operator<=>(const ConstText&) const = default;
    };

    template <std::size_t N>
    ConstText(const char (&)[N]) -> ConstText<N>;
}
