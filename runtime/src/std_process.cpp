#include "std_process.h"

#include <bit>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/wait.h>
#endif

namespace wio::runtime::std_process
{
    namespace
    {
        std::string quoteArgument(const std::string_view value)
        {
#if defined(_WIN32)
            if (value.empty())
                return "\"\"";

            bool needsQuotes = false;
            for (const char ch : value)
            {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '"' || ch == '&' || ch == '(' || ch == ')' || ch == ';')
                {
                    needsQuotes = true;
                    break;
                }
            }

            if (!needsQuotes)
                return std::string(value);

            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('"');
            for (const char ch : value)
            {
                if (ch == '"')
                    result += "\\\"";
                else
                    result.push_back(ch);
            }
            result.push_back('"');
            return result;
#else
            if (value.empty())
                return "''";

            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('\'');
            for (const char ch : value)
            {
                if (ch == '\'')
                    result += "'\\''";
                else
                    result.push_back(ch);
            }
            result.push_back('\'');
            return result;
#endif
        }

        std::string joinCommand(const std::string_view program, const std::vector<std::string>& args)
        {
            std::ostringstream stream;
            stream << quoteArgument(program);
            for (const auto& arg : args)
                stream << ' ' << quoteArgument(arg);
            return stream.str();
        }

        class ScopedCurrentPath
        {
        public:
            explicit ScopedCurrentPath(const std::filesystem::path& nextPath, std::error_code& ec)
            {
                previousPath_ = std::filesystem::current_path(ec);
                if (ec)
                    return;

                std::filesystem::current_path(nextPath, ec);
                if (!ec)
                    active_ = true;
            }

            ~ScopedCurrentPath()
            {
                if (!active_)
                    return;

                std::error_code restoreError;
                std::filesystem::current_path(previousPath_, restoreError);
            }

        private:
            std::filesystem::path previousPath_;
            bool active_ = false;
        };
    }

    const char* ToString(const ProcessError error) noexcept
    {
        switch (error)
        {
        case ProcessError::none:
            return "none";
        case ProcessError::empty_program:
            return "empty_program";
        case ProcessError::invalid_working_directory:
            return "invalid_working_directory";
        case ProcessError::launch_failed:
            return "launch_failed";
        }

        return "launch_failed";
    }

    std::string ExecutableSuffix()
    {
#if defined(_WIN32)
        return ".exe";
#else
        return "";
#endif
    }

    std::string SharedLibrarySuffix()
    {
#if defined(_WIN32)
        return ".dll";
#elif defined(__APPLE__)
        return ".dylib";
#else
        return ".so";
#endif
    }

    std::string StaticLibrarySuffix()
    {
        return ".a";
    }

    bool TryRunResult(
        const std::string_view program,
        const std::vector<std::string>& args,
        const std::string_view workingDirectory,
        int& exitCode,
        ProcessError& error,
        int& nativeError,
        std::string& message) noexcept
    {
        exitCode = -1;
        error = ProcessError::none;
        nativeError = 0;
        message.clear();

        if (program.empty())
        {
            error = ProcessError::empty_program;
            message = "process program cannot be empty";
            return false;
        }

        std::error_code pathError;
        std::optional<ScopedCurrentPath> pathGuard;
        if (!workingDirectory.empty())
        {
            const std::filesystem::path directoryPath = std::filesystem::path(std::string(workingDirectory));
            if (!std::filesystem::exists(directoryPath, pathError) ||
                !std::filesystem::is_directory(directoryPath, pathError))
            {
                error = ProcessError::invalid_working_directory;
                nativeError = static_cast<int>(pathError.value());
                message = "working directory does not exist: " + std::string(workingDirectory);
                return false;
            }

            pathGuard.emplace(directoryPath, pathError);
            if (pathError)
            {
                error = ProcessError::invalid_working_directory;
                nativeError = static_cast<int>(pathError.value());
                message = "could not switch to working directory: " + std::string(workingDirectory);
                return false;
            }
        }

        const std::string command = joinCommand(program, args);
        errno = 0;
        const int rawExitCode = std::system(command.c_str());
        if (rawExitCode == -1)
        {
            error = ProcessError::launch_failed;
            nativeError = errno;
            message = "process launch failed for: " + std::string(program);
            return false;
        }

#if defined(_WIN32)
        exitCode = rawExitCode;
#else
        if (WIFEXITED(rawExitCode))
            exitCode = WEXITSTATUS(rawExitCode);
        else
            exitCode = rawExitCode;
#endif

        return true;
    }
}

