#pragma once

#include "stream_format.h"

#include <charconv>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace wio::runtime
{
    namespace detail
    {
        // Do not inline stream construction/EH state into the usual decimal
        // append. The locale guard stays live on every call: native code may
        // change the global locale without going through a Wio setter.
        template <class T>
#if defined(_MSC_VER)
        __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
        __attribute__((noinline, cold))
#endif
        void
        AppendIntegerStream(std::string& target, const T value)
        {
            std::ostringstream stream;
            if constexpr (std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>)
                stream << +value;
            else
                stream << value;
            AppendStreamBuffer(target, stream);
        }
    } // namespace detail

    // Preserve stream formatting when native code installs a custom global
    // locale. The usual classic-locale decimal path needs no stream/facet work.
    template <class T>
        requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
    void AppendInteger(std::string& target, const T value)
    {
        if (std::locale() != std::locale::classic())
        {
            detail::AppendIntegerStream(target, value);
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
