#pragma once

#include "stream_format.h"

#include <cfenv>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>

namespace wio::runtime
{
    namespace detail
    {
        // Keep stream construction/EH state out of the usual finite/classic
        // path. libc++ currently uses this path for all values, so do not label
        // it cold there. Reading the buffer leaves stream state/lifetime intact.
        template <class T>
            requires(std::is_same_v<T, float> || std::is_same_v<T, double>)
#if defined(_MSC_VER)
        __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#if defined(_LIBCPP_VERSION)
        __attribute__((noinline))
#else
        __attribute__((noinline, cold))
#endif
#endif
        void
        AppendFloatingStream(std::string& target, const T value)
        {
            std::ostringstream stream;
            stream << value;
            AppendStreamBuffer(target, stream);
        }
    } // namespace detail

    // Wio's existing stream contract is defaultfloat/precision 6. Never replace
    // it with shortest-roundtrip formatting or silently discard native locale
    // facets / a non-default floating rounding mode.
    template <class T>
        requires(std::is_same_v<T, float> || std::is_same_v<T, double>)
    void AppendFloating(std::string& target, const T value)
    {
        // libc++'s stream and to_chars implementations do not always produce
        // identical defaultfloat text (for example -0x1.a349dp+20). Preserve
        // the existing host-stream contract until that fast path is qualified
        // independently; selecting a different spelling is not an optimization.
#if !defined(_LIBCPP_VERSION)
        if (std::isfinite(value) && std::fegetround() == FE_TONEAREST && std::locale() == std::locale::classic())
        {
            // General precision 6 needs only a small spelling, even at the
            // supported float/double exponent extremes. Preserve the fallback
            // on conversion failure rather than weakening the stream contract.
            char buffer[32];
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
#endif

        // Retain platform stream spellings for signed NaNs/infinities too.
        detail::AppendFloatingStream(target, value);
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
