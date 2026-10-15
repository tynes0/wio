#pragma once

#include <charconv>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace wio::runtime
{
    // Preserve stream formatting when native code installs a custom global
    // locale. The usual classic-locale decimal path needs no stream/facet work.
    template <class T>
        requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
    void AppendInteger(std::string& target, const T value)
    {
        if (std::locale() != std::locale::classic())
        {
            std::ostringstream stream;
            if constexpr (std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>)
                stream << +value;
            else
                stream << value;
            target.append(stream.str());
            return;
        }

        // digits10 excludes the possible leading decimal digit and sign.
        char buffer[std::numeric_limits<T>::digits10 + 3];
        const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (converted.ec != std::errc{})
            throw std::logic_error("Wio integer formatting buffer is insufficient");
        target.append(buffer, converted.ptr);
    }

    template <class T>
        requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
    std::string StringifyInteger(const T value)
    {
        std::string result;
        AppendInteger(result, value);
        return result;
    }
} // namespace wio::runtime
