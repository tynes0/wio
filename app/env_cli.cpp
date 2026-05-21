#include "env_cli.h"

#include <argonaut.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace wio::tooling::env
{
    namespace
    {
        constexpr std::string_view kProfileMarkerBegin = "# >>> wio env >>>";
        constexpr std::string_view kProfileMarkerEnd = "# <<< wio env <<<";

        struct EnvStatusSnapshot
        {
            std::optional<std::filesystem::path> toolchainRoot;
            std::optional<std::filesystem::path> binDirectory;
            std::string processWioRoot;
            std::string processWioHome;
            std::string processPath;
            bool processPathContainsBin = false;
            bool processHasWioRoot = false;
            bool processHasWioHome = false;
            bool processHasPath = false;
            bool persistentHasWioRoot = false;
            bool persistentHasWioHome = false;
            bool persistentPathContainsBin = false;
            std::string persistentWioRoot;
            std::string persistentWioHome;
            std::string persistentPath;
            bool profileMarkerPresent = false;
            std::vector<std::string> caseInsensitiveDuplicateEnvKeys;
        };

        struct BackendSmokeResult
        {
            bool attempted = false;
            bool success = false;
            int exitCode = EXIT_FAILURE;
            std::filesystem::path sourcePath;
            std::filesystem::path logPath;
            std::string output;
        };

        std::string trim(const std::string& value);
        std::string lowercase(std::string value);

        const char* tryGetEnv(const char* name)
        {
            const char* value = std::getenv(name);
            return (value != nullptr && *value != '\0') ? value : nullptr;
        }

        bool containsPathEntry(const std::string& pathList, const std::string& candidate, const char separator, const bool caseInsensitive)
        {
            std::stringstream stream(pathList);
            std::string entry;

            auto normalize = [caseInsensitive](std::string value)
            {
                while (!value.empty() && (value.back() == '\\' || value.back() == '/'))
                    value.pop_back();
                value = trim(value);
                if (caseInsensitive)
                    value = lowercase(value);
                return value;
            };

            const std::string normalizedCandidate = normalize(candidate);
            while (std::getline(stream, entry, separator))
            {
                if (normalize(entry) == normalizedCandidate)
                    return true;
            }

            return false;
        }

        std::vector<std::string> collectCommandArgs(const std::string& programName, int argc, char* argv[], int firstArgumentIndex)
        {
            std::vector<std::string> args;
            args.reserve(static_cast<size_t>(argc - firstArgumentIndex + 1));
            args.push_back(programName);

            for (int i = firstArgumentIndex; i < argc; ++i)
            {
                if (argv[i] != nullptr)
                    args.emplace_back(argv[i]);
            }

            return args;
        }

        std::vector<char*> buildArgvView(std::vector<std::string>& args)
        {
            std::vector<char*> argvView;
            argvView.reserve(args.size());
            for (std::string& arg : args)
                argvView.push_back(arg.data());
            return argvView;
        }

        std::optional<int> parseWithHandling(Argonaut::Parser& parser, std::vector<std::string>& args, const char* contextLabel)
        {
            std::vector<char*> argvView = buildArgvView(args);

            try
            {
                parser.Parse(static_cast<int>(argvView.size()), argvView.data());
                return std::nullopt;
            }
            catch (const Argonaut::HelpRequestedException& e)
            {
                std::cout << e.what();
                return EXIT_SUCCESS;
            }
            catch (const Argonaut::VersionRequestedException& e)
            {
                std::cout << e.what();
                return EXIT_SUCCESS;
            }
            catch (const Argonaut::ParsePrepException& e)
            {
                std::cerr << contextLabel << " CLI setup failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const Argonaut::ParseException& e)
            {
                std::cerr << contextLabel << " argument parsing failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Unhandled " << contextLabel << " CLI error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        bool getFlagValue(Argonaut::Parser& parser, const std::string& id)
        {
            auto values = parser.GetValuesOf<bool>(id);
            return !values.empty() && values.front();
        }

        std::string trim(const std::string& value)
        {
            size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
                ++start;

            size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
                --end;

            return value.substr(start, end - start);
        }

        std::string lowercase(std::string value)
        {
            for (char& ch : value)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return value;
        }

        bool promptYesNo(const std::string& prompt)
        {
            std::cout << prompt << " [y/N] ";
            std::string response;
            std::getline(std::cin, response);
            response = lowercase(trim(response));
            return response == "y" || response == "yes";
        }

        std::string shellEscapeSingleQuoted(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 8);
            for (const char ch : value)
            {
                if (ch == '\'')
                    escaped += "'\"'\"'";
                else
                    escaped.push_back(ch);
            }
            return escaped;
        }

        std::string powershellEscapeSingleQuoted(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 4);
            for (const char ch : value)
            {
                if (ch == '\'')
                    escaped += "''";
                else
                    escaped.push_back(ch);
            }
            return escaped;
        }

        std::string makeShellAssignment(const std::string& variable, const std::string& value)
        {
            return "export " + variable + "='" + shellEscapeSingleQuoted(value) + "'";
        }

        std::string makePowerShellAssignment(const std::string& variable, const std::string& value)
        {
            return "$env:" + variable + " = '" + powershellEscapeSingleQuoted(value) + "'";
        }

        std::string makeCmdAssignment(const std::string& variable, const std::string& value)
        {
            return "set \"" + variable + "=" + value + "\"";
        }

        std::filesystem::path tryGetExecutablePath()
        {
#if defined(_WIN32)
            std::wstring buffer(MAX_PATH, L'\0');
            const DWORD copiedLength = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (copiedLength == 0)
                return {};

            buffer.resize(copiedLength);
            return std::filesystem::path(buffer).make_preferred();
#else
            std::vector<char> buffer(4096, '\0');
            const ssize_t copiedLength = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
            if (copiedLength <= 0)
                return {};

            buffer[static_cast<size_t>(copiedLength)] = '\0';
            return std::filesystem::path(buffer.data()).make_preferred();
#endif
        }

        std::filesystem::path getUserCacheRoot()
        {
#if defined(_WIN32)
            if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData != nullptr && *localAppData != '\0')
                return std::filesystem::path(localAppData).make_preferred();

            if (const char* userProfile = std::getenv("USERPROFILE"); userProfile != nullptr && *userProfile != '\0')
                return (std::filesystem::path(userProfile) / "AppData" / "Local").make_preferred();
#else
            if (const char* xdgCacheHome = std::getenv("XDG_CACHE_HOME"); xdgCacheHome != nullptr && *xdgCacheHome != '\0')
                return std::filesystem::path(xdgCacheHome).make_preferred();

            if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
                return (std::filesystem::path(home) / ".cache").make_preferred();
#endif

            std::error_code ec;
            std::filesystem::path tempRoot = std::filesystem::temp_directory_path(ec);
            if (!ec && !tempRoot.empty())
                return tempRoot.make_preferred();

            return std::filesystem::absolute(std::filesystem::path(".")).make_preferred();
        }

        bool isToolchainRoot(const std::filesystem::path& candidate)
        {
            std::error_code ec;
            return std::filesystem::exists(candidate / "std", ec) &&
                   std::filesystem::exists(candidate / "runtime", ec) &&
                   std::filesystem::exists(candidate / "sdk", ec) &&
                   (std::filesystem::exists(candidate / "bin", ec) ||
                    std::filesystem::exists(candidate / "compiler", ec) ||
                    std::filesystem::exists(candidate / "app", ec));
        }

        std::optional<std::filesystem::path> tryFindToolchainRoot(const std::optional<std::filesystem::path>& explicitRoot)
        {
            std::error_code ec;
            if (explicitRoot.has_value())
            {
                const std::filesystem::path resolved = std::filesystem::absolute(*explicitRoot, ec).make_preferred();
                if (ec)
                    throw std::runtime_error("Could not resolve the provided WIO root path.");
                if (!isToolchainRoot(resolved))
                    throw std::runtime_error("The provided WIO root does not look like a Wio toolchain root: " + resolved.string());
                return resolved;
            }

            std::vector<std::filesystem::path> seeds;

            const char* wioRootEnv = std::getenv("WIO_ROOT");
            if (wioRootEnv != nullptr && *wioRootEnv != '\0')
                seeds.emplace_back(std::filesystem::path(wioRootEnv));

            const char* wioHomeEnv = std::getenv("WIO_HOME");
            if (wioHomeEnv != nullptr && *wioHomeEnv != '\0')
                seeds.emplace_back(std::filesystem::path(wioHomeEnv));

            const std::filesystem::path executablePath = tryGetExecutablePath();
            if (!executablePath.empty())
            {
                seeds.push_back(executablePath.parent_path());
                seeds.push_back(executablePath.parent_path().parent_path());
            }

            std::filesystem::path currentPath = std::filesystem::current_path(ec);
            if (!ec)
                seeds.push_back(currentPath);

            for (auto seed : seeds)
            {
                if (seed.empty())
                    continue;

                seed = std::filesystem::absolute(seed, ec).make_preferred();
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                while (!seed.empty())
                {
                    if (isToolchainRoot(seed))
                        return seed;

                    const auto parent = seed.parent_path();
                    if (parent == seed)
                        break;
                    seed = parent;
                }
            }

            return std::nullopt;
        }

        std::filesystem::path resolveBinDirectory(const std::filesystem::path& toolchainRoot)
        {
            std::error_code ec;
            std::filesystem::path packagedBin = toolchainRoot / "bin";
            if (std::filesystem::exists(packagedBin, ec))
                return packagedBin.make_preferred();

            std::filesystem::path executablePath = tryGetExecutablePath();
            if (!executablePath.empty())
                return executablePath.parent_path().make_preferred();

            return packagedBin.make_preferred();
        }

        std::string quoteShellArgument(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for (const char ch : value)
            {
                if (ch == '"')
                    escaped += "\\\"";
                else
                    escaped.push_back(ch);
            }
            escaped.push_back('"');
            return escaped;
        }

        std::string readUtf8FileIfPresent(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream.is_open())
                return {};

            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        struct CapturedCommandResult
        {
            int exitCode = EXIT_FAILURE;
            std::string output;
        };

        CapturedCommandResult captureCommandOutput(const std::string& command)
        {
            CapturedCommandResult result;

#if defined(_WIN32)
            SECURITY_ATTRIBUTES securityAttributes{};
            securityAttributes.nLength = sizeof(securityAttributes);
            securityAttributes.bInheritHandle = TRUE;

            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (CreatePipe(&readPipe, &writePipe, &securityAttributes, 0) == FALSE)
            {
                result.output = "Failed to create doctor output pipe.";
                return result;
            }

            SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA startupInfo{};
            startupInfo.cb = sizeof(startupInfo);
            startupInfo.dwFlags = STARTF_USESTDHANDLES;
            startupInfo.hStdOutput = writePipe;
            startupInfo.hStdError = writePipe;
            startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            PROCESS_INFORMATION processInfo{};
            std::vector<char> commandLine(command.begin(), command.end());
            commandLine.push_back('\0');

            const BOOL created = CreateProcessA(
                nullptr,
                commandLine.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startupInfo,
                &processInfo
            );

            CloseHandle(writePipe);

            if (created == FALSE)
            {
                CloseHandle(readPipe);
                result.output = "Failed to start backend smoke command.";
                return result;
            }

            std::array<char, 4096> buffer{};
            DWORD bytesRead = 0;
            while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) == TRUE && bytesRead > 0)
                result.output.append(buffer.data(), bytesRead);

            WaitForSingleObject(processInfo.hProcess, INFINITE);
            DWORD exitCode = EXIT_FAILURE;
            GetExitCodeProcess(processInfo.hProcess, &exitCode);
            result.exitCode = static_cast<int>(exitCode);

            CloseHandle(readPipe);
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
#else
            const std::string redirectedCommand = command + " 2>&1";
            FILE* pipe = popen(redirectedCommand.c_str(), "r");
            if (pipe == nullptr)
            {
                result.output = "Failed to start backend smoke command.";
                return result;
            }

            std::array<char, 4096> buffer{};
            while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
                result.output += buffer.data();

            const int closeResult = pclose(pipe);
            result.exitCode = closeResult;
            if (closeResult != -1 && WIFEXITED(closeResult))
                result.exitCode = WEXITSTATUS(closeResult);
#endif

            return result;
        }

        BackendSmokeResult runBackendSmoke()
        {
            BackendSmokeResult result;
            result.attempted = true;

            const std::filesystem::path executablePath = tryGetExecutablePath();
            if (executablePath.empty())
            {
                result.output = "Could not resolve the current wio executable path.";
                return result;
            }

            const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            std::error_code ec;
            std::filesystem::path doctorRoot;
            for (const std::filesystem::path candidate : {
                     (getUserCacheRoot() / "Wio" / "doctor").make_preferred(),
                     (std::filesystem::temp_directory_path(ec) / "Wio" / "doctor").make_preferred(),
                     std::filesystem::absolute(std::filesystem::path(".wio-doctor")).make_preferred()
                 })
            {
                ec.clear();
                std::filesystem::create_directories(candidate, ec);
                if (!ec)
                {
                    doctorRoot = candidate;
                    break;
                }
            }

            if (doctorRoot.empty())
            {
                result.output = "Could not create a writable Wio doctor working directory.";
                return result;
            }

            result.sourcePath = (doctorRoot / ("backend-smoke-" + std::to_string(timestamp) + ".wio")).make_preferred();
            result.logPath = (doctorRoot / ("backend-smoke-" + std::to_string(timestamp) + ".log")).make_preferred();

            {
                std::ofstream sourceStream(result.sourcePath, std::ios::binary | std::ios::trunc);
                if (!sourceStream.is_open())
                {
                    result.output = "Could not create the Wio backend smoke source file.";
                    return result;
                }

                sourceStream
                    << "use std::console;\n\n"
                    << "fn Entry() -> void\n"
                    << "{\n"
                    << "    std::console::PrintLine(\"Wio backend smoke ok\");\n"
                    << "}\n";
            }

            const std::string command =
                quoteShellArgument(executablePath.string()) +
                " file run " +
                quoteShellArgument(result.sourcePath.string()) +
                " --show-backend-info";

            const CapturedCommandResult commandResult = captureCommandOutput(command);
            result.exitCode = commandResult.exitCode;
            result.output = commandResult.output;
            {
                std::ofstream logStream(result.logPath, std::ios::binary | std::ios::trunc);
                if (logStream.is_open())
                    logStream.write(result.output.data(), static_cast<std::streamsize>(result.output.size()));
            }

            if (commandResult.exitCode == 0 && result.output.find("Wio backend smoke ok") != std::string::npos)
            {
                result.success = true;
                return result;
            }

            if (result.output.empty())
                result.output = "The backend smoke command did not produce output.";

            return result;
        }

        std::string renderShellEnvironment(const std::filesystem::path& toolchainRoot, const std::filesystem::path& binDirectory, const std::string& shell, bool addPath)
        {
            const std::string root = toolchainRoot.string();
            const std::string bin = binDirectory.string();

            std::ostringstream stream;
            const std::string shellId = lowercase(shell);
            if (shellId == "powershell" || shellId == "pwsh" || shellId == "ps")
            {
                stream << makePowerShellAssignment("WIO_ROOT", root) << '\n';
                stream << makePowerShellAssignment("WIO_HOME", root);
                if (addPath)
                    stream << '\n' << "$env:Path = '" << powershellEscapeSingleQuoted(bin) << ";' + $env:Path";
                return stream.str();
            }

            if (shellId == "cmd")
            {
                stream << makeCmdAssignment("WIO_ROOT", root) << '\n';
                stream << makeCmdAssignment("WIO_HOME", root);
                if (addPath)
                    stream << '\n' << "set \"PATH=" << bin << ";%PATH%\"";
                return stream.str();
            }

            stream << makeShellAssignment("WIO_ROOT", root) << '\n';
            stream << makeShellAssignment("WIO_HOME", root);
            if (addPath)
                stream << '\n' << "export PATH='" << shellEscapeSingleQuoted(bin) << ":$PATH'";
            return stream.str();
        }

        std::string renderShellEnvironmentRemoval(const std::filesystem::path& binDirectory, const std::string& shell, bool removePath)
        {
            const std::string bin = binDirectory.string();
            std::ostringstream stream;
            const std::string shellId = lowercase(shell);

            if (shellId == "powershell" || shellId == "pwsh" || shellId == "ps")
            {
                stream << "Remove-Item Env:WIO_ROOT -ErrorAction SilentlyContinue" << '\n';
                stream << "Remove-Item Env:WIO_HOME -ErrorAction SilentlyContinue";
                if (removePath)
                {
                    stream << '\n'
                           << "$wioBin = '" << powershellEscapeSingleQuoted(bin) << "'\n"
                           << "$env:Path = [string]::Join(';', (($env:Path -split ';') | Where-Object { $_ -and ([string]::Compare($_.TrimEnd('\\', '/'), $wioBin.TrimEnd('\\', '/'), $true) -ne 0) }))";
                }
                return stream.str();
            }

            if (shellId == "cmd")
            {
                stream << "set \"WIO_ROOT=\"" << '\n';
                stream << "set \"WIO_HOME=\"";
                if (removePath)
                    stream << '\n' << "REM Remove '" << bin << "' from PATH manually in this cmd.exe session.";
                return stream.str();
            }

            stream << "unset WIO_ROOT" << '\n';
            stream << "unset WIO_HOME";
            if (removePath)
                stream << '\n' << "export PATH=$(printf '%s' \"$PATH\" | awk -v RS=: -v ORS=: '$0 != \"" << shellEscapeSingleQuoted(bin) << "\"' | sed 's/:$//')";
            return stream.str();
        }