namespace wio::runtime::std_platform
{
    OperatingSystem CurrentOperatingSystem() noexcept
    {
#if defined(_WIN32)
        return OperatingSystem::windows;
#elif defined(__APPLE__) && defined(__MACH__)
        return OperatingSystem::macos;
#elif defined(__linux__)
        return OperatingSystem::linux;
#elif defined(__unix__)
        return OperatingSystem::unix_like;
#else
        return OperatingSystem::unknown;
#endif
    }

    Architecture CurrentArchitecture() noexcept
    {
#if defined(__wasm64__)
        return Architecture::wasm64;
#elif defined(__wasm32__)
        return Architecture::wasm32;
#elif defined(_M_ARM64) || defined(__aarch64__)
        return Architecture::arm64;
#elif defined(_M_ARM) || defined(__arm__)
        return Architecture::arm32;
#elif defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
        return Architecture::x64;
#elif defined(_M_IX86) || defined(__i386__)
        return Architecture::x86;
#else
        return Architecture::unknown;
#endif
    }

    const char* OperatingSystemName(const OperatingSystem value) noexcept
    {
        switch (value)
        {
        case OperatingSystem::windows: return "windows";
        case OperatingSystem::linux: return "linux";
        case OperatingSystem::macos: return "macos";
        case OperatingSystem::unix_like: return "unix";
        case OperatingSystem::unknown: break;
        }
        return "unknown";
    }

    const char* ArchitectureName(const Architecture value) noexcept
    {
        switch (value)
        {
        case Architecture::x86: return "x86";
        case Architecture::x64: return "x64";
        case Architecture::arm32: return "arm32";
        case Architecture::arm64: return "arm64";
        case Architecture::wasm32: return "wasm32";
        case Architecture::wasm64: return "wasm64";
        case Architecture::unknown: break;
        }
        return "unknown";
    }

    std::uint32_t PointerBits() noexcept
    {
        return static_cast<std::uint32_t>(sizeof(void*) * 8u);
    }

    bool IsLittleEndian() noexcept
    {
        return std::endian::native == std::endian::little;
    }

    std::uint32_t HardwareThreadCount() noexcept
    {
        return std::thread::hardware_concurrency();
    }

    std::string PathListSeparator()
    {
#if defined(_WIN32)
        return ";";
#else
        return ":";
#endif
    }

    std::string NativeNewLine()
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }
}

namespace wio::runtime::std_environment
{
    namespace
    {
#if defined(_WIN32)
        std::wstring widen(const std::string_view value)
        {
            if (value.empty())
                return {};
            const int size = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (size <= 0)
                return {};
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            if (MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    result.data(), size) <= 0)
            {
                return {};
            }
            return result;
        }

