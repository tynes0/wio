#pragma once

#include <cfenv>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>

namespace wio::runtime
{
    // Wio's existing stream contract is defaultfloat/precision 6. Never replace
    // it with shortest-roundtrip formatting or silently discard native locale
    // facets / a non-default floating rounding mode.
    template <class T>
        requires(std::is_same_v<T, float> || std::is_same_v<T, double>)
    void AppendFloating(std::string& target, const T value)
    {
        if (std::isfinite(value) && std::fegetround() == FE_TONEAREST && std::locale() == std::locale::classic())
        {
            char buffer[128];
            if constexpr (requires {
                              std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 6);
                          })
            {
                const auto converted =
                    std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 6);
                if (converted.ec == std::errc{})
                {
                    target.append(buffer, converted.ptr);
                    return;
                }
            }
        }

        // Retain platform stream spellings for signed NaNs/infinities too.
        std::ostringstream stream;
        stream << value;
        target.append(stream.str());
    }

    template <class T>
        requires(std::is_same_v<T, float> || std::is_same_v<T, double>)
    [[nodiscard]] std::string StringifyFloating(const T value)
    {
        std::string result;
        AppendFloating(result, value);
        return result;
    }
} // namespace wio::runtime
