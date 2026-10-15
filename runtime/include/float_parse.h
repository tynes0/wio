#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <string>
#include <string_view>
#include <type_traits>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace wio::runtime::detail
{
    class FloatParseLocale final
    {
    public:
#if defined(_WIN32)
        using Handle = _locale_t;
        FloatParseLocale() : handle(_create_locale(LC_NUMERIC, "C"))
        {
        }
        ~FloatParseLocale()
        {
            if (handle)
                _free_locale(handle);
        }
#else
        using Handle = locale_t;
        FloatParseLocale() : handle(newlocale(LC_NUMERIC_MASK, "C", nullptr))
        {
        }
        ~FloatParseLocale()
        {
            if (handle)
                freelocale(handle);
        }
#endif
        Handle handle;
    };

    // Compatibility for system libc++ versions without floating from_chars.
    // Keep parsing locale-independent and preserve bounded-view/end semantics.
    template <class T> std::from_chars_result parseFloatCLocale(std::string_view text, T& value) noexcept
    {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
        if (text.empty() || text.front() == '+' || text.front() == ' ' || text.front() == '\t' ||
            text.front() == '\n' || text.front() == '\r' || text.front() == '\v' || text.front() == '\f')
            return {text.data(), std::errc::invalid_argument};
        const auto sign = text.front() == '-' ? 1u : 0u;
        if (text.size() > sign + 2 && text[sign] == '0' && (text[sign + 1] == 'x' || text[sign + 1] == 'X'))
        {
            value = sign ? -T{0} : T{0};
            return {text.data() + sign + 1, {}};
        }
        try
        {
            static const FloatParseLocale locale;
            if (!locale.handle)
                return {text.data(), std::errc::result_out_of_range};
            const std::string terminated{text};
            char* end = nullptr;
            const int previousError = errno;
            errno = 0;
            T parsed;
#if defined(_WIN32)
            if constexpr (std::is_same_v<T, float>)
                parsed = _strtof_l(terminated.c_str(), &end, locale.handle);
            else
                parsed = _strtod_l(terminated.c_str(), &end, locale.handle);
#else
            if constexpr (std::is_same_v<T, float>)
                parsed = strtof_l(terminated.c_str(), &end, locale.handle);
            else
                parsed = strtod_l(terminated.c_str(), &end, locale.handle);
#endif
            const int conversionError = errno;
            errno = previousError;
            if (end == terminated.c_str())
                return {text.data(), std::errc::invalid_argument};
            const auto position = text.data() + (end - terminated.c_str());
            if (conversionError == ERANGE && (parsed == T{0} || !std::isfinite(parsed)))
                return {position, std::errc::result_out_of_range};
            value = parsed;
            return {position, {}};
        }
        catch (...)
        {
            return {text.data(), std::errc::result_out_of_range};
        }
    }

    template <class T> std::from_chars_result parseFloatChars(std::string_view text, T& value) noexcept
    {
        if constexpr (requires {
                          std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
                      })
            return std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
        else
            return parseFloatCLocale(text, value);
    }
} // namespace wio::runtime::detail
