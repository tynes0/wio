#include "std_fs.h"

#include "exception.h"

#include <algorithm>
#include <cerrno>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#undef CopyFile
#undef MoveFile
#endif

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

        std::int32_t classifyError(const std::error_code& error)
        {
            if (!error)
                return static_cast<std::int32_t>(ErrorCode::None);
            if (error == std::errc::invalid_argument || error == std::errc::filename_too_long)
                return static_cast<std::int32_t>(ErrorCode::InvalidPath);
            if (error == std::errc::no_such_file_or_directory)
                return static_cast<std::int32_t>(ErrorCode::NotFound);
            if (error == std::errc::file_exists)
                return static_cast<std::int32_t>(ErrorCode::AlreadyExists);
            if (error == std::errc::not_a_directory)
                return static_cast<std::int32_t>(ErrorCode::NotDirectory);
            if (error == std::errc::is_a_directory)
                return static_cast<std::int32_t>(ErrorCode::IsDirectory);
            if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted)
                return static_cast<std::int32_t>(ErrorCode::PermissionDenied);
            if (error == std::errc::operation_not_supported || error == std::errc::function_not_supported)
                return static_cast<std::int32_t>(ErrorCode::Unsupported);
            if (error.category() == std::generic_category() || error.category() == std::system_category())
                return static_cast<std::int32_t>(ErrorCode::Io);
            return static_cast<std::int32_t>(ErrorCode::Unknown);
        }

        void clearResultError(std::int32_t& error, std::int64_t& nativeError, std::string& message)
        {
            error = static_cast<std::int32_t>(ErrorCode::None);
            nativeError = 0;
            message.clear();
        }

        bool failResult(const std::string& operation,
                        const std::string& paths,
                        std::error_code native,
                        std::int32_t& error,
                        std::int64_t& nativeError,
                        std::string& message)
        {
            if (!native)
                native = std::make_error_code(std::errc::io_error);
            error = classifyError(native);
            nativeError = static_cast<std::int64_t>(native.value());
            message = operation + " failed";
            if (!paths.empty())
                message += " for " + paths;
            message += ": " + native.message();
            return false;
        }

        std::error_code streamError()
        {
            if (errno != 0)
                return std::error_code(errno, std::generic_category());
            return std::make_error_code(std::errc::io_error);
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

    bool TryCurrentPathResult(std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto path = std::filesystem::current_path(ec);
        if (ec)
            return failResult("query current path", "the current process", ec, error, nativeError, message);
        value = toGenericString(path);
        return true;
    }

    bool TryCreateDirectoriesResult(const std::string& path, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        if (path.empty())
            return failResult("create directories", "an empty path", std::make_error_code(std::errc::invalid_argument), error, nativeError, message);
        std::error_code ec;
        const auto fsPath = toPath(path);
        if (std::filesystem::exists(fsPath, ec))
        {
            if (ec)
                return failResult("inspect directory", path, ec, error, nativeError, message);
            if (std::filesystem::is_directory(fsPath, ec) && !ec)
                return true;
            if (!ec)
                ec = std::make_error_code(std::errc::not_a_directory);
            return failResult("create directories", path, ec, error, nativeError, message);
        }
        if (ec)
            return failResult("inspect directory", path, ec, error, nativeError, message);
        std::filesystem::create_directories(fsPath, ec);
        if (ec)
            return failResult("create directories", path, ec, error, nativeError, message);
        return true;
    }

    bool TryRemoveResult(const std::string& path, bool& removed, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        removed = std::filesystem::remove(toPath(path), ec);
        if (ec)
            return failResult("remove", path, ec, error, nativeError, message);
        return true;
    }

    bool TryRemoveAllResult(const std::string& path, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        std::filesystem::remove_all(toPath(path), ec);
        if (ec)
            return failResult("remove recursively", path, ec, error, nativeError, message);
        return true;
    }

    bool TryCopyFileResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        std::filesystem::copy_file(toPath(source), toPath(target), std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
            return failResult("copy file", source + " -> " + target, ec, error, nativeError, message);
        return true;
    }

    bool TryCopyRecursiveResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto sourcePath = toPath(source);
        const auto targetPath = toPath(target);
        if (!std::filesystem::exists(sourcePath, ec) || ec)
        {
            if (!ec)
                ec = std::make_error_code(std::errc::no_such_file_or_directory);
            return failResult("copy recursively", source + " -> " + target, ec, error, nativeError, message);
        }
        if (std::filesystem::is_directory(sourcePath, ec))
        {
            std::filesystem::create_directories(targetPath, ec);
            if (!ec)
                std::filesystem::copy(sourcePath, targetPath,
                    std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        }
        else
        {
            const auto parent = targetPath.parent_path();
            if (!parent.empty())
                std::filesystem::create_directories(parent, ec);
            if (!ec)
                std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
        }
        if (ec)
            return failResult("copy recursively", source + " -> " + target, ec, error, nativeError, message);
        return true;
    }

    bool TryMoveFileResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        std::filesystem::rename(toPath(source), toPath(target), ec);
        if (ec)
            return failResult("move", source + " -> " + target, ec, error, nativeError, message);
        return true;
    }

    bool TryReplaceFileAtomicResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
