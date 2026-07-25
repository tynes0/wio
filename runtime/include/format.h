#pragma once

#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace wio::runtime
{
    namespace detail
    {
        template <typename>
        inline constexpr bool AlwaysFalse = false;

        template <typename TValue>
        void appendFormattedValue(std::ostringstream& stream, TValue&& value)
        {
            using RawValue = std::remove_cvref_t<TValue>;

            if constexpr (std::is_same_v<RawValue, bool>)
                stream << (value ? "true" : "false");
            else if constexpr (std::is_same_v<RawValue, signed char>)
                stream << static_cast<int>(value);
            else if constexpr (std::is_same_v<RawValue, unsigned char>)
                stream << static_cast<unsigned int>(value);
            else if constexpr (
                std::is_same_v<RawValue, std::string> ||
                std::is_same_v<RawValue, std::string_view>)
                stream << value;
            else if constexpr (
                std::is_same_v<RawValue, const char*> ||
                std::is_same_v<RawValue, char*>)
                stream << (value != nullptr ? value : "null");
            else if constexpr (requires { value.Get(); value->_WF_ToString(); })
            {
                if (value)
                    appendFormattedValue(stream, value->_WF_ToString());
                else
                    stream << "null";
            }
            else if constexpr (requires { value._WF_ToString(); })
                appendFormattedValue(stream, value._WF_ToString());
            else if constexpr (std::is_enum_v<RawValue>)
                appendFormattedValue(
                    stream,
                    static_cast<std::underlying_type_t<RawValue>>(value)
                );
            else if constexpr (requires
            {
                std::begin(value);
                std::end(value);
                (*std::begin(value)).first;
                (*std::begin(value)).second;
            })
            {
                stream << '{';
                bool first = true;
                for (const auto& entry : value)
                {
                    if (!first)
                        stream << ", ";
                    first = false;
                    appendFormattedValue(stream, entry.first);
                    stream << ": ";
                    appendFormattedValue(stream, entry.second);
                }
                stream << '}';
            }
            else if constexpr (requires
            {
                std::begin(value);
                std::end(value);
            })
            {
                stream << '[';
                bool first = true;
                for (const auto& item : value)
                {
                    if (!first)
                        stream << ", ";
                    first = false;
                    appendFormattedValue(stream, item);
                }
                stream << ']';
            }
            else if constexpr (requires { stream << std::forward<TValue>(value); })
                stream << std::forward<TValue>(value);
            else
                static_assert(
                    AlwaysFalse<RawValue>,
                    "Wio formatting does not support this type. Add a public ToString() -> string method."
                );
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