#if defined(_WIN32)
        std::wstring normalizeWindowsPath(const std::wstring& value)
        {
            std::wstring normalized = value;
            std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
            while (!normalized.empty() && (normalized.back() == L'\\' || normalized.back() == L'/'))
                normalized.pop_back();

            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t ch)
            {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            return normalized;
        }

        std::wstring trimWindowsString(const std::wstring& value)
        {
            size_t start = 0;
            while (start < value.size() && std::iswspace(value[start]) != 0)
                ++start;

            size_t end = value.size();
            while (end > start && std::iswspace(value[end - 1]) != 0)
                --end;

            return value.substr(start, end - start);
        }

        std::string narrowFromWide(const std::wstring& value)
        {
            std::string result;
            result.reserve(value.size());
            for (const wchar_t ch : value)
                result.push_back(static_cast<char>(ch));
            return result;
        }

        std::wstring readRegistryString(HKEY key, const wchar_t* valueName)
        {
            DWORD type = 0;
            DWORD size = 0;
            const LONG queryStatus = RegGetValueW(key, nullptr, valueName, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, nullptr, &size);
            if (queryStatus != ERROR_SUCCESS || size == 0)
                return {};

            std::wstring buffer(size / sizeof(wchar_t), L'\0');
            if (RegGetValueW(key, nullptr, valueName, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, buffer.data(), &size) != ERROR_SUCCESS)
                return {};

            while (!buffer.empty() && buffer.back() == L'\0')
                buffer.pop_back();
            return buffer;
        }

        void deleteRegistryValueIfPresent(HKEY key, const wchar_t* valueName)
        {
            const LONG status = RegDeleteValueW(key, valueName);
            if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
                throw std::runtime_error("Could not update the Windows user environment registry.");
        }

        void writeRegistryExpandString(HKEY key, const wchar_t* valueName, const std::wstring& value)
        {
            const DWORD size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
            const LONG status = RegSetValueExW(key, valueName, 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(value.c_str()), size);
            if (status != ERROR_SUCCESS)
                throw std::runtime_error("Could not write the Windows user environment registry.");
        }

        bool containsPathEntry(const std::wstring& pathList, const std::wstring& candidate)
        {
            std::wstringstream stream(pathList);
            std::wstring entry;
            const std::wstring normalizedCandidate = normalizeWindowsPath(candidate);

            while (std::getline(stream, entry, L';'))
            {
                if (normalizeWindowsPath(trimWindowsString(entry)) == normalizedCandidate)
                    return true;
            }

            return false;
        }

        std::wstring removePathEntry(const std::wstring& pathList, const std::wstring& candidate)
        {
            std::wstringstream stream(pathList);
            std::wstring entry;
            std::vector<std::wstring> keptEntries;
            const std::wstring normalizedCandidate = normalizeWindowsPath(candidate);

            while (std::getline(stream, entry, L';'))
            {
                const std::wstring trimmedEntry = trimWindowsString(entry);
                if (trimmedEntry.empty())
                    continue;

                if (normalizeWindowsPath(trimmedEntry) == normalizedCandidate)
                    continue;

                keptEntries.push_back(trimmedEntry);
            }

            std::wostringstream result;
            for (size_t i = 0; i < keptEntries.size(); ++i)
            {
                if (i > 0)
                    result << L';';
                result << keptEntries[i];
            }

            return result.str();
        }

        std::vector<std::string> collectDuplicateEnvironmentKeysWindows()
        {
            std::vector<std::string> duplicates;
            LPWCH environmentStrings = GetEnvironmentStringsW();
            if (environmentStrings == nullptr)
                return duplicates;

            std::unordered_set<std::string> seenKeys;
            std::unordered_set<std::string> duplicateKeys;

            for (LPCWSTR cursor = environmentStrings; *cursor != L'\0'; cursor += std::wcslen(cursor) + 1)
            {
                std::wstring_view entry(cursor);
                const size_t equalsIndex = entry.find(L'=');
                if (equalsIndex == std::wstring_view::npos || equalsIndex == 0)
                    continue;

                std::wstring key(entry.substr(0, equalsIndex));
                std::transform(key.begin(), key.end(), key.begin(), [](wchar_t ch)
                {
                    return static_cast<wchar_t>(std::towlower(ch));
                });

                std::string narrowKey;
                narrowKey.reserve(key.size());
                for (const wchar_t ch : key)
                    narrowKey.push_back(static_cast<char>(ch));

                if (!seenKeys.insert(narrowKey).second)
                    duplicateKeys.insert(narrowKey);
            }

            FreeEnvironmentStringsW(environmentStrings);

            duplicates.assign(duplicateKeys.begin(), duplicateKeys.end());
            std::sort(duplicates.begin(), duplicates.end());
            return duplicates;
        }

        void persistUserEnvironmentWindows(const std::filesystem::path& toolchainRoot, const std::filesystem::path& binDirectory, bool addPath)
        {
            HKEY key = nullptr;
            const LONG openStatus = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
            if (openStatus != ERROR_SUCCESS)
                throw std::runtime_error("Could not open the current user Environment registry key.");

            const std::wstring root = toolchainRoot.wstring();
            const std::wstring bin = binDirectory.wstring();
            writeRegistryExpandString(key, L"WIO_ROOT", root);
            writeRegistryExpandString(key, L"WIO_HOME", root);

            if (addPath)
            {
                std::wstring existingPath = readRegistryString(key, L"Path");
                if (!containsPathEntry(existingPath, bin))
                {
                    if (!existingPath.empty() && existingPath.front() == L';')
                        existingPath.erase(existingPath.begin());
                    if (!existingPath.empty())
                        existingPath = bin + L";" + existingPath;
                    else
                        existingPath = bin;
                    writeRegistryExpandString(key, L"Path", existingPath);
                }
            }

            RegCloseKey(key);

            SetEnvironmentVariableW(L"WIO_ROOT", root.c_str());
            SetEnvironmentVariableW(L"WIO_HOME", root.c_str());
            if (addPath)
            {
                std::wstring processPath;
                processPath.resize(32767, L'\0');
                const DWORD copied = GetEnvironmentVariableW(L"Path", processPath.data(), static_cast<DWORD>(processPath.size()));
                if (copied > 0 && copied < processPath.size())
                    processPath.resize(copied);
                else
                    processPath.clear();

                if (!containsPathEntry(processPath, bin))
                {
                    if (!processPath.empty())
                        processPath = bin + L";" + processPath;
                    else
                        processPath = bin;
                    SetEnvironmentVariableW(L"Path", processPath.c_str());
                }
            }

            SendMessageTimeoutW(HWND_BROADCAST,
                                WM_SETTINGCHANGE,
                                0,
                                reinterpret_cast<LPARAM>(L"Environment"),
                                SMTO_ABORTIFHUNG,
                                5000,
                                nullptr);
        }

        void removeUserEnvironmentWindows(const std::filesystem::path& binDirectory, bool removePath)
        {
            HKEY key = nullptr;
            const LONG openStatus = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
            if (openStatus != ERROR_SUCCESS)
                throw std::runtime_error("Could not open the current user Environment registry key.");

            deleteRegistryValueIfPresent(key, L"WIO_ROOT");
            deleteRegistryValueIfPresent(key, L"WIO_HOME");

            if (removePath)
            {
                const std::wstring existingPath = readRegistryString(key, L"Path");
                const std::wstring updatedPath = removePathEntry(existingPath, binDirectory.wstring());
                if (updatedPath.empty())
                    deleteRegistryValueIfPresent(key, L"Path");
                else if (updatedPath != existingPath)
                    writeRegistryExpandString(key, L"Path", updatedPath);
            }

            RegCloseKey(key);

            SetEnvironmentVariableW(L"WIO_ROOT", nullptr);
            SetEnvironmentVariableW(L"WIO_HOME", nullptr);
            if (removePath)
            {
                std::wstring processPath;
                processPath.resize(32767, L'\0');
                const DWORD copied = GetEnvironmentVariableW(L"Path", processPath.data(), static_cast<DWORD>(processPath.size()));
                if (copied > 0 && copied < processPath.size())
                    processPath.resize(copied);
                else
                    processPath.clear();

                const std::wstring updatedProcessPath = removePathEntry(processPath, binDirectory.wstring());
                SetEnvironmentVariableW(L"Path", updatedProcessPath.empty() ? nullptr : updatedProcessPath.c_str());
            }

            SendMessageTimeoutW(HWND_BROADCAST,
                                WM_SETTINGCHANGE,
                                0,
                                reinterpret_cast<LPARAM>(L"Environment"),
                                SMTO_ABORTIFHUNG,
                                5000,
                                nullptr);
        }