        std::string narrow(const std::wstring_view value)
        {
            if (value.empty())
                return {};
            const int size = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                nullptr, 0, nullptr, nullptr);
            if (size <= 0)
                return {};
            std::string result(static_cast<std::size_t>(size), '\0');
            if (WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    result.data(), size, nullptr, nullptr) <= 0)
            {
                return {};
            }
            return result;
        }

        bool openUserEnvironment(HKEY& key, const REGSAM access, const bool create) noexcept
        {
            key = nullptr;
            if (create)
            {
                DWORD disposition = 0;
                return RegCreateKeyExW(
                    HKEY_CURRENT_USER, L"Environment", 0, nullptr, 0, access, nullptr,
                    &key, &disposition) == ERROR_SUCCESS;
            }
            return RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, access, &key) == ERROR_SUCCESS;
        }

        bool readRegistryValue(HKEY key, const std::wstring& name, std::wstring& value) noexcept
        {
            value.clear();
            DWORD type = 0;
            DWORD bytes = 0;
            LONG status = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &bytes);
            if (status != ERROR_SUCCESS || bytes == 0 ||
                (type != REG_SZ && type != REG_EXPAND_SZ))
            {
                return false;
            }
            std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
            status = RegQueryValueExW(
                key, name.c_str(), nullptr, &type,
                reinterpret_cast<BYTE*>(buffer.data()), &bytes);
            if (status != ERROR_SUCCESS)
                return false;
            while (!buffer.empty() && buffer.back() == L'\0')
                buffer.pop_back();
            value = std::move(buffer);
            return true;
        }

        bool writeRegistryValue(
            HKEY key, const std::wstring& name, const std::wstring& value) noexcept
        {
            const DWORD bytes = static_cast<DWORD>((value.size() + 1u) * sizeof(wchar_t));
            return RegSetValueExW(
                key, name.c_str(), 0, REG_EXPAND_SZ,
                reinterpret_cast<const BYTE*>(value.c_str()), bytes) == ERROR_SUCCESS;
        }

        std::wstring normalizePath(std::wstring value)
        {
            std::replace(value.begin(), value.end(), L'/', L'\\');
            while (!value.empty() && (value.back() == L'\\' || value.back() == L'/'))
                value.pop_back();
            std::transform(value.begin(), value.end(), value.begin(), [](const wchar_t ch)
            {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            return value;
        }

        std::vector<std::wstring> splitUserPath(const std::wstring& value)
        {
            std::vector<std::wstring> entries;
            std::wstringstream stream(value);
            std::wstring entry;
            while (std::getline(stream, entry, L';'))
            {
                const auto first = entry.find_first_not_of(L" \t");
                const auto last = entry.find_last_not_of(L" \t");
                if (first != std::wstring::npos)
                    entries.push_back(entry.substr(first, last - first + 1u));
            }
            return entries;
        }

        void broadcastEnvironmentChange() noexcept
        {
            SendMessageTimeoutW(
                HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"Environment"),
                SMTO_ABORTIFHUNG, 5000, nullptr);
        }
#else
        constexpr std::string_view ProfileMarkerBegin = "# >>> wio environment >>>";
        constexpr std::string_view ProfileMarkerEnd = "# <<< wio environment <<<";

        bool validVariableName(const std::string_view name) noexcept
        {
            if (name.empty() ||
                !(std::isalpha(static_cast<unsigned char>(name.front())) != 0 ||
                  name.front() == '_'))
            {
                return false;
            }
            return std::all_of(name.begin() + 1, name.end(), [](const char ch)
            {
                return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
            });
        }

        std::filesystem::path profilePath()
        {
            const char* home = std::getenv("HOME");
            return home == nullptr || *home == '\0'
                ? std::filesystem::path{}
                : std::filesystem::path(home) / ".profile";
        }

        std::string readFile(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return {};
            std::ostringstream stream;
            stream << input.rdbuf();
            return stream.str();
        }

        std::string shellQuote(const std::string_view value)
        {
            std::string result = "'";
            for (const char ch : value)
            {
                if (ch == '\'')
                    result += "'\\''";
                else
                    result.push_back(ch);
            }
            result.push_back('\'');
            return result;
        }

        bool shellUnquote(const std::string_view value, std::string& result)
        {
            result.clear();
            if (value.size() < 2u || value.front() != '\'' || value.back() != '\'')
                return false;
            for (std::size_t index = 1u; index + 1u < value.size();)
            {
                if (index + 3u < value.size() && value.substr(index, 4u) == "'\\''")
                {
                    result.push_back('\'');
                    index += 4u;
                }
                else
                {
                    result.push_back(value[index]);
                    ++index;
                }
            }
            return true;
        }

        struct ProfileParts
        {
            std::string prefix;
            std::vector<std::string> lines;
            std::string suffix;
        };

        ProfileParts parseProfile(const std::string& content)
        {
            ProfileParts parts;
            const std::size_t begin = content.find(ProfileMarkerBegin);
            if (begin == std::string::npos)
            {
                parts.prefix = content;
                return parts;
            }
            parts.prefix = content.substr(0u, begin);
            const std::size_t bodyStart = begin + ProfileMarkerBegin.size();
            const std::size_t end = content.find(ProfileMarkerEnd, bodyStart);
            const std::string body = content.substr(
                bodyStart, end == std::string::npos ? std::string::npos : end - bodyStart);
            std::istringstream lines(body);
            std::string line;
            while (std::getline(lines, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                    parts.lines.push_back(line);
            }
            if (end != std::string::npos)
            {
                std::size_t suffixStart = end + ProfileMarkerEnd.size();
                if (suffixStart < content.size() && content[suffixStart] == '\r')
                    ++suffixStart;
                if (suffixStart < content.size() && content[suffixStart] == '\n')
                    ++suffixStart;
                parts.suffix = content.substr(suffixStart);
            }
            return parts;
        }

        bool writeProfile(ProfileParts parts)
        {
            const auto path = profilePath();
            if (path.empty())
                return false;
            std::ostringstream output;
            output << parts.prefix;
            if (!parts.prefix.empty() && parts.prefix.back() != '\n')
                output << '\n';
            if (!parts.lines.empty())
            {
                output << ProfileMarkerBegin << '\n';
                for (const auto& line : parts.lines)
                    output << line << '\n';
                output << ProfileMarkerEnd << '\n';
            }
            output << parts.suffix;
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            const std::string content = output.str();
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            return file.good();
        }

        std::string normalizedPosixPath(std::string value)
        {
            while (value.size() > 1u && value.back() == '/')
                value.pop_back();
            return value;
        }
#endif
    }

    bool TryGet(const std::string_view name, std::string& value) noexcept
    {
        value.clear();
        if (name.empty())
            return false;

        const std::string key(name);
#if defined(_WIN32)
        char* buffer = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&buffer, &length, key.c_str()) != 0 || buffer == nullptr)
            return false;
        value.assign(buffer);
        std::free(buffer);
        return true;
