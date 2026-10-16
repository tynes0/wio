#pragma once

#include "floating_format.h"
#include "integer_format.h"
#include "text.h"

#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace wio::runtime
{
    namespace detail
    {
        template <class T>
        struct BasicFormat
            : std::bool_constant<std::is_integral_v<T> || std::is_enum_v<T> || std::is_same_v<T, float> ||
                                 std::is_same_v<T, double> || std::is_same_v<T, std::string> || std::is_same_v<T, Text>>
        {
        };

        template <class T, class A> struct BasicFormat<std::vector<T, A>> : BasicFormat<T>
        {
        };

        template <class K, class V, class C, class A>
        struct BasicFormat<std::map<K, V, C, A>> : std::bool_constant<BasicFormat<K>::value && BasicFormat<V>::value>
        {
        };

        template <class K, class V, class H, class E, class A>
        struct BasicFormat<std::unordered_map<K, V, H, E, A>>
            : std::bool_constant<BasicFormat<K>::value && BasicFormat<V>::value>
        {
        };
    } // namespace detail

    template <class T> inline constexpr bool IsBasicFormattable = detail::BasicFormat<std::remove_cvref_t<T>>::value;

    template <class T>
    inline constexpr bool IsBasicFormatContainer =
        IsBasicFormattable<T> && !std::is_arithmetic_v<std::remove_cvref_t<T>> &&
        !std::is_enum_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, std::string> &&
        !std::is_same_v<std::remove_cvref_t<T>, Text>;

    // Internal generated-code helper, not a new Wio API. Containers with user
    // hooks/native stream operators retain the old generic formatting path.
    template <class T>
        requires IsBasicFormattable<T>
    void AppendBasicFormatted(std::string& target, const T& value)
    {
        using Raw = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<Raw, std::string>)
            target.append(value);
        else if constexpr (std::is_same_v<Raw, Text>)
            target.append(value.Utf8());
        else if constexpr (std::is_same_v<Raw, bool>)
            target.append(value ? "true" : "false");
        else if constexpr (std::is_same_v<Raw, char>)
            target.push_back(value);
        else if constexpr (std::is_enum_v<Raw>)
            AppendBasicFormatted(target, static_cast<std::underlying_type_t<Raw>>(value));
        else if constexpr (std::is_same_v<Raw, signed char> || std::is_same_v<Raw, unsigned char>)
            target.append(std::to_string(+value));
        else if constexpr (std::is_integral_v<Raw>)
            AppendInteger(target, value);
        else if constexpr (std::is_floating_point_v<Raw>)
            AppendFloating(target, value);
        else if constexpr (requires { typename Raw::mapped_type; })
        {
            target.push_back('{');
            bool first = true;
            for (const auto& [key, item] : value)
            {
                if (!first)
                    target.append(", ");
                first = false;
                AppendBasicFormatted(target, key);
                target.append(": ");
                AppendBasicFormatted(target, item);
            }

            target.push_back('}');
        }
        else
        {
            target.push_back('[');
            bool first = true;
            for (const auto& item : value)
            {
                if (!first)
                    target.append(", ");
                first = false;
                AppendBasicFormatted(target, item);
            }

            target.push_back(']');
        }
    }
} // namespace wio::runtime
