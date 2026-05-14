#include "package_cli.h"

#include <argonaut.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace wio::tooling::package
{
    namespace
    {
        std::string quoteCommandPart(const std::string& value)
        {
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
                return value;

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
        }

        std::string joinCommand(const std::vector<std::string>& parts)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < parts.size(); ++i)
            {
                if (i > 0)
                    stream << ' ';
                stream << quoteCommandPart(parts[i]);
            }
            return stream.str();
        }

        int runShellCommand(const std::vector<std::string>& parts,
                            const std::optional<std::filesystem::path>& workingDirectory = std::nullopt)
        {
            const std::string command = joinCommand(parts);

#if defined(_WIN32)
            LPCH environmentStrings = GetEnvironmentStringsA();
            if (environmentStrings == nullptr)
                return EXIT_FAILURE;

            std::unordered_set<std::string> seenKeys;
            std::vector<std::string> sanitizedEntries;

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

                if (!seenKeys.insert(normalizedKey).second)
                    continue;

                sanitizedEntries.emplace_back(entry);
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
                workingDirectory.has_value() ? workingDirectory->string().c_str() : nullptr,
                &startupInfo,
                &processInfo
            );

            if (created == FALSE)
                return EXIT_FAILURE;

            WaitForSingleObject(processInfo.hProcess, INFINITE);

            DWORD exitCode = EXIT_FAILURE;
            GetExitCodeProcess(processInfo.hProcess, &exitCode);

            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
            return static_cast<int>(exitCode);
#else
            if (workingDirectory.has_value())
            {
                const auto previous = std::filesystem::current_path();
                std::filesystem::current_path(*workingDirectory);
                const int result = std::system(command.c_str());
                std::filesystem::current_path(previous);
                return result;
            }
            return std::system(command.c_str());
#endif
        }

        std::optional<std::filesystem::path> tryFindRepoRoot()
        {
            std::error_code ec;
            std::filesystem::path current = std::filesystem::current_path(ec);
            if (ec)
                current.clear();

            std::vector<std::filesystem::path> seeds;
            if (!current.empty())
                seeds.push_back(current);

#if defined(_WIN32)
            std::wstring buffer(MAX_PATH, L'\0');
            const DWORD copiedLength = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (copiedLength > 0)
            {
                buffer.resize(copiedLength);
                seeds.push_back(std::filesystem::path(buffer).parent_path());
            }
#endif

            auto isRepoRoot = [](const std::filesystem::path& candidate) -> bool
            {
                std::error_code localEc;
                return std::filesystem::exists(candidate / "CMakeLists.txt", localEc) &&
                       std::filesystem::exists(candidate / "compiler", localEc) &&
                       std::filesystem::exists(candidate / "app", localEc);
            };

            for (auto seed : seeds)
            {
                while (!seed.empty())
                {
                    if (isRepoRoot(seed))
                        return std::filesystem::absolute(seed).make_preferred();

                    const auto parent = seed.parent_path();
                    if (parent == seed)
                        break;
                    seed = parent;
                }
            }

            return std::nullopt;
        }

        std::string readUtf8File(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream.is_open())
                throw std::runtime_error("Could not open file: " + path.string());

            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        void writeUtf8File(const std::filesystem::path& path, const std::string& content)
        {
            std::error_code ec;
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path(), ec);

            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
                throw std::runtime_error("Could not open file for writing: " + path.string());

            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!stream.good())
                throw std::runtime_error("Could not write file: " + path.string());
        }

        std::string jsonEscape(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 8);
            for (const char ch : value)
            {
                switch (ch)
                {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped.push_back(ch);
                    break;
                }
            }
            return escaped;
        }

        std::string quoteJson(const std::string& value)
        {
            return "\"" + jsonEscape(value) + "\"";
        }

        std::string currentUtcIso8601()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

            std::tm utc{};
#if defined(_WIN32)
            gmtime_s(&utc, &nowTime);
#else
            gmtime_r(&nowTime, &utc);
