#include "std_process.h"

#include <bit>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#if !defined(_WIN32)
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
