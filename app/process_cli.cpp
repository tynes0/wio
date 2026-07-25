#include "process_cli.h"

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_set>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace wio::tooling::process
{
    namespace
    {
#if defined(_WIN32)
        class ScopedWindowsPathOverride
        {
        public:
            explicit ScopedWindowsPathOverride(const std::vector<std::filesystem::path>& extraPathEntries)
            {
                if (extraPathEntries.empty())
                    return;

                std::string existingPath;
                const DWORD requiredLength = GetEnvironmentVariableA("Path", nullptr, 0);
                if (requiredLength > 0)
                {
                    existingPath.assign(requiredLength, '\0');
                    const DWORD copiedLength = GetEnvironmentVariableA("Path", existingPath.data(), requiredLength);
                    if (copiedLength > 0 && copiedLength < requiredLength)
                        existingPath.resize(copiedLength);
                    else if (copiedLength == 0)
                        existingPath.clear();
                }

                originalPath_ = existingPath;
                hadOriginalPath_ = requiredLength > 0;

                std::string overriddenPath;
                for (const auto& pathEntry : extraPathEntries)
                {
                    if (pathEntry.empty())
                        continue;

                    if (!overriddenPath.empty())
                        overriddenPath.push_back(';');
                    overriddenPath += pathEntry.string();
                }

                if (!existingPath.empty())
                {
                    if (!overriddenPath.empty())
                        overriddenPath.push_back(';');
                    overriddenPath += existingPath;
                }

                if (!overriddenPath.empty())
                {
                    SetEnvironmentVariableA("Path", overriddenPath.c_str());
                    active_ = true;
                }
            }

            ScopedWindowsPathOverride(const ScopedWindowsPathOverride&) = delete;
            ScopedWindowsPathOverride& operator=(const ScopedWindowsPathOverride&) = delete;

            ~ScopedWindowsPathOverride()
            {
                if (!active_)
                    return;

                if (hadOriginalPath_)
                    SetEnvironmentVariableA("Path", originalPath_.c_str());
                else
                    SetEnvironmentVariableA("Path", nullptr);
            }

        private:
            bool active_ = false;
            bool hadOriginalPath_ = false;
            std::string originalPath_;
        };

        std::string quoteWindowsArgument(const std::string& value)
        {
            if (value.empty())
                return "\"\"";

            bool needsQuotes = false;
            for (const char ch : value)
            {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '"')
                {
                    needsQuotes = true;
                    break;
                }
            }

            if (!needsQuotes)
                return value;

            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('"');

            size_t backslashCount = 0;
            for (const char ch : value)
            {
                if (ch == '\\')
                {
                    ++backslashCount;
                    continue;
                }

                if (ch == '"')
                {
                    result.append((backslashCount * 2) + 1, '\\');
                    result.push_back('"');
                    backslashCount = 0;
                    continue;
                }

                result.append(backslashCount, '\\');
                backslashCount = 0;
                result.push_back(ch);
            }

            result.append(backslashCount * 2, '\\');
            result.push_back('"');
            return result;
        }
#else
        std::string quoteDisplayArgument(const std::string& value)
        {
            if (value.empty())
                return "''";

            bool needsQuotes = false;
            for (const char ch : value)
            {
                if (std::isspace(static_cast<unsigned char>(ch)) != 0 ||
                    ch == '\'' || ch == '"' || ch == '\\' || ch == '$' ||
                    ch == '&' || ch == ';' || ch == '|' || ch == '<' ||
                    ch == '>' || ch == '(' || ch == ')' || ch == '*')
                {
                    needsQuotes = true;
                    break;
                }
            }

            if (!needsQuotes)
                return value;

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
#endif
    }

    std::string FormatCommand(const std::vector<std::string>& parts)
    {
        std::ostringstream stream;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0)
                stream << ' ';
#if defined(_WIN32)
            stream << quoteWindowsArgument(parts[i]);
#else
            stream << quoteDisplayArgument(parts[i]);
#endif
        }
        return stream.str();
    }

    int Run(const std::vector<std::string>& parts,
            const std::optional<std::filesystem::path>& workingDirectory,
            const std::vector<std::filesystem::path>& extraPathEntries)
    {
        if (parts.empty() || parts.front().empty())
        {
            std::cerr << "Cannot launch an empty command.\n";
            return EXIT_FAILURE;
        }

#if defined(_WIN32)
        const std::string command = FormatCommand(parts);
        ScopedWindowsPathOverride scopedPathOverride(extraPathEntries);

        LPCH environmentStrings = GetEnvironmentStringsA();
        if (environmentStrings == nullptr)
        {
            std::cerr << "Failed to read the process environment.\n";
            return EXIT_FAILURE;
        }

        std::unordered_set<std::string> seenKeys;
        std::vector<std::string> sanitizedEntries;
        std::string mergedPath;
        std::optional<size_t> pathEntryIndex;

        for (LPCSTR cursor = environmentStrings; *cursor != '\0'; cursor += std::strlen(cursor) + 1)
        {
            const std::string_view entry(cursor);
            const size_t equalsIndex = entry.find('=');
            if (equalsIndex == std::string_view::npos || equalsIndex == 0)
            {
                sanitizedEntries.emplace_back(entry);
                continue;
            }

            std::string normalizedKey(entry.substr(0, equalsIndex));
            for (char& ch : normalizedKey)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            if (normalizedKey == "path")
            {
                if (!pathEntryIndex.has_value())
                    pathEntryIndex = sanitizedEntries.size();
                if (!mergedPath.empty() && mergedPath.back() != ';' &&
                    equalsIndex + 1 < entry.size() && entry[equalsIndex + 1] != ';')
                {
                    mergedPath.push_back(';');
                }
                mergedPath.append(entry.substr(equalsIndex + 1));
                continue;
            }

            if (!seenKeys.insert(normalizedKey).second)
                continue;

            sanitizedEntries.emplace_back(entry);
        }

        if (!mergedPath.empty() && pathEntryIndex.has_value())
        {
            sanitizedEntries.insert(
                sanitizedEntries.begin() + static_cast<std::ptrdiff_t>(*pathEntryIndex),
                "Path=" + mergedPath
            );
        }

        FreeEnvironmentStringsA(environmentStrings);

        std::vector<char> environmentBlock;
        for (const std::string& entry : sanitizedEntries)
        {
            environmentBlock.insert(environmentBlock.end(), entry.begin(), entry.end());
            environmentBlock.push_back('\0');
        }
        environmentBlock.push_back('\0');

        std::vector<char> commandLine(command.begin(), command.end());
        commandLine.push_back('\0');
        const std::string workingDirectoryText =
            workingDirectory.has_value() ? workingDirectory->string() : std::string{};

        STARTUPINFOA startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};
        const BOOL created = CreateProcessA(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            environmentBlock.data(),
            workingDirectory.has_value() ? workingDirectoryText.c_str() : nullptr,
            &startupInfo,
            &processInfo
        );

        if (created == FALSE)
        {
            std::cerr << "Failed to launch '" << parts.front()
                      << "' (Windows error " << GetLastError() << ").\n";
            return EXIT_FAILURE;
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);

        DWORD exitCode = EXIT_FAILURE;
        if (GetExitCodeProcess(processInfo.hProcess, &exitCode) == FALSE)
            std::cerr << "Failed to read child process exit code.\n";

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return static_cast<int>(exitCode);
#else
        const pid_t processId = fork();
        if (processId < 0)
        {
            std::cerr << "Failed to start process: " << std::strerror(errno) << '\n';
            return EXIT_FAILURE;
        }

        if (processId == 0)
        {
            if (workingDirectory.has_value() && chdir(workingDirectory->c_str()) != 0)
            {
                std::fprintf(stderr, "Failed to enter working directory '%s': %s\n",
                             workingDirectory->string().c_str(), std::strerror(errno));
                _exit(126);
            }

            if (!extraPathEntries.empty())
            {
                std::string pathValue;
                for (const auto& entry : extraPathEntries)
                {
                    if (entry.empty())
                        continue;
                    if (!pathValue.empty())
                        pathValue.push_back(':');
                    pathValue += entry.string();
                }

                if (const char* existingPath = std::getenv("PATH");
                    existingPath != nullptr && *existingPath != '\0')
                {
                    if (!pathValue.empty())
                        pathValue.push_back(':');
                    pathValue += existingPath;
                }

                if (!pathValue.empty())
                    setenv("PATH", pathValue.c_str(), 1);
            }

            std::vector<char*> argvView;
            argvView.reserve(parts.size() + 1);
            for (const std::string& part : parts)
                argvView.push_back(const_cast<char*>(part.c_str()));
            argvView.push_back(nullptr);

            execvp(argvView.front(), argvView.data());
            std::fprintf(stderr, "Failed to launch '%s': %s\n",
                         parts.front().c_str(), std::strerror(errno));
            _exit(errno == ENOENT ? 127 : 126);
        }

        int status = 0;
        while (waitpid(processId, &status, 0) < 0)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "Failed while waiting for process: " << std::strerror(errno) << '\n';
            return EXIT_FAILURE;
        }

        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        if (WIFSIGNALED(status))
            return 128 + WTERMSIG(status);
        return EXIT_FAILURE;
#endif
    }
}