#endif

            std::ostringstream stream;
            stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
            return stream.str();
        }

        std::string getWioVersion(const std::filesystem::path& cmakeListsPath)
        {
            const std::string content = readUtf8File(cmakeListsPath);
            std::smatch match;
            if (!std::regex_search(content, match, std::regex(R"(project\s*\(\s*wio_lang\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+))", std::regex::icase)))
                throw std::runtime_error("Could not determine Wio version from '" + cmakeListsPath.string() + "'.");

            return match[1].str();
        }

        std::string getPlatformTag()
        {
#if defined(_WIN32)
            return "windows";
#elif defined(__APPLE__)
            return "macos";
#elif defined(__linux__)
            return "linux";
#else
            return "unknown";
#endif
        }

        std::string getArchitectureTag()
        {
#if defined(_M_X64) || defined(__x86_64__)
            return "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
            return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
            return "x86";
#else
            return "unknown";
#endif
        }

        std::string normalizeSuffixTag(const std::string& value)
        {
            if (value.empty())
                return {};

            std::string normalized;
            normalized.reserve(value.size());
            for (const unsigned char ch : value)
            {
                if (std::isalnum(ch) != 0 || ch == '.' || ch == '_' || ch == '-')
                    normalized.push_back(static_cast<char>(ch));
                else
                    normalized.push_back('-');
            }

            return normalized;
        }

        Argonaut::Parser makePackageParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("BUILD-DIR")
                        .AddAlias("--build-dir")
                        .SetDefaultValue("build")
                        .SetDescription("Repo build directory used to configure and stage the package.")
                )
                .Add(
                    Argonaut::Argument("CONFIG")
                        .AddAlias("--config")
                        .SetDefaultValue("Release")
                        .SetDescription("Build configuration used for the package.")
                )
                .Add(
                    Argonaut::Argument("OUTPUT-DIR")
                        .AddAlias("--output-dir")
                        .SetDefaultValue("artifacts\\packages")
                        .SetDescription("Directory where the versioned package folder will be written.")
                )
                .Add(
                    Argonaut::Argument("VERSION-SUFFIX")
                        .AddAlias("--version-suffix")
                        .SetDefaultValue("")
                        .SetDescription("Optional suffix appended to the package name.")
                )
                .Add(
                    Argonaut::Argument("GENERATOR")
                        .AddAlias("--generator")
                        .SetDefaultValue("")
                        .SetDescription("Optional CMake generator override.")
                )
                .Add(
                    Argonaut::Argument("NO-ZIP")
                        .AddAlias("--no-zip")
                        .Flag()
                        .SetDescription("Skip creating the .zip archive and only stage the package directory.")
                )
                .Add(
                    Argonaut::Argument("CLEAN")
                        .AddAlias("--clean")
                        .Flag()
                        .SetDescription("Delete any previous package directory and archive before staging a fresh one.")
                )
                .AutoHelp()
                .SetVersion("0.1.0");

            return parser;
        }

        std::vector<char*> buildArgvView(std::vector<std::string>& args)
        {
            std::vector<char*> argvView;
            argvView.reserve(args.size());
            for (std::string& arg : args)
                argvView.push_back(arg.data());
            return argvView;
        }

        std::optional<int> parseWithHandling(Argonaut::Parser& parser, std::vector<std::string>& args)
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
                std::cerr << "Package CLI setup failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const Argonaut::ParseException& e)
            {
                std::cerr << "Package argument parsing failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Unhandled package CLI error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
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

        bool getFlagValue(Argonaut::Parser& parser, const std::string& id)
        {
            auto values = parser.GetValuesOf<bool>(id);
            return !values.empty() && values.front();
        }

        std::string buildInstallScript()
        {
            return R"(param(
    [switch]$SetUserEnvironment,
    [switch]$NoPrompt
)

$ErrorActionPreference = "Stop"

$packageRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
$binDir = Join-Path $packageRoot "bin"

if (-not (Test-Path -LiteralPath $binDir)) {
    throw "The packaged Wio bin directory was not found under '$packageRoot'."
}

$shouldSetEnvironment = $SetUserEnvironment
if (-not $shouldSetEnvironment -and -not $NoPrompt) {
    $choice = Read-Host "Set WIO_ROOT and WIO_HOME for the current user? [y/N]"
    if ($choice -match '^(y|yes)$') {
        $shouldSetEnvironment = $true
    }
}

if ($shouldSetEnvironment) {
    [Environment]::SetEnvironmentVariable("WIO_ROOT", $packageRoot, "User")
    [Environment]::SetEnvironmentVariable("WIO_HOME", $packageRoot, "User")
    $env:WIO_ROOT = $packageRoot
    $env:WIO_HOME = $packageRoot
    Write-Host "Set user environment variables:"
    Write-Host "  WIO_ROOT=$packageRoot"
    Write-Host "  WIO_HOME=$packageRoot"
} else {
    Write-Host "Skipped persistent WIO_ROOT/WIO_HOME configuration."
}

Write-Host ""
Write-Host "Wio package root: $packageRoot"
Write-Host "Binary directory : $binDir"
Write-Host ""
Write-Host "Recommended next steps:"
Write-Host "  1. Run '$binDir\wio.exe --help' (or 'wio --help' if already on PATH)."
Write-Host "  2. Use the packaged scripts only as compatibility helpers while the Wio CLI grows."
Write-Host "  3. Point CMake projects at WIO_ROOT='$packageRoot' when using WioProject.cmake."
)";
        }

        int handlePackageCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makePackageParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            try
            {
                const auto repoRoot = tryFindRepoRoot();
                if (!repoRoot.has_value())
                    throw std::runtime_error("Could not resolve the Wio repository root for 'wio package'.");

                const std::string buildDirValue = parser.GetValuesOf<std::string>("BUILD-DIR").front();
                const std::string config = parser.GetValuesOf<std::string>("CONFIG").front();
                const std::string outputDirValue = parser.GetValuesOf<std::string>("OUTPUT-DIR").front();
                const std::string versionSuffix = parser.GetValuesOf<std::string>("VERSION-SUFFIX").front();
                const std::string generator = parser.GetValuesOf<std::string>("GENERATOR").front();
                const bool noZip = getFlagValue(parser, "NO-ZIP");
                const bool clean = getFlagValue(parser, "CLEAN");

                const std::filesystem::path buildDir =
                    std::filesystem::path(buildDirValue).is_absolute()
                        ? std::filesystem::path(buildDirValue).make_preferred()
                        : std::filesystem::absolute(*repoRoot / buildDirValue).make_preferred();

                const std::filesystem::path outputDir =
                    std::filesystem::path(outputDirValue).is_absolute()
                        ? std::filesystem::path(outputDirValue).make_preferred()
                        : std::filesystem::absolute(*repoRoot / outputDirValue).make_preferred();

                const std::filesystem::path cmakeListsPath = *repoRoot / "CMakeLists.txt";
                const std::filesystem::path licensePath = *repoRoot / "LICENSE";
                const std::filesystem::path readmePath = *repoRoot / "README.md";
                const std::filesystem::path languageDraftPath = *repoRoot / "docs" / "WIO_LANGUAGE_DRAFT.md";

                const std::string version = getWioVersion(cmakeListsPath);
                const std::string platformTag = getPlatformTag();
                const std::string architectureTag = getArchitectureTag();

                std::string normalizedConfig = config;
                for (char& ch : normalizedConfig)
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

                const std::string suffixTag = versionSuffix.empty() ? "" : "-" + normalizeSuffixTag(versionSuffix);
                const std::string packageName = "wio-" + version + "-" + platformTag + "-" + architectureTag + "-" + normalizedConfig + suffixTag;

                const std::filesystem::path packageRoot = outputDir / packageName;
                const std::filesystem::path distPrefix = buildDir / "dist";
                const std::filesystem::path archivePath = outputDir / (packageName + ".zip");

                std::error_code ec;
                if (clean)
                {
                    std::filesystem::remove_all(packageRoot, ec);
                    ec.clear();
                    std::filesystem::remove(archivePath, ec);
                    ec.clear();
                }

                std::filesystem::create_directories(outputDir, ec);
                if (ec)
                    throw std::runtime_error("Could not create package output directory: " + outputDir.string());

                if (std::filesystem::exists(distPrefix, ec))
                {
                    std::filesystem::remove_all(distPrefix, ec);
                    if (ec)
                        throw std::runtime_error("Could not remove previous dist staging directory: " + distPrefix.string());
                }

                std::vector<std::string> configureCommand{
                    "cmake",
                    "-S", repoRoot->string(),
                    "-B", buildDir.string(),
                    "-DWIO_DIST_DIR=" + distPrefix.string()
                };
                if (!generator.empty())
                {
                    configureCommand.push_back("-G");
                    configureCommand.push_back(generator);
                }

                if (const int configureResult = runShellCommand(configureCommand, *repoRoot); configureResult != 0)
                    return configureResult;

                if (const int buildResult = runShellCommand({
                        "cmake",
                        "--build", buildDir.string(),
                        "--target", "wio_dist",
                        "--config", config
                    }, *repoRoot);
                    buildResult != 0)
                {
                    return buildResult;
                }

                if (std::filesystem::exists(packageRoot, ec))
                {
                    std::filesystem::remove_all(packageRoot, ec);
                    if (ec)
                        throw std::runtime_error("Could not remove previous package root: " + packageRoot.string());
                }

                std::filesystem::copy(distPrefix,
                                      packageRoot,
                                      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                                      ec);
                if (ec)
                    throw std::runtime_error("Could not copy staged distribution into package root: " + packageRoot.string());

                if (std::filesystem::exists(licensePath, ec))
                    std::filesystem::copy_file(licensePath, packageRoot / "LICENSE", std::filesystem::copy_options::overwrite_existing, ec);
                ec.clear();
                if (std::filesystem::exists(readmePath, ec))
                    std::filesystem::copy_file(readmePath, packageRoot / "README.md", std::filesystem::copy_options::overwrite_existing, ec);
                ec.clear();

                if (std::filesystem::exists(languageDraftPath, ec))
                {
                    std::filesystem::create_directories((packageRoot / "docs"), ec);
                    ec.clear();
                    std::filesystem::copy_file(languageDraftPath,
                                               packageRoot / "docs" / "WIO_LANGUAGE_DRAFT.md",
                                               std::filesystem::copy_options::overwrite_existing,
                                               ec);
                }
                ec.clear();

                std::ostringstream packageInfo;
                packageInfo
                    << "{\n"
                    << "  \"name\": " << quoteJson(packageName) << ",\n"
                    << "  \"version\": " << quoteJson(version) << ",\n"
                    << "  \"platform\": " << quoteJson(platformTag) << ",\n"
                    << "  \"architecture\": " << quoteJson(architectureTag) << ",\n"
                    << "  \"config\": " << quoteJson(config) << ",\n"
                    << "  \"buildDir\": " << quoteJson(buildDir.string()) << ",\n"
                    << "  \"packageRoot\": " << quoteJson(packageRoot.string()) << ",\n"
                    << "  \"generatedAtUtc\": " << quoteJson(currentUtcIso8601()) << "\n"
                    << "}\n";

                writeUtf8File(packageRoot / "WIO_PACKAGE_INFO.json", packageInfo.str());
                writeUtf8File(packageRoot / "Install-Wio.ps1", buildInstallScript());

                if (!noZip)
                {
                    if (std::filesystem::exists(archivePath, ec))
                    {
                        std::filesystem::remove(archivePath, ec);
                        if (ec)
                            throw std::runtime_error("Could not remove previous archive: " + archivePath.string());
                    }

                    if (const int zipResult = runShellCommand({
                            "cmake",
                            "-E",
                            "tar",
                            "cf",
                            archivePath.filename().string(),
                            "--format=zip",
                            packageName
                        }, outputDir);
                        zipResult != 0)
                    {
                        return zipResult;
                    }
                }

                std::cout << "Wio package root : " << packageRoot.string() << '\n';
                if (!noZip)
                    std::cout << "Wio package zip  : " << archivePath.string() << '\n';

                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Package creation failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    std::optional<int> tryHandlePackageCommand(int argc, char* argv[])
    {
        if (argc < 2 || argv == nullptr || argv[1] == nullptr)
            return std::nullopt;

        const std::string_view command = argv[1];
        if (command != "package")
            return std::nullopt;

        return handlePackageCommand(collectCommandArgs("wio package", argc, argv, 2));
    }
}
