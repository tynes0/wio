#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "exception.h"

namespace wio::runtime::std_fs
{
    namespace detail
    {
        inline std::filesystem::path ToPath(const std::string& value)
        {
            return std::filesystem::path(value);
        }

        inline std::string ToGenericString(const std::filesystem::path& value)
        {
            return value.generic_string();
        }

        [[noreturn]] inline void ThrowFileError(const std::string& message)
        {
            throw FileError(message.c_str());
        }
    }

    inline bool Exists(const std::string& path)
    {
        std::error_code ec;
        const bool exists = std::filesystem::exists(detail::ToPath(path), ec);
        return !ec && exists;
    }

    inline bool IsFile(const std::string& path)
    {
        std::error_code ec;
        const bool result = std::filesystem::is_regular_file(detail::ToPath(path), ec);
        return !ec && result;
    }

    inline bool IsDirectory(const std::string& path)
    {
        std::error_code ec;
        const bool result = std::filesystem::is_directory(detail::ToPath(path), ec);
        return !ec && result;
    }

    inline bool IsAbsolute(const std::string& path)
    {
        return detail::ToPath(path).is_absolute();
    }

    inline bool CreateDirectories(const std::string& path)
    {
        if (path.empty())
            return false;

        std::error_code ec;
        const std::filesystem::path fsPath = detail::ToPath(path);

        if (std::filesystem::exists(fsPath, ec))
            return !ec && std::filesystem::is_directory(fsPath, ec);

        const bool created = std::filesystem::create_directories(fsPath, ec);
        return !ec && (created || std::filesystem::is_directory(fsPath, ec));
    }

    inline bool Remove(const std::string& path)
    {
        std::error_code ec;
        const bool removed = std::filesystem::remove(detail::ToPath(path), ec);
        return !ec && removed;
    }

    inline bool CopyFile(const std::string& source, const std::string& target)
    {
        std::error_code ec;
        const bool copied = std::filesystem::copy_file(
            detail::ToPath(source),
            detail::ToPath(target),
            std::filesystem::copy_options::overwrite_existing,
            ec
        );
        return !ec && copied;
    }

    inline bool MoveFile(const std::string& source, const std::string& target)
    {
        std::error_code ec;
        std::filesystem::rename(detail::ToPath(source), detail::ToPath(target), ec);
        return !ec;
    }

    inline std::string CurrentPath()
    {
        std::error_code ec;
        const std::filesystem::path path = std::filesystem::current_path(ec);
        if (ec)
            detail::ThrowFileError("Failed to query current path: " + ec.message());

        return detail::ToGenericString(path);
    }

    inline std::string ReadText(const std::string& path)
    {
        std::ifstream input(detail::ToPath(path), std::ios::binary);
        if (!input)
            detail::ThrowFileError("Failed to open file for reading: " + path);

        std::ostringstream builder;
        builder << input.rdbuf();

        if (!input.good() && !input.eof())
            detail::ThrowFileError("Failed while reading file: " + path);

        return builder.str();
    }

    inline bool WriteText(const std::string& path, const std::string& text)
    {
        std::ofstream output(detail::ToPath(path), std::ios::binary | std::ios::trunc);
        if (!output)
            return false;

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return output.good();
    }

    inline bool AppendText(const std::string& path, const std::string& text)
    {
        std::ofstream output(detail::ToPath(path), std::ios::binary | std::ios::app);
        if (!output)
            return false;

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return output.good();
    }

    inline std::int64_t FileSize(const std::string& path)
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(detail::ToPath(path), ec);
        if (ec)
            detail::ThrowFileError("Failed to query file size for: " + path + " (" + ec.message() + ")");

        return static_cast<std::int64_t>(size);
    }

    inline std::string FileName(const std::string& path)
    {
        return detail::ToPath(path).filename().generic_string();
    }

    inline std::string Stem(const std::string& path)
    {
        return detail::ToPath(path).stem().generic_string();
    }

    inline std::string Extension(const std::string& path)
    {
        return detail::ToPath(path).extension().generic_string();
    }

    inline std::string ParentPath(const std::string& path)
    {
        return detail::ToPath(path).parent_path().generic_string();
    }

    inline std::string Normalize(const std::string& path)
    {
        return detail::ToPath(path).lexically_normal().generic_string();
    }

    inline std::string Join(const std::string& left, const std::string& right)
    {
        return detail::ToGenericString(detail::ToPath(left) / detail::ToPath(right));
    }

    inline std::string Join3(const std::string& first, const std::string& second, const std::string& third)
    {
        return detail::ToGenericString(detail::ToPath(first) / detail::ToPath(second) / detail::ToPath(third));
    }

    inline std::string ChangeExtension(const std::string& path, const std::string& extension)
    {
        std::filesystem::path changed = detail::ToPath(path);
        changed.replace_extension(extension);
        return detail::ToGenericString(changed);
    }
}