#else
        const char* result = std::getenv(key.c_str());
        if (result == nullptr)
            return false;
        value.assign(result);
        return true;
#endif
    }

    bool Set(const std::string_view name, const std::string_view value) noexcept
    {
        if (name.empty())
            return false;
        const std::string key(name);
        const std::string text(value);
#if defined(_WIN32)
        return _putenv_s(key.c_str(), text.c_str()) == 0;
#else
        return setenv(key.c_str(), text.c_str(), 1) == 0;
#endif
    }

    bool Remove(const std::string_view name) noexcept
    {
        if (name.empty())
            return false;
        const std::string key(name);
#if defined(_WIN32)
        return _putenv_s(key.c_str(), "") == 0;
#else
        return unsetenv(key.c_str()) == 0;
#endif
    }

    bool TryGetUser(const std::string_view name, std::string& value) noexcept
    {
        value.clear();
        if (name.empty())
            return false;
#if defined(_WIN32)
        const std::wstring wideName = widen(name);
        if (wideName.empty())
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_QUERY_VALUE, false))
            return false;
        std::wstring wideValue;
        const bool found = readRegistryValue(key, wideName, wideValue);
        RegCloseKey(key);
        if (!found)
            return false;
        value = narrow(wideValue);
        return true;
#else
        if (!validVariableName(name))
            return false;
        const ProfileParts parts = parseProfile(readFile(profilePath()));
        const std::string prefix = "export " + std::string(name) + "=";
        for (const auto& line : parts.lines)
        {
            if (line.starts_with(prefix))
                return shellUnquote(std::string_view(line).substr(prefix.size()), value);
        }
        return false;
#endif
    }

    bool SetUser(const std::string_view name, const std::string_view value) noexcept
    {
        if (name.empty())
            return false;
#if defined(_WIN32)
        const std::wstring wideName = widen(name);
        const std::wstring wideValue = widen(value);
        if (wideName.empty() || (!value.empty() && wideValue.empty()))
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_SET_VALUE, true))
            return false;
        const bool written = writeRegistryValue(key, wideName, wideValue);
        RegCloseKey(key);
        if (written)
            broadcastEnvironmentChange();
        return written;
