#include "std_process.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

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
