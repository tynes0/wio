#pragma once

#include <cstdio>
#include <string>
#include <system_error>
#include <typeinfo>

#include "exception.h"
#include "../general/traits/integer_traits.h"
#include "../general/traits/float_traits.h"

// Prefixes
// h -> helper
// b -> built-in

namespace wio::runtime::io
{
    template <typename T>
struct IsStringLike : std::false_type {};

    template <>
    struct IsStringLike<std::string> : std::true_type {};

    template <>
    struct IsStringLike<const char*> : std::true_type {};

    template <>
    struct IsStringLike<char*> : std::true_type {};

    template <size_t N>
    struct IsStringLike<const char[N]> : std::true_type {};

    template <size_t N>
    struct IsStringLike<char[N]> : std::true_type {};

    template <typename T>
    inline constexpr bool IsStringLikeV = IsStringLike<std::decay_t<T>>::value;
    
    inline bool hWriteFileBase(FILE* file, const std::string& output)
    {
        if (!file)
            throw FileError("Writing failed: Null file.");
        
        const char* data = output.data();
        size_t total = output.size();
        size_t written = 0;

        while (written < total)
        {
            size_t n = std::fwrite(data + written, 1, total - written, file);
            
            if (n == 0)
            {
                if (std::ferror(file))
                {
                    std::error_code ec(errno, std::generic_category());
                    throw FileError(("Writing failed: " + ec.message()).c_str()); // Todo: should we throw?
                }
                break;
            }
            written += n;
        }
        return written == total;
    }
    
    template <typename T>
    bool bWrite(FILE* file, const T& value)
    {
        std::string output;
        if constexpr (IsStringLikeV<T>)
        {
            output = value;
        }
        if constexpr (std::is_integral_v<T>)
        {
            output = traits::IntegerTraits<T>::toString(value);
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            output = traits::FloatTraits<T>::toString(value);
        }

        return hWriteFileBase(file, output);
    }
    
    template <typename T>
    bool bPrint(const T& value)
    {
        return bWrite(stdout, value);
    }
}