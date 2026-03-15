#pragma once
#include <filesystem>
#include <string>

namespace wio::common::filesystem
{
    enum class Encoding : uint8_t { Unknown, UTF8, UTF16_LE, UTF16_BE, UTF32_LE, UTF32_BE };

    FILE* openFile(const std::filesystem::path& path, const char* mode);
    std::filesystem::path getAbsPath(const std::filesystem::path& filepath);
    std::filesystem::path getCanonicalPath(const std::filesystem::path& filepath);
    bool checkExtension(const std::filesystem::path& filepath, const std::string& expected_extension);
    std::filesystem::path changeExtension(const std::filesystem::path& filepath, const std::string& new_extension);
    bool fileExists(const std::filesystem::path& filepath);
    bool createDirectories(const std::filesystem::path& path);
    bool hasPrefix(const std::string& str, const std::string& prefix);
    std::string readFile(const std::filesystem::path& filepath);
    std::string readFile(FILE* file);
    std::string readStdin();
    bool writeFileBase(FILE* file, const std::string& output);
    std::vector<std::string> listFiles(const std::filesystem::path& dir_path, bool recursive = true);
    Encoding stripBOM(std::string& text);

    template <typename T>
    bool writeFile(const T& value, FILE* file);
    template <typename T>
    bool writeStdout(const T& value);
    template <typename T>
    bool writeFilepath(const T& value, const std::filesystem::path& filepath);
}

namespace wio
{
    namespace filesystem = common::filesystem;
}

#include "filesystem.inl"