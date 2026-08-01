#include "std_fs.h"

#include "exception.h"

#include <algorithm>

namespace wio::runtime::std_fs
{
    namespace
    {
        std::filesystem::path toPath(const std::string& value)
        {
            return std::filesystem::path(value);
        }

        std::string toGenericString(const std::filesystem::path& value)
        {
            return value.generic_string();
        }

        [[noreturn]] void throwFileError(const std::string& message)
        {
            throw FileError(message.c_str());
        }
    }

    bool Exists(const std::string& path)
    {
        std::error_code ec;
        const bool exists = std::filesystem::exists(toPath(path), ec);
        return !ec && exists;
    }

    bool IsFile(const std::string& path)
    {
        std::error_code ec;
        const bool result = std::filesystem::is_regular_file(toPath(path), ec);
        return !ec && result;
    }

    bool IsDirectory(const std::string& path)
    {
        std::error_code ec;
        const bool result = std::filesystem::is_directory(toPath(path), ec);
        return !ec && result;
    }

    bool IsDirectoryEmpty(const std::string& path)
    {
        std::error_code ec;
        const std::filesystem::path fsPath = toPath(path);
        if (!std::filesystem::is_directory(fsPath, ec) || ec)
            return false;

        const bool result = std::filesystem::is_empty(fsPath, ec);
        return !ec && result;
    }

    bool IsAbsolute(const std::string& path)
    {
        return toPath(path).is_absolute();
    }

    bool CreateDirectories(const std::string& path)
    {
        if (path.empty())
            return false;

        std::error_code ec;
        const std::filesystem::path fsPath = toPath(path);

        if (std::filesystem::exists(fsPath, ec))
            return !ec && std::filesystem::is_directory(fsPath, ec);

        const bool created = std::filesystem::create_directories(fsPath, ec);
        return !ec && (created || std::filesystem::is_directory(fsPath, ec));
    }

    bool Remove(const std::string& path)
    {
        std::error_code ec;
        const bool removed = std::filesystem::remove(toPath(path), ec);
        return !ec && removed;
    }

    bool RemoveAll(const std::string& path)
    {
        std::error_code ec;
        const std::filesystem::path fsPath = toPath(path);
        if (!std::filesystem::exists(fsPath, ec))
            return !ec;

        std::filesystem::remove_all(fsPath, ec);
        return !ec && !std::filesystem::exists(fsPath, ec);
    }

    bool CopyFile(const std::string& source, const std::string& target)
    {
        std::error_code ec;
        const bool copied = std::filesystem::copy_file(
            toPath(source),
            toPath(target),
            std::filesystem::copy_options::overwrite_existing,
            ec
        );
        return !ec && copied;
    }

    bool CopyRecursive(const std::string& source, const std::string& target)
    {
        std::error_code ec;
        const std::filesystem::path sourcePath = toPath(source);
        const std::filesystem::path targetPath = toPath(target);
        if (!std::filesystem::exists(sourcePath, ec) || ec)
            return false;

        if (std::filesystem::is_directory(sourcePath, ec))
        {
            std::filesystem::create_directories(targetPath, ec);
            if (ec)
                return false;
            std::filesystem::copy(
                sourcePath,
                targetPath,
                std::filesystem::copy_options::recursive |
                    std::filesystem::copy_options::overwrite_existing,
                ec
            );
            return !ec;
        }

        std::filesystem::create_directories(targetPath.parent_path(), ec);
        if (ec)
            return false;
        std::filesystem::copy_file(
            sourcePath,
            targetPath,
            std::filesystem::copy_options::overwrite_existing,
            ec
        );
        return !ec;
    }

    bool MoveFile(const std::string& source, const std::string& target)
    {
        std::error_code ec;
        std::filesystem::rename(toPath(source), toPath(target), ec);
        return !ec;
    }

