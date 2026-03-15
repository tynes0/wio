#pragma once

#include "general/traits/integer_traits.h"
#include "general/traits/float_traits.h"
#include "../exception.h"

namespace wio::common::filesystem
{
    template <typename T>
    bool writeFile(const T& value, FILE* file)
    {
        std::string output;
        if constexpr (std::is_same_v<T, std::string>)
        {
            output = value;
        }
        if constexpr (std::is_integral_v<T>)
        {
            output = traits::IntegerTraits<T>::toString();
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            output = traits::FloatTraits<T>::toString();
        }

        return writeFileBase(file, output);
    }

    template <typename T>
    bool writeStdout(const T& value)
    {
        return writeFile<T>(value, stdout);
    }

    template <typename T>
    bool writeFilepath(const T& value, const std::filesystem::path& filepath)
    {
        if (FILE* file = openFile(filepath, "wb"); file)
        {
            bool result = writeFile<T>(value, file);
            (void)fclose(file);
            return result;
        }
        throw FileError(("File '" + filepath.string() + "' cannot be opened!").c_str());
    }
}
