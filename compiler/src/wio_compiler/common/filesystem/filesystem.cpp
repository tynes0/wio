#include "wio/common/filesystem/filesystem.h"

#include "general/core.h"
#include "wio/common/exception.h"

#include <array>
#include <cstdio>  // FILE, fopen, fread, fwrite, fclose
#include <cstring> // strlen
#include <vector>
#include <system_error> 

namespace wio::common::filesystem
{
    FILE* openFile(const std::filesystem::path& path, const char* mode)
    {
#ifdef _MSC_VER
        std::wstring wmode(mode, mode + strlen(mode));
        FILE* f = nullptr;
        errno_t eno = _wfopen_s(&f, path.c_str(), wmode.c_str());
        if (eno != 0)
            return nullptr;
        return f;
#else
        return fopen(path.c_str(), mode);
#endif
    }
    // -----------------------------------------------------------------------
    // PATH OPERATIONS
    // -----------------------------------------------------------------------

    std::filesystem::path getAbsPath(const std::filesystem::path& filepath)
    {
        return std::filesystem::absolute(filepath);
    }

    std::filesystem::path getCanonicalPath(const std::filesystem::path& filepath)
    {
        std::error_code ec;
        auto cPath = std::filesystem::weakly_canonical(filepath, ec);
        return (ec) ? filepath : cPath;
    }

    bool checkExtension(const std::filesystem::path& filepath, const std::string& expected_extension)
    {
        return filepath.extension() == expected_extension;
    }

    std::filesystem::path changeExtension(const std::filesystem::path& filepath, const std::string& new_extension)
    {
        std::filesystem::path p = filepath;
        return p.replace_extension(new_extension);
    }

    bool fileExists(const std::filesystem::path& filepath)
    {
        std::error_code ec;
        return std::filesystem::exists(filepath, ec);
    }

    bool createDirectories(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::create_directories(path, ec);
    }

    bool hasPrefix(const std::string& str, const std::string& prefix)
    {
        return str.starts_with(prefix);
    }

    // -----------------------------------------------------------------------
    // READ OPERATIONS (High Performance C-Style)
    // -----------------------------------------------------------------------

    std::string readFile(const std::filesystem::path& filepath)
    {
        FILE* file = openFile(filepath, "rb");
        if (!file)
            return {};

        if (fseek(file, 0, SEEK_END) != 0)
        {
            (void)fclose(file);
            return {};
        }
        
        long fileSize = ftell(file);
        if (fileSize <= 0)
        {
            (void)fclose(file);
            return {};
        }

        if (fseek(file, 0, SEEK_SET)) return {};

        std::string buffer; 
        try
        {
            buffer.resize(fileSize);
        }
        catch (...)
        {
            (void)fclose(file);
            throw OutOfMemory("File reading failed. Memory exceeded.");
        }

        size_t readSize = fread(buffer.data(), 1, fileSize, file);
        (void)fclose(file);

        // NOLINTNEXTLINE(modernize-use-integer-sign-comparison)
        if (readSize != static_cast<size_t>(fileSize))
            buffer.resize(readSize);

        return buffer;
    }

    std::string readFile(FILE* file)
    {
        if (!file) return {};

        if (fseek(file, 0, SEEK_END) != 0) return {};
        long fileSize = ftell(file);
        if (fileSize <= 0) return {};
        if (fseek(file, 0, SEEK_SET) != 0) return {};

        std::string buffer;
        buffer.resize(fileSize);
        
        size_t readSize = fread(buffer.data(), 1, fileSize, file);
        buffer.resize(readSize);

        return buffer;
    }

    std::string readStdin()
    {
        std::string buffer;
        std::array<char, WIO_BUFSIZ> temp{};

        while (fgets(temp.data(), static_cast<int>(temp.size()), stdin))
        {
            size_t len = strlen(temp.data());

            if (len > 0 && temp[len - 1] == '\n')
            {
                temp[--len] = '\0';
                buffer.append(temp.data(), len);
                break;
            }
            
            buffer.append(temp.data(), len);
        }
        return buffer;
    }

    // -----------------------------------------------------------------------
    // WRITE OPERATIONS
    // -----------------------------------------------------------------------

    bool writeFileBase(FILE* file, const std::string& output)
    {
        if (!file)
        {
            throw FileError("Writing failed: Null file.");
        }
        
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
    
    // -----------------------------------------------------------------------
    // LISTING OPERATIONS
    // -----------------------------------------------------------------------

    std::vector<std::string> listFiles(const std::filesystem::path& dir_path, bool recursive)
    {
        std::vector<std::string> files;
        std::error_code ec;

        if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec))
            return files;

        try
        {
            if (recursive)
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path, ec))
                {
                    if (entry.is_regular_file(ec))
                        files.push_back(entry.path().string());
                }
            } else
            {
                for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec))
                {
                    if (entry.is_regular_file(ec))
                        files.push_back(entry.path().string());
                }
            }
        }
        catch (...)
        {
            return {}; 
        }
        return files;
    }

    struct BomEntry
    {
        std::initializer_list<unsigned char> bytes;
        Encoding encoding;
    };

    static const BomEntry table[] = {
        { .bytes = {0xEF,0xBB,0xBF},        .encoding = Encoding::UTF8},
        { .bytes = {0xFF,0xFE},             .encoding = Encoding::UTF16_LE},
        { .bytes = {0xFE,0xFF},             .encoding = Encoding::UTF16_BE},
        { .bytes = {0xFF,0xFE,0x00,0x00},   .encoding = Encoding::UTF32_LE},
        { .bytes = {0x00,0x00,0xFE,0xFF},   .encoding = Encoding::UTF32_BE},
    };
    
    // Strips BOM if present.
    // Does NOT convert encoding.
    // If UTF-16/32 is detected, caller must handle conversion.
    Encoding stripBOM(std::string& text)
    {
        for (auto entry : table)
        {
            if (text.size() < entry.bytes.size())
                continue;
                
            if (entry.bytes.size() == 2)
            {
                if (static_cast<unsigned char>(text[0]) == entry.bytes.begin()[0] &&
                    static_cast<unsigned char>(text[1]) == entry.bytes.begin()[1])
                {
                    text.erase(0, 2);
                    return entry.encoding;
                }
            }
            if (entry.bytes.size() == 3)
            {
                if (static_cast<unsigned char>(text[0]) == entry.bytes.begin()[0] &&
                    static_cast<unsigned char>(text[1]) == entry.bytes.begin()[1] &&
                    static_cast<unsigned char>(text[2]) == entry.bytes.begin()[2])
                {
                    text.erase(0, 3);
                    return entry.encoding;
                }
            }
            if (entry.bytes.size() == 4)
            {
                if (static_cast<unsigned char>(text[0]) == entry.bytes.begin()[0] &&
                    static_cast<unsigned char>(text[1]) == entry.bytes.begin()[1] &&
                    static_cast<unsigned char>(text[2]) == entry.bytes.begin()[2] &&
                    static_cast<unsigned char>(text[3]) == entry.bytes.begin()[3])
                {
                    text.erase(0, 4);
                    return entry.encoding;
                }
            }
        }
        return Encoding::Unknown;
    }
}
