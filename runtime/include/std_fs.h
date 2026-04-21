#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "exception.h"

namespace wio::runtime::std_fs
{
    bool Exists(const std::string& path);
    bool IsFile(const std::string& path);
    bool IsDirectory(const std::string& path);
    bool IsAbsolute(const std::string& path);
    bool CreateDirectories(const std::string& path);
    bool Remove(const std::string& path);
    bool CopyFile(const std::string& source, const std::string& target);
    bool MoveFile(const std::string& source, const std::string& target);
    std::string CurrentPath();
    std::string ReadText(const std::string& path);
    bool WriteText(const std::string& path, const std::string& text);
    bool AppendText(const std::string& path, const std::string& text);
    std::int64_t FileSize(const std::string& path);
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
}