#else
        if (!validVariableName(name))
            return false;
        ProfileParts parts = parseProfile(readFile(profilePath()));
        const std::string prefix = "export " + std::string(name) + "=";
        parts.lines.erase(
            std::remove_if(parts.lines.begin(), parts.lines.end(), [&](const std::string& line)
            {
                return line.starts_with(prefix);
            }),
            parts.lines.end());
        parts.lines.push_back(prefix + shellQuote(value));
        return writeProfile(std::move(parts));
#endif
    }

    bool RemoveUser(const std::string_view name) noexcept
    {
        if (name.empty())
            return false;
#if defined(_WIN32)
        const std::wstring wideName = widen(name);
        if (wideName.empty())
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_SET_VALUE, false))
            return true;
        const LONG status = RegDeleteValueW(key, wideName.c_str());
        RegCloseKey(key);
        const bool removed = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
        if (removed)
            broadcastEnvironmentChange();
        return removed;
#else
        if (!validVariableName(name))
            return false;
        ProfileParts parts = parseProfile(readFile(profilePath()));
        const std::string prefix = "export " + std::string(name) + "=";
        parts.lines.erase(
            std::remove_if(parts.lines.begin(), parts.lines.end(), [&](const std::string& line)
            {
                return line.starts_with(prefix);
            }),
            parts.lines.end());
        return writeProfile(std::move(parts));