#else
        void replaceProfileBlock(std::string& profileContent, const std::string& block)
        {
            const size_t markerBegin = profileContent.find(kProfileMarkerBegin);
            if (markerBegin != std::string::npos)
            {
                size_t markerEnd = profileContent.find(kProfileMarkerEnd, markerBegin);
                if (markerEnd != std::string::npos)
                {
                    markerEnd += kProfileMarkerEnd.size();
                    if (markerEnd < profileContent.size() && profileContent[markerEnd] == '\n')
                        ++markerEnd;
                    profileContent.erase(markerBegin, markerEnd - markerBegin);
                }
                else
                {
                    profileContent.erase(markerBegin);
                }
            }

            if (!profileContent.empty() && profileContent.back() != '\n')
                profileContent.push_back('\n');

            profileContent += block;
            if (!profileContent.empty() && profileContent.back() != '\n')
                profileContent.push_back('\n');
        }

        void persistUserEnvironmentPosix(const std::filesystem::path& toolchainRoot, const std::filesystem::path& binDirectory, bool addPath)
        {
            const char* homeValue = std::getenv("HOME");
            if (homeValue == nullptr || *homeValue == '\0')
                throw std::runtime_error("Could not resolve the HOME directory for persistent environment setup.");

            const std::filesystem::path profilePath = std::filesystem::path(homeValue) / ".profile";
            std::string profileContent;

            {
                std::ifstream input(profilePath, std::ios::binary);
                if (input.is_open())
                {
                    std::ostringstream buffer;
                    buffer << input.rdbuf();
                    profileContent = buffer.str();
                }
            }

            std::ostringstream block;
            block << kProfileMarkerBegin << '\n'
                  << makeShellAssignment("WIO_ROOT", toolchainRoot.string()) << '\n'
                  << makeShellAssignment("WIO_HOME", toolchainRoot.string()) << '\n';
            if (addPath)
                block << "export PATH='" << shellEscapeSingleQuoted(binDirectory.string()) << ":$PATH'" << '\n';
            block << kProfileMarkerEnd << '\n';

            replaceProfileBlock(profileContent, block.str());

            std::ofstream output(profilePath, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                throw std::runtime_error("Could not write the shell profile at '" + profilePath.string() + "'.");

            output.write(profileContent.data(), static_cast<std::streamsize>(profileContent.size()));
            if (!output.good())
                throw std::runtime_error("Could not update the shell profile at '" + profilePath.string() + "'.");
        }

        void removeUserEnvironmentPosix(const std::filesystem::path& binDirectory, bool removePath)
        {
            const char* homeValue = std::getenv("HOME");
            if (homeValue == nullptr || *homeValue == '\0')
                throw std::runtime_error("Could not resolve the HOME directory for persistent environment cleanup.");

            const std::filesystem::path profilePath = std::filesystem::path(homeValue) / ".profile";
            std::string profileContent;

            {
                std::ifstream input(profilePath, std::ios::binary);
                if (input.is_open())
                {
                    std::ostringstream buffer;
                    buffer << input.rdbuf();
                    profileContent = buffer.str();
                }
            }

            std::ostringstream block;
            if (!removePath)
            {
                block << kProfileMarkerBegin << '\n'
                      << "export PATH='" << shellEscapeSingleQuoted(binDirectory.string()) << ":$PATH'" << '\n'
                      << kProfileMarkerEnd << '\n';
                replaceProfileBlock(profileContent, block.str());
            }
            else
            {
                replaceProfileBlock(profileContent, "");
            }

            std::ofstream output(profilePath, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                throw std::runtime_error("Could not write the shell profile at '" + profilePath.string() + "'.");

            output.write(profileContent.data(), static_cast<std::streamsize>(profileContent.size()));
            if (!output.good())
                throw std::runtime_error("Could not update the shell profile at '" + profilePath.string() + "'.");
        }
#endif

        std::string determineDefaultShell()
        {
#if defined(_WIN32)
            return "powershell";
#else
            return "sh";
#endif
        }

        EnvStatusSnapshot inspectEnvironment(const std::optional<std::filesystem::path>& explicitRoot)
        {
            EnvStatusSnapshot snapshot;
            snapshot.toolchainRoot = tryFindToolchainRoot(explicitRoot);
            if (snapshot.toolchainRoot.has_value())
                snapshot.binDirectory = resolveBinDirectory(*snapshot.toolchainRoot);

            if (const char* value = tryGetEnv("WIO_ROOT"))
            {
                snapshot.processWioRoot = value;
                snapshot.processHasWioRoot = true;
            }

            if (const char* value = tryGetEnv("WIO_HOME"))
            {
                snapshot.processWioHome = value;
                snapshot.processHasWioHome = true;
            }

#if defined(_WIN32)
            {
                std::wstring processPath;
                processPath.resize(32767, L'\0');
                const DWORD copied = GetEnvironmentVariableW(L"Path", processPath.data(), static_cast<DWORD>(processPath.size()));
                if (copied > 0 && copied < processPath.size())
                {
                    processPath.resize(copied);
                    snapshot.processPath = narrowFromWide(processPath);
                    snapshot.processHasPath = true;
                }
            }

            snapshot.caseInsensitiveDuplicateEnvKeys = collectDuplicateEnvironmentKeysWindows();

            HKEY key = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
            {
                const std::wstring userRoot = readRegistryString(key, L"WIO_ROOT");
                const std::wstring userHome = readRegistryString(key, L"WIO_HOME");
                const std::wstring userPath = readRegistryString(key, L"Path");
                RegCloseKey(key);

                snapshot.persistentHasWioRoot = !userRoot.empty();
                snapshot.persistentHasWioHome = !userHome.empty();
                snapshot.persistentWioRoot = narrowFromWide(userRoot);
                snapshot.persistentWioHome = narrowFromWide(userHome);
                snapshot.persistentPath = narrowFromWide(userPath);
            }
#else
            if (const char* value = tryGetEnv("PATH"))
            {
                snapshot.processPath = value;
                snapshot.processHasPath = true;
            }

            const char* homeValue = std::getenv("HOME");
            if (homeValue != nullptr && *homeValue != '\0')
            {
                const std::filesystem::path profilePath = std::filesystem::path(homeValue) / ".profile";
                std::ifstream input(profilePath, std::ios::binary);
                if (input.is_open())
                {
                    std::ostringstream buffer;
                    buffer << input.rdbuf();
                    snapshot.persistentPath = buffer.str();
                    snapshot.profileMarkerPresent = snapshot.persistentPath.find(kProfileMarkerBegin) != std::string::npos;
                    snapshot.persistentHasWioRoot = snapshot.persistentPath.find("WIO_ROOT") != std::string::npos;
                    snapshot.persistentHasWioHome = snapshot.persistentPath.find("WIO_HOME") != std::string::npos;
                }
            }
#endif

            if (snapshot.binDirectory.has_value())
            {
                if (snapshot.processHasPath)
                    snapshot.processPathContainsBin = containsPathEntry(snapshot.processPath, snapshot.binDirectory->string(),
#if defined(_WIN32)
                                                                      ';', true
#else
                                                                      ':', false
#endif
                    );

#if defined(_WIN32)
                snapshot.persistentPathContainsBin = containsPathEntry(snapshot.persistentPath, snapshot.binDirectory->string(), ';', true);
#else
                snapshot.persistentPathContainsBin = snapshot.persistentPath.find(snapshot.binDirectory->string()) != std::string::npos;
#endif
            }

            return snapshot;
        }

        void printEnvironmentStatus(const EnvStatusSnapshot& snapshot)
        {
            std::cout << "Wio environment status\n\n";

            std::cout << "Resolved toolchain root : "
                      << (snapshot.toolchainRoot.has_value() ? snapshot.toolchainRoot->string() : "<not resolved>") << '\n';
            std::cout << "Resolved bin directory  : "
                      << (snapshot.binDirectory.has_value() ? snapshot.binDirectory->string() : "<not resolved>") << '\n';
            std::cout << '\n';

            std::cout << "Current process\n";
            std::cout << "  WIO_ROOT              : " << (snapshot.processHasWioRoot ? snapshot.processWioRoot : "<unset>") << '\n';
            std::cout << "  WIO_HOME              : " << (snapshot.processHasWioHome ? snapshot.processWioHome : "<unset>") << '\n';
            std::cout << "  PATH contains Wio bin : " << (snapshot.processPathContainsBin ? "yes" : "no") << '\n';

#if defined(_WIN32)
            std::cout << "\nPersistent user environment (registry)\n";
            std::cout << "  WIO_ROOT              : " << (snapshot.persistentHasWioRoot ? snapshot.persistentWioRoot : "<unset>") << '\n';
            std::cout << "  WIO_HOME              : " << (snapshot.persistentHasWioHome ? snapshot.persistentWioHome : "<unset>") << '\n';
            std::cout << "  PATH contains Wio bin : " << (snapshot.persistentPathContainsBin ? "yes" : "no") << '\n';
            std::cout << '\n';

            if (snapshot.caseInsensitiveDuplicateEnvKeys.empty())
            {
                std::cout << "Current shell duplicate env keys : none\n";
            }
            else
            {
                std::cout << "Current shell duplicate env keys : ";
                for (size_t i = 0; i < snapshot.caseInsensitiveDuplicateEnvKeys.size(); ++i)
                {
                    if (i > 0)
                        std::cout << ", ";
                    std::cout << snapshot.caseInsensitiveDuplicateEnvKeys[i];
                }
                std::cout << '\n';
            }
#else
            std::cout << "\nPersistent user environment (.profile)\n";
            std::cout << "  Wio profile block     : " << (snapshot.profileMarkerPresent ? "present" : "missing") << '\n';
            std::cout << "  WIO_ROOT configured   : " << (snapshot.persistentHasWioRoot ? "yes" : "no") << '\n';
            std::cout << "  WIO_HOME configured   : " << (snapshot.persistentHasWioHome ? "yes" : "no") << '\n';
            std::cout << "  PATH contains Wio bin : " << (snapshot.persistentPathContainsBin ? "yes" : "no") << '\n';
#endif
        }

        std::vector<std::string> diagnoseEnvironment(const EnvStatusSnapshot& snapshot)
        {
            std::vector<std::string> issues;
            if (!snapshot.toolchainRoot.has_value())
                issues.emplace_back("Could not resolve a Wio toolchain root from the current command, environment, or working directory.");

            if (!snapshot.processHasPath || !snapshot.processPathContainsBin)
                issues.emplace_back("The current shell PATH does not contain the active Wio bin directory.");

            if (!snapshot.persistentHasWioRoot || !snapshot.persistentHasWioHome)
                issues.emplace_back("The persistent user environment is missing WIO_ROOT and/or WIO_HOME.");

            if (snapshot.binDirectory.has_value() && !snapshot.persistentPathContainsBin)
                issues.emplace_back("The persistent user PATH does not contain the Wio bin directory.");

            if (!snapshot.caseInsensitiveDuplicateEnvKeys.empty())
                issues.emplace_back("The current shell exposes duplicate environment keys (for example Path/PATH), which can break MSBuild and other .NET tools on Windows.");

            return issues;
        }

        Argonaut::Parser makeEnvPrintParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("WIO-ROOT")
                        .AddAlias("--wio-root")
                        .SetDefaultValue("")
                        .SetDescription("Optional explicit Wio toolchain root.")
                )
                .Add(
                    Argonaut::Argument("SHELL")
                        .AddAlias("--shell")
                        .SetDefaultValue(determineDefaultShell())
                        .SetDescription("Shell syntax to emit: powershell, cmd, or sh.")
                )
                .Add(
                    Argonaut::Argument("ADD-PATH")
                        .AddAlias("--add-path")
                        .Flag()
                        .SetDescription("Include the Wio bin directory in the emitted PATH command.")
                )
                .AutoHelp()
                .SetVersion("0.1.0");

            return parser;
        }

        Argonaut::Parser makeEnvSetupParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("WIO-ROOT")
                        .AddAlias("--wio-root")
                        .SetDefaultValue("")
                        .SetDescription("Optional explicit Wio toolchain root.")
                )
                .Add(
                    Argonaut::Argument("SET-USER")
                        .AddAlias("--set-user")
                        .Flag()
                        .SetDescription("Persist WIO_ROOT and WIO_HOME for the current user.")
                )
                .Add(
                    Argonaut::Argument("NO-PROMPT")
                        .AddAlias("--no-prompt")
                        .Flag()
                        .SetDescription("Do not ask interactive questions; print commands unless --set-user is provided.")
                )
                .Add(
                    Argonaut::Argument("ADD-PATH")
                        .AddAlias("--add-path")
                        .Flag()
                        .SetDescription("Include the packaged bin directory in PATH when persisting settings.")
                )
                .AutoHelp()
                .SetVersion("0.1.0");

            return parser;
        }

        Argonaut::Parser makeEnvStatusParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("WIO-ROOT")
                        .AddAlias("--wio-root")
                        .SetDefaultValue("")
                        .SetDescription("Optional explicit Wio toolchain root.")
                )
                .AutoHelp()
                .SetVersion("0.1.0");

            return parser;
        }

        Argonaut::Parser makeEnvRemoveParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("WIO-ROOT")
                        .AddAlias("--wio-root")
                        .SetDefaultValue("")
                        .SetDescription("Optional explicit Wio toolchain root.")
                )
                .Add(
                    Argonaut::Argument("SHELL")
                        .AddAlias("--shell")
                        .SetDefaultValue(determineDefaultShell())
                        .SetDescription("Shell syntax to emit for preview mode: powershell, cmd, or sh.")
                )
                .Add(
                    Argonaut::Argument("SET-USER")
                        .AddAlias("--set-user")
                        .Flag()
                        .SetDescription("Persist removal for the current user.")
                )
                .Add(
                    Argonaut::Argument("NO-PROMPT")
                        .AddAlias("--no-prompt")
                        .Flag()
                        .SetDescription("Do not ask interactive questions; print commands unless --set-user is provided.")
                )
                .Add(
                    Argonaut::Argument("REMOVE-PATH")
                        .AddAlias("--remove-path")
                        .Flag()
                        .SetDescription("Also remove the Wio bin directory from PATH.")
                )
                .AutoHelp()
                .SetVersion("0.1.0");

            return parser;
        }

        Argonaut::Parser makeEnvDoctorParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("WIO-ROOT")
                        .AddAlias("--wio-root")
                        .SetDefaultValue("")
                        .SetDescription("Optional explicit Wio toolchain root.")
                )
                .Add(
                    Argonaut::Argument("BACKEND-SMOKE")
                        .AddAlias("--backend-smoke")
                        .Flag()
                        .SetDescription("Compile and run a tiny Wio program to verify the bundled/native backend toolchain.")
                )
                .AutoHelp()
                .SetVersion("0.1.0");

            return parser;
        }

        int handleEnvPrintCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeEnvPrintParser();
            if (const auto parseResult = parseWithHandling(parser, args, "Env print"); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::string rootArgument = parser.GetValuesOf<std::string>("WIO-ROOT").front();
                const std::string shell = parser.GetValuesOf<std::string>("SHELL").front();
                const bool addPath = getFlagValue(parser, "ADD-PATH");

                const std::optional<std::filesystem::path> explicitRoot =
                    rootArgument.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::path(rootArgument));

                const auto toolchainRoot = tryFindToolchainRoot(explicitRoot);
                if (!toolchainRoot.has_value())
                    throw std::runtime_error("Could not resolve a Wio toolchain root.");

                const std::filesystem::path binDirectory = resolveBinDirectory(*toolchainRoot);
                std::cout << renderShellEnvironment(*toolchainRoot, binDirectory, shell, addPath) << '\n';
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Env print failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        int handleEnvSetupCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeEnvSetupParser();
            if (const auto parseResult = parseWithHandling(parser, args, "Env setup"); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::string rootArgument = parser.GetValuesOf<std::string>("WIO-ROOT").front();
                bool setUser = getFlagValue(parser, "SET-USER");
                const bool noPrompt = getFlagValue(parser, "NO-PROMPT");
                bool addPath = getFlagValue(parser, "ADD-PATH");

                const std::optional<std::filesystem::path> explicitRoot =
                    rootArgument.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::path(rootArgument));

                const auto toolchainRoot = tryFindToolchainRoot(explicitRoot);
                if (!toolchainRoot.has_value())
                    throw std::runtime_error("Could not resolve a Wio toolchain root.");

                const std::filesystem::path binDirectory = resolveBinDirectory(*toolchainRoot);

                if (!setUser && !noPrompt)
                    setUser = promptYesNo("Persist WIO_ROOT and WIO_HOME for the current user?");

                if (setUser && !addPath && !noPrompt)
                    addPath = promptYesNo("Add the Wio bin directory to PATH for the current user?");

                if (setUser)
                {
#if defined(_WIN32)
                    persistUserEnvironmentWindows(*toolchainRoot, binDirectory, addPath);
#else
                    persistUserEnvironmentPosix(*toolchainRoot, binDirectory, addPath);
#endif
                    std::cout << "Updated user environment for Wio.\n";
                    std::cout << "WIO_ROOT=" << toolchainRoot->string() << '\n';
                    std::cout << "WIO_HOME=" << toolchainRoot->string() << '\n';
                    if (addPath)
                        std::cout << "PATH includes " << binDirectory.string() << '\n';
                    std::cout << "Open a new terminal to pick up the persistent PATH changes cleanly.\n";
                    return EXIT_SUCCESS;
                }

                std::cout << "No persistent environment changes were applied.\n";
                std::cout << "This was a preview-only setup. If you want a persistent install, rerun with:\n";
                std::cout << "  wio env setup --wio-root " << toolchainRoot->string() << " --set-user";
                if (addPath)
                    std::cout << " --add-path";
                std::cout << "\n\n";
                std::cout << "Use one of the following commands for the current shell:\n\n";
                std::cout << renderShellEnvironment(*toolchainRoot, binDirectory, determineDefaultShell(), addPath) << '\n';
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Env setup failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        int handleEnvStatusCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeEnvStatusParser();
            if (const auto parseResult = parseWithHandling(parser, args, "Env status"); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::string rootArgument = parser.GetValuesOf<std::string>("WIO-ROOT").front();
                const std::optional<std::filesystem::path> explicitRoot =
                    rootArgument.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::path(rootArgument));

                const EnvStatusSnapshot snapshot = inspectEnvironment(explicitRoot);
                printEnvironmentStatus(snapshot);
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Env status failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        int handleEnvRemoveCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeEnvRemoveParser();
            if (const auto parseResult = parseWithHandling(parser, args, "Env remove"); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::string rootArgument = parser.GetValuesOf<std::string>("WIO-ROOT").front();
                const std::string shell = parser.GetValuesOf<std::string>("SHELL").front();
                bool setUser = getFlagValue(parser, "SET-USER");
                const bool noPrompt = getFlagValue(parser, "NO-PROMPT");
                bool removePath = getFlagValue(parser, "REMOVE-PATH");

                const std::optional<std::filesystem::path> explicitRoot =
                    rootArgument.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::path(rootArgument));

                const auto toolchainRoot = tryFindToolchainRoot(explicitRoot);
                if (!toolchainRoot.has_value())
                    throw std::runtime_error("Could not resolve a Wio toolchain root for environment removal.");

                const std::filesystem::path binDirectory = resolveBinDirectory(*toolchainRoot);

                if (!setUser && !noPrompt)
                    setUser = promptYesNo("Remove persistent Wio environment values for the current user?");

                if (setUser && !removePath && !noPrompt)
                    removePath = promptYesNo("Also remove the Wio bin directory from PATH?");

                if (setUser)
                {
#if defined(_WIN32)
                    removeUserEnvironmentWindows(binDirectory, removePath);
#else
                    removeUserEnvironmentPosix(binDirectory, removePath);
#endif
                    std::cout << "Removed persistent Wio environment values for the current user.\n";
                    if (removePath)
                        std::cout << "PATH no longer includes " << binDirectory.string() << '\n';
                    std::cout << "Open a new terminal to observe the cleaned environment.\n";
                    return EXIT_SUCCESS;
                }

                std::cout << "No persistent environment changes were applied.\n";
                std::cout << "This was a preview-only removal. If you want a persistent cleanup, rerun with:\n";
                std::cout << "  wio env remove --wio-root " << toolchainRoot->string() << " --set-user";
                if (removePath)
                    std::cout << " --remove-path";
                std::cout << "\n\n";
                std::cout << "Use one of the following commands for the current shell:\n\n";
                std::cout << renderShellEnvironmentRemoval(binDirectory, shell, removePath) << '\n';
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Env remove failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        int handleEnvDoctorCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeEnvDoctorParser();
            if (const auto parseResult = parseWithHandling(parser, args, "Env doctor"); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::string rootArgument = parser.GetValuesOf<std::string>("WIO-ROOT").front();
                const bool backendSmokeRequested = getFlagValue(parser, "BACKEND-SMOKE");
                const std::optional<std::filesystem::path> explicitRoot =
                    rootArgument.empty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::path(rootArgument));

                const EnvStatusSnapshot snapshot = inspectEnvironment(explicitRoot);
                const std::vector<std::string> issues = diagnoseEnvironment(snapshot);
                std::optional<BackendSmokeResult> backendSmoke;
                if (backendSmokeRequested && snapshot.toolchainRoot.has_value())
                    backendSmoke = runBackendSmoke();

                std::cout << "Wio environment doctor\n\n";
                if (issues.empty())
                {
                    std::cout << "No obvious environment problems were detected.\n";
                    if (!backendSmoke.has_value())
                        return EXIT_SUCCESS;
                }
                else
                {
                    std::cout << "Detected issues:\n";
                    for (const std::string& issue : issues)
                        std::cout << "- " << issue << '\n';
                }

