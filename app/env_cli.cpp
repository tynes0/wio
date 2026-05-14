#include "env_cli.h"

#include <argonaut.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace wio::tooling::env
{
    namespace
    {
        constexpr std::string_view kProfileMarkerBegin = "# >>> wio env >>>";
        constexpr std::string_view kProfileMarkerEnd = "# <<< wio env <<<";

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
#endif

        std::string determineDefaultShell()
        {
#if defined(_WIN32)
            return "powershell";
#else
            return "sh";
#endif
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
                    return EXIT_SUCCESS;
                }

                std::cout << "No persistent environment changes were applied.\n";
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
            std::cerr << "Expected an env subcommand. Currently supported: print, setup\n";
            return EXIT_FAILURE;
        }

        const std::string_view subcommand = argv[2];
        if (subcommand == "print")
            return handleEnvPrintCommand(collectCommandArgs("wio env print", argc, argv, 3));
        if (subcommand == "setup")
            return handleEnvSetupCommand(collectCommandArgs("wio env setup", argc, argv, 3));

        std::cerr << "Unknown env subcommand: " << subcommand << '\n';
        return EXIT_FAILURE;
    }
}