#endif
    }

    bool UserPathContains(const std::string_view entry) noexcept
    {
        if (entry.empty())
            return false;
#if defined(_WIN32)
        const std::wstring candidate = normalizePath(widen(entry));
        HKEY key = nullptr;
        if (candidate.empty() || !openUserEnvironment(key, KEY_QUERY_VALUE, false))
            return false;
        std::wstring pathValue;
        const bool found = readRegistryValue(key, L"Path", pathValue);
        RegCloseKey(key);
        if (!found)
            return false;
        const auto entries = splitUserPath(pathValue);
        return std::any_of(entries.begin(), entries.end(), [&](const std::wstring& current)
        {
            return normalizePath(current) == candidate;
        });
#else
        const std::string candidate = normalizedPosixPath(std::string(entry));
        const ProfileParts parts = parseProfile(readFile(profilePath()));
        for (const auto& line : parts.lines)
        {
            constexpr std::string_view prefix = "export PATH=";
            constexpr std::string_view suffix = ":$PATH";
            if (!line.starts_with(prefix) || !line.ends_with(suffix))
                continue;
            const std::string_view encoded(line.data() + prefix.size(),
                line.size() - prefix.size() - suffix.size());
            std::string decoded;
            if (shellUnquote(encoded, decoded) && normalizedPosixPath(decoded) == candidate)
                return true;
        }
        return false;
#endif
    }

    bool AddUserPath(const std::string_view entry) noexcept
    {
        if (entry.empty() || UserPathContains(entry))
            return !entry.empty();
#if defined(_WIN32)
        const std::wstring wideEntry = widen(entry);
        if (wideEntry.empty())
            return false;
        HKEY key = nullptr;
        if (!openUserEnvironment(key, KEY_QUERY_VALUE | KEY_SET_VALUE, true))
            return false;
        std::wstring pathValue;
        readRegistryValue(key, L"Path", pathValue);
        const std::wstring updated = pathValue.empty() ? wideEntry : wideEntry + L";" + pathValue;
        const bool written = writeRegistryValue(key, L"Path", updated);
        RegCloseKey(key);
        if (written)
            broadcastEnvironmentChange();
        return written;
#else
        ProfileParts parts = parseProfile(readFile(profilePath()));
        parts.lines.push_back("export PATH=" + shellQuote(entry) + ":$PATH");
        return writeProfile(std::move(parts));
#endif
    }

    bool RemoveUserPath(const std::string_view entry) noexcept
    {
        if (entry.empty())
            return false;
#if defined(_WIN32)
        const std::wstring candidate = normalizePath(widen(entry));
        HKEY key = nullptr;
        if (candidate.empty() || !openUserEnvironment(key, KEY_QUERY_VALUE | KEY_SET_VALUE, false))
            return true;
        std::wstring pathValue;
        if (!readRegistryValue(key, L"Path", pathValue))
        {
            RegCloseKey(key);
            return true;
        }
        const auto entries = splitUserPath(pathValue);
        std::wstring updated;
        for (const auto& current : entries)
        {
            if (normalizePath(current) == candidate)
                continue;
            if (!updated.empty())
                updated.push_back(L';');
            updated += current;
        }
        bool written = false;
        if (updated.empty())
        {
            const LONG status = RegDeleteValueW(key, L"Path");
            written = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
        }
        else
        {
            written = writeRegistryValue(key, L"Path", updated);
        }
        RegCloseKey(key);
        if (written)
            broadcastEnvironmentChange();
        return written;
#else
        const std::string candidate = normalizedPosixPath(std::string(entry));
        ProfileParts parts = parseProfile(readFile(profilePath()));
        parts.lines.erase(
            std::remove_if(parts.lines.begin(), parts.lines.end(), [&](const std::string& line)
            {
                constexpr std::string_view prefix = "export PATH=";
                constexpr std::string_view suffix = ":$PATH";
                if (!line.starts_with(prefix) || !line.ends_with(suffix))
                    return false;
                const std::string_view encoded(line.data() + prefix.size(),
                    line.size() - prefix.size() - suffix.size());
                std::string decoded;
                return shellUnquote(encoded, decoded) &&
                    normalizedPosixPath(decoded) == candidate;
            }),
            parts.lines.end());
        return writeProfile(std::move(parts));
#endif
    }

    std::vector<std::string> DuplicateKeys()
    {
        std::vector<std::string> result;
#if defined(_WIN32)
        LPWCH environment = GetEnvironmentStringsW();
        if (environment == nullptr)
            return result;
        std::unordered_set<std::wstring> seen;
        std::unordered_set<std::wstring> duplicates;
        for (LPCWCH cursor = environment; *cursor != L'\0'; cursor += std::wcslen(cursor) + 1u)
        {
            const std::wstring_view entry(cursor);
            const std::size_t equals = entry.find(L'=');
            if (equals == std::wstring_view::npos || equals == 0u)
                continue;
            std::wstring key(entry.substr(0u, equals));
            std::transform(key.begin(), key.end(), key.begin(), [](const wchar_t ch)
            {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            if (!seen.insert(key).second)
                duplicates.insert(std::move(key));
        }
        FreeEnvironmentStringsW(environment);
        for (const auto& key : duplicates)
            result.push_back(narrow(key));
        std::sort(result.begin(), result.end());
#endif
        return result;
    }

    std::string TemporaryDirectory()
    {
        std::error_code ec;
        const auto path = std::filesystem::temp_directory_path(ec);
        return ec ? std::string{} : path.lexically_normal().generic_string();
    }

    std::string HomeDirectory()
    {
        std::string value;
#if defined(_WIN32)
        if (TryGet("USERPROFILE", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
        std::string drive;
        std::string homePath;
        if (TryGet("HOMEDRIVE", drive) && TryGet("HOMEPATH", homePath))
            return std::filesystem::path(drive + homePath).lexically_normal().generic_string();
#else
        if (TryGet("HOME", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
#endif
        return {};
    }

    std::string CacheDirectory()
    {
        std::string value;
#if defined(_WIN32)
        if (TryGet("LOCALAPPDATA", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
        const std::string home = HomeDirectory();
        if (!home.empty())
            return (std::filesystem::path(home) / "AppData" / "Local").lexically_normal().generic_string();
#else
        if (TryGet("XDG_CACHE_HOME", value))
            return std::filesystem::path(value).lexically_normal().generic_string();
        const std::string home = HomeDirectory();
        if (!home.empty())
            return (std::filesystem::path(home) / ".cache").lexically_normal().generic_string();
#endif
        return TemporaryDirectory();
    }
}