#if defined(_WIN32)
        if (!MoveFileExW(toPath(source).wstring().c_str(), toPath(target).wstring().c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            const auto ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
            return failResult("atomically replace", source + " -> " + target, ec, error, nativeError, message);
        }
#else
        std::error_code ec;
        std::filesystem::rename(toPath(source), toPath(target), ec);
        if (ec)
            return failResult("atomically replace", source + " -> " + target, ec, error, nativeError, message);
#endif
        return true;
    }

    bool TryListFilesRecursiveResult(const std::string& path, std::vector<std::string>& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        value.clear();
        std::error_code ec;
        const auto root = toPath(path);
        if (!std::filesystem::is_directory(root, ec) || ec)
        {
            if (!ec)
                ec = std::make_error_code(std::errc::not_a_directory);
            return failResult("enumerate directory", path, ec, error, nativeError, message);
        }
        for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end && !ec; it.increment(ec))
        {
            if (it->is_regular_file(ec) && !ec)
                value.push_back(toGenericString(it->path().lexically_normal()));
        }
        if (ec)
            return failResult("enumerate directory", path, ec, error, nativeError, message);
        std::sort(value.begin(), value.end());
        return true;
    }

    bool TryReadTextResult(const std::string& path, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        errno = 0;
        std::ifstream input(toPath(path), std::ios::binary);
        if (!input)
            return failResult("read text", path, streamError(), error, nativeError, message);
        std::ostringstream builder;
        builder << input.rdbuf();
        if (!input.good() && !input.eof())
            return failResult("read text", path, streamError(), error, nativeError, message);
        value = builder.str();
        return true;
    }

    bool TryWriteTextResult(const std::string& path, const std::string& text, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        errno = 0;
        std::ofstream output(toPath(path), std::ios::binary | std::ios::trunc);
        if (!output)
            return failResult("write text", path, streamError(), error, nativeError, message);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!output.good())
            return failResult("write text", path, streamError(), error, nativeError, message);
        return true;
    }

    bool TryAppendTextResult(const std::string& path, const std::string& text, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        errno = 0;
        std::ofstream output(toPath(path), std::ios::binary | std::ios::app);
        if (!output)
            return failResult("append text", path, streamError(), error, nativeError, message);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!output.good())
            return failResult("append text", path, streamError(), error, nativeError, message);
        return true;
    }

    bool TrySetExecutableResult(const std::string& path, const bool executable, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto fsPath = toPath(path);
        if (!std::filesystem::is_regular_file(fsPath, ec) || ec)
        {
            if (!ec)
                ec = std::make_error_code(std::errc::no_such_file_or_directory);
            return failResult("set executable permission", path, ec, error, nativeError, message);
        }
#if !defined(_WIN32)
        constexpr auto bits = std::filesystem::perms::owner_exec |
                              std::filesystem::perms::group_exec |
                              std::filesystem::perms::others_exec;
        std::filesystem::permissions(fsPath, bits,
            executable ? std::filesystem::perm_options::add : std::filesystem::perm_options::remove, ec);
        if (ec)
            return failResult("set executable permission", path, ec, error, nativeError, message);
#else
        (void)executable;
#endif
        return true;
    }

    bool TryFileSizeResult(const std::string& path, std::int64_t& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto size = std::filesystem::file_size(toPath(path), ec);
        if (ec)
            return failResult("query file size", path, ec, error, nativeError, message);
        value = static_cast<std::int64_t>(size);
        return true;
    }

    bool TryLastWriteTimeResult(const std::string& path, std::int64_t& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto timestamp = std::filesystem::last_write_time(toPath(path), ec);
        if (ec)
            return failResult("query last write time", path, ec, error, nativeError, message);
        value = static_cast<std::int64_t>(timestamp.time_since_epoch().count());
        return true;
    }

    bool TryMetadataResult(const std::string& path, bool& isFile, bool& isDirectory, std::int64_t& size,
                           std::int64_t& lastWriteTime, bool& executable, std::int32_t& error,
                           std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto fsPath = toPath(path);
        const auto status = std::filesystem::status(fsPath, ec);
        if (ec)
            return failResult("query metadata", path, ec, error, nativeError, message);
        if (!std::filesystem::exists(status))
            return failResult("query metadata", path, std::make_error_code(std::errc::no_such_file_or_directory), error, nativeError, message);
        isFile = std::filesystem::is_regular_file(status);
        isDirectory = std::filesystem::is_directory(status);
        size = -1;
        if (isFile)
        {
            const auto rawSize = std::filesystem::file_size(fsPath, ec);
            if (ec)
                return failResult("query metadata size", path, ec, error, nativeError, message);
            size = static_cast<std::int64_t>(rawSize);
        }
        const auto timestamp = std::filesystem::last_write_time(fsPath, ec);
        if (ec)
            return failResult("query metadata timestamp", path, ec, error, nativeError, message);
        lastWriteTime = static_cast<std::int64_t>(timestamp.time_since_epoch().count());
#if defined(_WIN32)
        executable = isFile;
#else
        constexpr auto bits = std::filesystem::perms::owner_exec |
                              std::filesystem::perms::group_exec |
                              std::filesystem::perms::others_exec;
        executable = isFile && (status.permissions() & bits) != std::filesystem::perms::none;
#endif
        return true;
    }

    bool TryAbsoluteResult(const std::string& path, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto resolved = std::filesystem::absolute(toPath(path), ec);
        if (ec)
            return failResult("resolve absolute path", path, ec, error, nativeError, message);
        value = toGenericString(resolved.lexically_normal());
        return true;
    }

    bool TryCanonicalResult(const std::string& path, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto resolved = std::filesystem::canonical(toPath(path), ec);
        if (ec)
            return failResult("canonicalize path", path, ec, error, nativeError, message);
        value = toGenericString(resolved);
        return true;
    }

    bool TryRelativeResult(const std::string& path, const std::string& base, std::string& value,
                           std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        clearResultError(error, nativeError, message);
        std::error_code ec;
        const auto resolved = std::filesystem::relative(toPath(path), toPath(base), ec);
        if (ec)
            return failResult("resolve relative path", path + " from " + base, ec, error, nativeError, message);
        value = toGenericString(resolved.lexically_normal());
        return true;
    }
}