#if defined(_WIN32)
                if (!snapshot.caseInsensitiveDuplicateEnvKeys.empty())
                {
                    std::cout << "\nQuick current-shell fix for Path/PATH style collisions:\n";
                    std::cout << "$effectivePath = $env:Path\n";
                    std::cout << "Remove-Item Env:PATH -ErrorAction SilentlyContinue\n";
                    std::cout << "$env:Path = $effectivePath\n";
                }
#endif

                if (snapshot.toolchainRoot.has_value())
                {
                    std::cout << "\nRecommended persistent install command:\n";
                    std::cout << "wio env setup --wio-root " << snapshot.toolchainRoot->string() << " --set-user --add-path\n";
                    std::cout << "\nRecommended persistent cleanup command:\n";
                    std::cout << "wio env remove --wio-root " << snapshot.toolchainRoot->string() << " --set-user --remove-path\n";
                }

                if (backendSmoke.has_value())
                {
                    std::cout << "\nBackend smoke:\n";
                    if (backendSmoke->success)
                    {
                        std::cout << "- Success: compile + run passed.\n";
                        std::cout << "- Log: " << backendSmoke->logPath.string() << '\n';
                    }
                    else
                    {
                        std::cout << "- Failed with exit code " << backendSmoke->exitCode << '\n';
                        if (!backendSmoke->sourcePath.empty())
                            std::cout << "- Smoke file: " << backendSmoke->sourcePath.string() << '\n';
                        if (!backendSmoke->logPath.empty())
                            std::cout << "- Log: " << backendSmoke->logPath.string() << '\n';
                        if (!backendSmoke->output.empty())
                            std::cout << "\nSmoke output:\n" << backendSmoke->output << '\n';
                        return EXIT_FAILURE;
                    }
                }

                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Env doctor failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    std::optional<int> tryHandleEnvCommand(int argc, char* argv[])
    {
        if (argc < 2 || argv == nullptr || argv[1] == nullptr)
            return std::nullopt;

        const std::string_view command = argv[1];
        if (command != "env")
            return std::nullopt;

        if (argc < 3 || argv[2] == nullptr)
        {
            std::cerr << "Expected an env subcommand. Currently supported: print, setup, status, remove, doctor\n";
            return EXIT_FAILURE;
        }

        const std::string_view subcommand = argv[2];
        if (subcommand == "print")
            return handleEnvPrintCommand(collectCommandArgs("wio env print", argc, argv, 3));
        if (subcommand == "setup")
            return handleEnvSetupCommand(collectCommandArgs("wio env setup", argc, argv, 3));
        if (subcommand == "status")
            return handleEnvStatusCommand(collectCommandArgs("wio env status", argc, argv, 3));
        if (subcommand == "remove")
            return handleEnvRemoveCommand(collectCommandArgs("wio env remove", argc, argv, 3));
        if (subcommand == "doctor")
            return handleEnvDoctorCommand(collectCommandArgs("wio env doctor", argc, argv, 3));

        std::cerr << "Unknown env subcommand: " << subcommand << '\n';
        return EXIT_FAILURE;
    }
}
