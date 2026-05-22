#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace wio::runtime
{
    namespace detail
    {
        template <typename TValue>
        void appendFormattedValue(std::ostringstream& stream, TValue&& value)
        {
            using RawValue = std::remove_cvref_t<TValue>;

            if constexpr (std::is_same_v<RawValue, bool>)
                stream << (value ? "true" : "false");
            else
                stream << std::forward<TValue>(value);
        }

        inline void appendFormatLiterals(std::ostringstream& stream, std::string_view format)
        {
            stream << format;
        }

        template <typename TArg, typename... TRest>
        void appendFormatLiterals(std::ostringstream& stream,
                                  std::string_view format,
                                  TArg&& arg,
                                  TRest&&... rest)
        {
            const std::size_t marker = format.find("{}");
            if (marker == std::string_view::npos)
            {
                stream << format;
                return;
            }

            stream << format.substr(0, marker);
            appendFormattedValue(stream, std::forward<TArg>(arg));
            appendFormatLiterals(stream, format.substr(marker + 2), std::forward<TRest>(rest)...);
        }
    }

    template <typename... TArgs>
    std::string Format(std::string_view format, TArgs&&... args)
    {
        std::ostringstream stream;
        detail::appendFormatLiterals(stream, format, std::forward<TArgs>(args)...);
        return stream.str();
    }
}