    std::vector<std::string> ListFilesRecursive(const std::string& path)
    {
        std::vector<std::string> files;
        std::error_code ec;
        const std::filesystem::path root = toPath(path);
        if (!std::filesystem::is_directory(root, ec) || ec)
            return files;

        for (std::filesystem::recursive_directory_iterator it(root, ec), end;
             it != end && !ec;
             it.increment(ec))
        {
            if (!ec && it->is_regular_file(ec) && !ec)
                files.push_back(toGenericString(it->path().lexically_normal()));
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    std::string CurrentPath()
    {
        std::error_code ec;
        const std::filesystem::path path = std::filesystem::current_path(ec);
        if (ec)
            throwFileError("Failed to query current path: " + ec.message());

        return toGenericString(path);
    }

    std::string ReadText(const std::string& path)
    {
        std::ifstream input(toPath(path), std::ios::binary);
        if (!input)
            throwFileError("Failed to open file for reading: " + path);

        std::ostringstream builder;
        builder << input.rdbuf();

        if (!input.good() && !input.eof())
            throwFileError("Failed while reading file: " + path);

        return builder.str();
    }

    bool WriteText(const std::string& path, const std::string& text)
    {
        std::ofstream output(toPath(path), std::ios::binary | std::ios::trunc);
        if (!output)
            return false;

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return output.good();
    }

    bool AppendText(const std::string& path, const std::string& text)
    {
        std::ofstream output(toPath(path), std::ios::binary | std::ios::app);
        if (!output)
            return false;

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return output.good();
    }

    bool IsExecutable(const std::string& path)
    {
        std::error_code ec;
        const std::filesystem::path fsPath = toPath(path);
        if (!std::filesystem::is_regular_file(fsPath, ec) || ec)
            return false;

#if defined(_WIN32)
        return true;
#else
        const auto permissions = std::filesystem::status(fsPath, ec).permissions();
        if (ec)
            return false;
        constexpr auto executableBits =
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec;
        return (permissions & executableBits) != std::filesystem::perms::none;
#endif
    }

    bool SetExecutable(const std::string& path, const bool executable)
    {
        std::error_code ec;
        const std::filesystem::path fsPath = toPath(path);
        if (!std::filesystem::is_regular_file(fsPath, ec) || ec)
            return false;

#if defined(_WIN32)
        (void)executable;
        return true;
#else
        constexpr auto executableBits =
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec;
        std::filesystem::permissions(
            fsPath,
            executableBits,
            executable ? std::filesystem::perm_options::add : std::filesystem::perm_options::remove,
            ec
        );
        return !ec;
#endif
    }

    std::int64_t FileSize(const std::string& path)
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(toPath(path), ec);
        if (ec)
            throwFileError("Failed to query file size for: " + path + " (" + ec.message() + ")");

        return static_cast<std::int64_t>(size);
    }

    std::int64_t LastWriteTime(const std::string& path)
    {
        const std::filesystem::path fsPath = toPath(path);
        std::error_code ec;
        if (!std::filesystem::exists(fsPath, ec) || ec)
            return -1;

        auto timestamp = std::filesystem::last_write_time(fsPath, ec);
        if (ec)
            return -1;
        std::int64_t latest = static_cast<std::int64_t>(timestamp.time_since_epoch().count());

        if (!std::filesystem::is_directory(fsPath, ec) || ec)
            return latest;

        for (std::filesystem::recursive_directory_iterator it(fsPath, ec), end;
             it != end && !ec;
             it.increment(ec))
        {
            const auto childTimestamp = it->last_write_time(ec);
            if (ec)
                break;
            latest = (std::max)(
                latest,
                static_cast<std::int64_t>(childTimestamp.time_since_epoch().count())
            );
        }
        return ec ? -1 : latest;
    }

    std::string FileName(const std::string& path)
    {
        return toPath(path).filename().generic_string();
    }

    std::string Stem(const std::string& path)
    {
        return toPath(path).stem().generic_string();
    }

    std::string Extension(const std::string& path)
    {
        return toPath(path).extension().generic_string();
    }

    std::string RootName(const std::string& path)
    {
        return toPath(path).root_name().generic_string();
    }

    std::string RootPath(const std::string& path)
    {
        return toPath(path).root_path().generic_string();
    }

    std::string ParentPath(const std::string& path)
    {
        return toPath(path).parent_path().generic_string();
    }

    std::string Normalize(const std::string& path)
    {
        return toPath(path).lexically_normal().generic_string();
    }

    std::string Absolute(const std::string& path)
    {
        std::error_code ec;
        const std::filesystem::path absolutePath = std::filesystem::absolute(toPath(path), ec);
        if (ec)
            throwFileError("Failed to resolve absolute path for: " + path + " (" + ec.message() + ")");

        return toGenericString(absolutePath.lexically_normal());
    }

    std::string Relative(const std::string& path, const std::string& base)
    {
        std::error_code ec;
        std::filesystem::path relativePath = std::filesystem::relative(toPath(path), toPath(base), ec);
        if (!ec)
            return toGenericString(relativePath.lexically_normal());

        ec.clear();
        const std::filesystem::path absolutePath = std::filesystem::absolute(toPath(path), ec);
        if (ec)
            throwFileError("Failed to resolve path for relative conversion: " + path + " (" + ec.message() + ")");

        ec.clear();
        const std::filesystem::path absoluteBase = std::filesystem::absolute(toPath(base), ec);
        if (ec)
            throwFileError("Failed to resolve base path for relative conversion: " + base + " (" + ec.message() + ")");

        relativePath = absolutePath.lexically_relative(absoluteBase);
        return toGenericString(relativePath.lexically_normal());
    }

    bool Equivalent(const std::string& left, const std::string& right)
    {
        std::error_code ec;
        const bool sameFile = std::filesystem::equivalent(toPath(left), toPath(right), ec);
        if (!ec)
            return sameFile;

        ec.clear();
        const std::filesystem::path absoluteLeft = std::filesystem::absolute(toPath(left), ec);
        if (ec)
            return false;

        ec.clear();
        const std::filesystem::path absoluteRight = std::filesystem::absolute(toPath(right), ec);
        if (ec)
            return false;

        return absoluteLeft.lexically_normal() == absoluteRight.lexically_normal();
    }

    std::string Join(const std::string& left, const std::string& right)
    {
        return toGenericString(toPath(left) / toPath(right));
    }

    std::string Join3(const std::string& first, const std::string& second, const std::string& third)
    {
        return toGenericString(toPath(first) / toPath(second) / toPath(third));
    }

    std::string ChangeExtension(const std::string& path, const std::string& extension)
    {
        std::filesystem::path changed = toPath(path);
        changed.replace_extension(extension);
        return toGenericString(changed);
    }
}
