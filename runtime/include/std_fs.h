#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace wio::runtime::std_fs
{
    enum class ErrorCode : std::int32_t
    {
        None = 0,
        InvalidPath = 1,
        NotFound = 2,
        AlreadyExists = 3,
        NotDirectory = 4,
        IsDirectory = 5,
        PermissionDenied = 6,
        Io = 7,
        Unsupported = 8,
        Unknown = 9
    };

    bool Exists(const std::string& path);
    bool IsFile(const std::string& path);
    bool IsDirectory(const std::string& path);
    bool IsDirectoryEmpty(const std::string& path);
    bool IsAbsolute(const std::string& path);
    bool CreateDirectories(const std::string& path);
    bool Remove(const std::string& path);
    bool RemoveAll(const std::string& path);
    bool CopyFile(const std::string& source, const std::string& target);
    bool CopyRecursive(const std::string& source, const std::string& target);
    bool MoveFile(const std::string& source, const std::string& target);
    std::vector<std::string> ListFilesRecursive(const std::string& path);
    std::string CurrentPath();
    std::string ReadText(const std::string& path);
    bool WriteText(const std::string& path, const std::string& text);
    bool AppendText(const std::string& path, const std::string& text);
    bool IsExecutable(const std::string& path);
    bool SetExecutable(const std::string& path, bool executable);
    std::int64_t FileSize(const std::string& path);
    std::int64_t LastWriteTime(const std::string& path);
    std::string FileName(const std::string& path);
    std::string Stem(const std::string& path);
    std::string Extension(const std::string& path);
    std::string RootName(const std::string& path);
    std::string RootPath(const std::string& path);
    std::string ParentPath(const std::string& path);
    std::string Normalize(const std::string& path);
    std::string Absolute(const std::string& path);
    std::string Relative(const std::string& path, const std::string& base);
    bool Equivalent(const std::string& left, const std::string& right);
    std::string Join(const std::string& left, const std::string& right);
    std::string Join3(const std::string& first, const std::string& second, const std::string& third);
    std::string ChangeExtension(const std::string& path, const std::string& extension);

    bool TryCurrentPathResult(std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryCreateDirectoriesResult(const std::string& path, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryRemoveResult(const std::string& path, bool& removed, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryRemoveAllResult(const std::string& path, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryCopyFileResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryCopyRecursiveResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryMoveFileResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryReplaceFileAtomicResult(const std::string& source, const std::string& target, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryListFilesRecursiveResult(const std::string& path, std::vector<std::string>& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryReadTextResult(const std::string& path, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryWriteTextResult(const std::string& path, const std::string& text, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryAppendTextResult(const std::string& path, const std::string& text, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TrySetExecutableResult(const std::string& path, bool executable, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryFileSizeResult(const std::string& path, std::int64_t& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryLastWriteTimeResult(const std::string& path, std::int64_t& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryMetadataResult(const std::string& path, bool& isFile, bool& isDirectory, std::int64_t& size, std::int64_t& lastWriteTime, bool& executable, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryAbsoluteResult(const std::string& path, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryCanonicalResult(const std::string& path, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
    bool TryRelativeResult(const std::string& path, const std::string& base, std::string& value, std::int32_t& error, std::int64_t& nativeError, std::string& message);
}
