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
                    Argonaut::Argument("VISUAL-INSTALLER")
                        .AddAlias("--visual-installer")
                        .Flag()
                        .SetDescription("Build the Windows visual installer .exe when supported.")
                )
                .Add(
                    Argonaut::Argument("NO-VISUAL-INSTALLER")
                        .AddAlias("--no-visual-installer")
                        .Flag()
                        .SetDescription("Skip building the Windows visual installer .exe.")
                )
                .Add(
                    Argonaut::Argument("INNO-COMPILER")
                        .AddAlias("--inno-compiler")
                        .SetDefaultValue("")
                        .SetDescription("Optional path to ISCC.exe used for the Windows visual installer.")
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

        std::string buildPowerShellInstallScript()
        {
            return R"(param(
    [string]$InstallRoot,
    [switch]$AllUsers,
    [switch]$NoPrompt,
    [switch]$Force,
    [switch]$SkipEnvironmentSetup,
    [switch]$SkipPath,
    [switch]$SetUserEnvironment,
    [switch]$AddPath
)

$ErrorActionPreference = "Stop"

$packageRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
$packageWioExe = Join-Path $packageRoot "bin\wio.exe"

if (-not (Test-Path -LiteralPath $packageWioExe)) {
    throw "The packaged wio executable was not found under '$packageRoot\bin'."
}

if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    if ($AllUsers) {
        $InstallRoot = Join-Path $env:ProgramFiles "Wio"
    } elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $InstallRoot = Join-Path $env:LOCALAPPDATA "Programs\Wio"
    } else {
        $InstallRoot = Join-Path $HOME "Wio"
    }
}

$packageRoot = [System.IO.Path]::GetFullPath($packageRoot)
$InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)

if ((-not $Force) -and (-not $NoPrompt) -and (Test-Path -LiteralPath $InstallRoot) -and ($packageRoot -ne $InstallRoot)) {
    $response = Read-Host "Wio is already installed at '$InstallRoot'. Overwrite it? [y/N]"
    if ($response -notmatch '^(?i:y|yes)$') {
        Write-Host "Installation cancelled."
        exit 1
    }
}

if ($packageRoot -ne $InstallRoot) {
    $installParent = Split-Path -Parent $InstallRoot
    if (-not [string]::IsNullOrWhiteSpace($installParent)) {
        New-Item -ItemType Directory -Force -Path $installParent | Out-Null
    }

    if (Test-Path -LiteralPath $InstallRoot) {
        Remove-Item -LiteralPath $InstallRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
    Get-ChildItem -LiteralPath $packageRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $InstallRoot -Recurse -Force
    }
}

$installedWioExe = Join-Path $InstallRoot "bin\wio.exe"
if (-not (Test-Path -LiteralPath $installedWioExe)) {
    throw "The installed wio executable was not found under '$InstallRoot\bin'."
}

if (-not $SkipEnvironmentSetup) {
    $cliArgs = @("env", "setup", "--wio-root", $InstallRoot, "--set-user", "--no-prompt")

    if (-not $SkipPath) {
        $cliArgs += "--add-path"
    }

    & $installedWioExe @cliArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host "Wio installed to '$InstallRoot'."
if (-not $SkipEnvironmentSetup) {
    Write-Host "Open a new terminal and run 'wio'."
}
exit $LASTEXITCODE
)";
        }

        std::string buildShellInstallScript()
        {
            return R"WIOINSTALL(#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_WIO_EXE="$SCRIPT_DIR/bin/wio"
INSTALL_ROOT="${WIO_INSTALL_ROOT:-$HOME/.local/share/wio}"
SKIP_PATH=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-root)
            INSTALL_ROOT="$2"
            shift 2
            ;;
        --skip-path)
            SKIP_PATH=1
            shift
            ;;
        *)
            echo "Unknown install-wio.sh argument: $1" >&2
            exit 1
            ;;
    esac
done

if [ ! -x "$PACKAGE_WIO_EXE" ]; then
    echo "The packaged wio executable was not found under '$SCRIPT_DIR/bin'." >&2
    exit 1
fi

if [ "$SCRIPT_DIR" != "$INSTALL_ROOT" ]; then
    mkdir -p "$(dirname "$INSTALL_ROOT")"
    rm -rf "$INSTALL_ROOT"
    mkdir -p "$INSTALL_ROOT"

    for entry in "$SCRIPT_DIR"/* "$SCRIPT_DIR"/.[!.]* "$SCRIPT_DIR"/..?*; do
        [ -e "$entry" ] || continue
        cp -R "$entry" "$INSTALL_ROOT"/
    done
fi

INSTALLED_WIO_EXE="$INSTALL_ROOT/bin/wio"

if [ ! -x "$INSTALLED_WIO_EXE" ]; then
    echo "The installed wio executable was not found under '$INSTALL_ROOT/bin'." >&2
    exit 1
fi

if [ "$SKIP_PATH" -eq 1 ]; then
    exec "$INSTALLED_WIO_EXE" env setup --wio-root "$INSTALL_ROOT" --set-user
fi

exec "$INSTALLED_WIO_EXE" env setup --wio-root "$INSTALL_ROOT" --set-user --add-path
)WIOINSTALL";
        }

        std::string buildPowerShellUninstallScript()
        {
            return R"(param(
    [string]$InstallRoot,
    [switch]$NoPrompt,
    [switch]$KeepFiles
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path $MyInvocation.MyCommand.Path -Parent

if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    $InstallRoot = $scriptRoot
}

$InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)
$wioExe = Join-Path $InstallRoot "bin\wio.exe"

if ((-not $NoPrompt) -and (-not $KeepFiles)) {
    $response = Read-Host "Remove Wio from '$InstallRoot'? [y/N]"
    if ($response -notmatch '^(?i:y|yes)$') {
        Write-Host "Uninstall cancelled."
        exit 1
    }
}

if (Test-Path -LiteralPath $wioExe) {
    & $wioExe env remove --wio-root $InstallRoot --set-user --remove-path --no-prompt
}

if ($KeepFiles) {
    Write-Host "Wio environment entries were removed. Files were kept at '$InstallRoot'."
    exit 0
}

$cleanupCommand = "Start-Sleep -Milliseconds 700; Remove-Item -LiteralPath '" + ($InstallRoot.Replace("'", "''")) + "' -Recurse -Force"
Start-Process -FilePath "powershell" -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", $cleanupCommand) -WindowStyle Hidden | Out-Null

Write-Host "Wio uninstall started for '$InstallRoot'."
Write-Host "This window can now be closed."
exit 0
)";
        }

        std::string buildShellUninstallScript()
        {
            return R"WIOUNINSTALL(#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INSTALL_ROOT="${WIO_INSTALL_ROOT:-$SCRIPT_DIR}"
KEEP_FILES=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-root)
            INSTALL_ROOT="$2"
            shift 2
            ;;
        --keep-files)
            KEEP_FILES=1
            shift
            ;;
        *)
            echo "Unknown uninstall-wio.sh argument: $1" >&2
            exit 1
            ;;
    esac
done

WIO_EXE="$INSTALL_ROOT/bin/wio"

if [ -x "$WIO_EXE" ]; then
    "$WIO_EXE" env remove --wio-root "$INSTALL_ROOT" --set-user --remove-path --no-prompt || true
fi

if [ "$KEEP_FILES" -eq 1 ]; then
    echo "Wio environment entries were removed. Files were kept at '$INSTALL_ROOT'."
    exit 0
fi

rm -rf "$INSTALL_ROOT"
echo "Wio uninstalled from '$INSTALL_ROOT'."
)WIOUNINSTALL";
        }

        std::string buildWindowsBootstrapInstallerScript(const std::string& packageName)
        {
            std::string script = R"(param(
    [string]$PackageZipPath,
    [string]$InstallRoot,
    [switch]$AllUsers,
    [switch]$NoPrompt,
    [switch]$Force,
    [switch]$SkipPath
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
if ([string]::IsNullOrWhiteSpace($PackageZipPath)) {
    $PackageZipPath = Join-Path $scriptRoot "__PACKAGE_NAME__.zip"
}

if (-not (Test-Path -LiteralPath $PackageZipPath)) {
    throw "The package zip was not found at '$PackageZipPath'."
}

$stagingRoot = Join-Path $env:TEMP ("wio-installer-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null

try {
    Expand-Archive -LiteralPath $PackageZipPath -DestinationPath $stagingRoot -Force
    $packageRoot = Join-Path $stagingRoot "__PACKAGE_NAME__"
    $installScript = Join-Path $packageRoot "Install-Wio.ps1"
    if (-not (Test-Path -LiteralPath $installScript)) {
        throw "The extracted package does not contain Install-Wio.ps1."
    }

    $args = @()
    if (-not [string]::IsNullOrWhiteSpace($InstallRoot)) { $args += @('-InstallRoot', $InstallRoot) }
    if ($AllUsers) { $args += '-AllUsers' }
    if ($NoPrompt) { $args += '-NoPrompt' }
    if ($Force) { $args += '-Force' }
    if ($SkipPath) { $args += '-SkipPath' }

    & $installScript @args
    exit $LASTEXITCODE
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
)";

            const std::string placeholder = "__PACKAGE_NAME__";
            size_t position = 0;
            while ((position = script.find(placeholder, position)) != std::string::npos)
            {
                script.replace(position, placeholder.size(), packageName);
                position += packageName.size();
            }

            return script;
        }

        std::optional<std::filesystem::path> tryFindExecutableOnPath(const std::string& executableName)
        {
            if (executableName.empty())
                return std::nullopt;

#if defined(_WIN32)
            std::vector<char> buffer(MAX_PATH, '\0');
            const DWORD copied = SearchPathA(nullptr, executableName.c_str(), nullptr, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
            if (copied > 0 && copied < buffer.size())
                return std::filesystem::path(buffer.data()).make_preferred();
#else
            if (const char* pathValue = std::getenv("PATH"))
            {
                std::stringstream stream(pathValue);
                std::string segment;
                while (std::getline(stream, segment, ':'))
                {
                    if (segment.empty())
                        continue;

                    std::filesystem::path candidate = std::filesystem::path(segment) / executableName;
                    std::error_code ec;
                    if (std::filesystem::exists(candidate, ec) && !ec &&
                        std::filesystem::is_regular_file(candidate, ec) && !ec)
                    {
                        return std::filesystem::absolute(candidate).make_preferred();
                    }
                }
            }
#endif

            return std::nullopt;
        }

        std::optional<std::filesystem::path> tryResolvePortableBackendRoot()
        {
            if (const char* explicitRoot = std::getenv("WIO_PORTABLE_BACKEND_ROOT"))
            {
                if (*explicitRoot != '\0')
                {
                    const std::filesystem::path root = std::filesystem::absolute(std::filesystem::path(explicitRoot)).make_preferred();
                    std::error_code ec;
                    if (std::filesystem::exists(root / "bin", ec) && !ec)
                        return root;
                }
            }

#if defined(_WIN32)
            const auto compilerPath = tryFindExecutableOnPath("g++.exe");
            const auto archiverPath = tryFindExecutableOnPath("ar.exe");
#else
            const auto compilerPath = tryFindExecutableOnPath("g++");
            const auto archiverPath = tryFindExecutableOnPath("ar");
#endif

            if (!compilerPath.has_value() || !archiverPath.has_value())
                return std::nullopt;

            const std::filesystem::path compilerRoot = compilerPath->parent_path().parent_path();
            const std::filesystem::path archiverRoot = archiverPath->parent_path().parent_path();
            if (compilerRoot != archiverRoot)
                return std::nullopt;

            std::error_code ec;
            if (!std::filesystem::exists(compilerRoot / "bin", ec) || ec)
                return std::nullopt;
            ec.clear();
            if (!std::filesystem::exists(compilerRoot / "lib", ec) || ec)
                return std::nullopt;

            auto normalizedCompilerRoot = compilerRoot;
            return normalizedCompilerRoot.make_preferred();
        }

        std::filesystem::path copyPortableBackendToolchain(const std::filesystem::path& packageRoot)
        {
            const auto backendRoot = tryResolvePortableBackendRoot();
            if (!backendRoot.has_value())
            {
                throw std::runtime_error(
                    "Could not resolve a portable backend compiler root for packaging. "
                    "Ensure g++.exe and ar.exe are available on PATH, or set WIO_PORTABLE_BACKEND_ROOT."
                );
            }

            const std::filesystem::path targetRoot =
#if defined(_WIN32)
                packageRoot / "toolchains" / "windows-x64-mingw";
#else
                packageRoot / "toolchains" / "host-backend";
#endif

            std::error_code ec;
            if (std::filesystem::exists(targetRoot, ec))
            {
                std::filesystem::remove_all(targetRoot, ec);
                if (ec)
                    throw std::runtime_error("Could not replace previous bundled backend toolchain at: " + targetRoot.string());
            }

            std::filesystem::create_directories(targetRoot.parent_path(), ec);
            if (ec)
                throw std::runtime_error("Could not create bundled backend parent directory: " + targetRoot.parent_path().string());

            std::filesystem::create_directories(targetRoot, ec);
            if (ec)
                throw std::runtime_error("Could not create bundled backend directory: " + targetRoot.string());

            const auto copyRecursive = [&](const std::filesystem::path& relativePath)
            {
                ec.clear();
                const std::filesystem::path sourcePath = (*backendRoot / relativePath).make_preferred();
                if (!std::filesystem::exists(sourcePath, ec) || ec)
                    return;

                ec.clear();
                std::filesystem::copy(sourcePath,
                                      (targetRoot / relativePath).make_preferred(),
                                      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                                      ec);
                if (ec)
                    throw std::runtime_error("Could not copy bundled backend subtree into package root: " + sourcePath.string());
            };

            copyRecursive("bin");
            copyRecursive("include");
            copyRecursive("lib");
            copyRecursive("libexec");
            copyRecursive("x86_64-w64-mingw32");

            for (const std::filesystem::path topLevelFile : {
                     std::filesystem::path("build-info.txt"),
                     std::filesystem::path("COPYING"),
                     std::filesystem::path("COPYING.txt"),
                     std::filesystem::path("LICENSE"),
                     std::filesystem::path("LICENSE.txt")
                 })
            {
                ec.clear();
                const std::filesystem::path sourceFile = (*backendRoot / topLevelFile).make_preferred();
                if (!std::filesystem::exists(sourceFile, ec) || ec)
                    continue;

                ec.clear();
                std::filesystem::copy_file(sourceFile,
                                           (targetRoot / topLevelFile.filename()).make_preferred(),
                                           std::filesystem::copy_options::overwrite_existing,
                                           ec);
                if (ec)
                    throw std::runtime_error("Could not copy bundled backend file into package root: " + sourceFile.string());
            }

            auto normalizedTargetRoot = targetRoot;
            return normalizedTargetRoot.make_preferred();
        }

        std::string buildPackageQuickstart(const std::string& packageName)
        {
            std::ostringstream stream;
            stream
                << "# Wio Package Quickstart\n\n"
                << "This packaged toolchain was staged as `" << packageName << "`.\n\n"
                << "## 1. Try the CLI directly\n\n"
                << "From the package root:\n\n"
                << "```powershell\n"
                << "bin\\wio.exe --help\n"
                << "bin\\wio.exe env print --wio-root . --shell powershell --add-path\n"
                << "```\n\n"
                << "Or from a POSIX shell:\n\n"
                << "```sh\n"
                << "./bin/wio --help\n"
                << "./bin/wio env print --wio-root . --shell sh --add-path\n"
                << "```\n\n"
                << "## 2. Install into a stable location\n\n"
                << "The package zip is portable, but a normal install should copy Wio into a stable root so later moves of the extracted zip do not break `PATH`.\n\n"
                << "PowerShell (default user install under `%LOCALAPPDATA%\\Programs\\Wio`):\n\n"
                << "```powershell\n"
                << ".\\Install-Wio.ps1\n"
                << "```\n\n"
                << "PowerShell (explicit machine-style target):\n\n"
                << "```powershell\n"
                << ".\\Install-Wio.ps1 -InstallRoot \"C:\\Program Files\\Wio\"\n"
                << "```\n\n"
                << "POSIX shell (default user install under `$HOME/.local/share/wio`):\n\n"
                << "```sh\n"
                << "sh ./install-wio.sh\n"
                << "```\n\n"
                << "Portable use without copying into a stable location is still supported, but it should be treated as a portable workflow rather than a normal installed toolchain.\n\n"
                << "## 2.5. Bundled backend compiler\n\n"
                << "Windows release packages also carry a portable GNU backend toolchain under:\n\n"
                << "```text\n"
                << "toolchains/windows-x64-mingw/\n"
                << "```\n\n"
                << "This is what lets `wio file run test.wio` work on a clean machine without requiring a separate MinGW install.\n\n"
                << "## 3. Uninstall\n\n"
                << "PowerShell:\n\n"
                << "```powershell\n"
                << "C:\\Users\\<you>\\AppData\\Local\\Programs\\Wio\\Uninstall-Wio.ps1\n"
                << "```\n\n"
                << "POSIX shell:\n\n"
                << "```sh\n"
                << "~/.local/share/wio/uninstall-wio.sh\n"
                << "```\n\n"
                << "## 4. Create and run a project\n\n"
                << "```powershell\n"
                << "wio project new MyGame --output-dir C:\\Projects --template wio-app\n"
                << "wio project build --project C:\\Projects\\MyGame\n"
                << "wio project run --project C:\\Projects\\MyGame\n"
                << "```\n\n"
                << "## 5. Useful references\n\n"
                << "- `README.md`\n"
                << "- `docs/README.md`\n"
                << "- `docs/WIO_PROJECT_SYSTEM.md`\n"
                << "- `docs/WIO_LANGUAGE_DRAFT.md`\n";

            return stream.str();
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
                const std::string innoCompiler = parser.GetValuesOf<std::string>("INNO-COMPILER").front();
                const bool explicitVisualInstaller = getFlagValue(parser, "VISUAL-INSTALLER");
                const bool noVisualInstaller = getFlagValue(parser, "NO-VISUAL-INSTALLER");
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
                const std::filesystem::path docsIndexPath = *repoRoot / "docs" / "README.md";
                const std::filesystem::path projectSystemPath = *repoRoot / "docs" / "WIO_PROJECT_SYSTEM.md";

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

#if defined(_WIN32)
                const std::filesystem::path bundledBackendRoot = copyPortableBackendToolchain(packageRoot);
#else
                const std::filesystem::path bundledBackendRoot;
#endif

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
                if (std::filesystem::exists(docsIndexPath, ec))
                {
                    std::filesystem::create_directories((packageRoot / "docs"), ec);
                    ec.clear();
                    std::filesystem::copy_file(docsIndexPath,
                                               packageRoot / "docs" / "README.md",
                                               std::filesystem::copy_options::overwrite_existing,
                                               ec);
                }
                ec.clear();
                if (std::filesystem::exists(projectSystemPath, ec))
                {
                    std::filesystem::create_directories((packageRoot / "docs"), ec);
                    ec.clear();
                    std::filesystem::copy_file(projectSystemPath,
                                               packageRoot / "docs" / "WIO_PROJECT_SYSTEM.md",
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
                    << "  \"bundledBackendRoot\": " << quoteJson(bundledBackendRoot.empty() ? "" : bundledBackendRoot.string()) << ",\n"
                    << "  \"generatedAtUtc\": " << quoteJson(currentUtcIso8601()) << "\n"
                    << "}\n";

                writeUtf8File(packageRoot / "WIO_PACKAGE_INFO.json", packageInfo.str());
                writeUtf8File(packageRoot / "Install-Wio.ps1", buildPowerShellInstallScript());
                writeUtf8File(packageRoot / "Uninstall-Wio.ps1", buildPowerShellUninstallScript());
                writeUtf8File(packageRoot / "install-wio.sh", buildShellInstallScript());
                writeUtf8File(packageRoot / "uninstall-wio.sh", buildShellUninstallScript());
                writeUtf8File(packageRoot / "QUICKSTART.md", buildPackageQuickstart(packageName));

                std::filesystem::path bootstrapInstallerPath;
                std::filesystem::path visualInstallerPath;
                if (platformTag == "windows")
                {
                    bootstrapInstallerPath = outputDir / (packageName + "-installer.ps1");
                    writeUtf8File(bootstrapInstallerPath, buildWindowsBootstrapInstallerScript(packageName));
                }

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

#if defined(_WIN32)
                const bool tryVisualInstaller = platformTag == "windows" && !noVisualInstaller;
                if (tryVisualInstaller)
                {
                    const std::filesystem::path visualInstallerScript = *repoRoot / "tools" / "Build-WioVisualInstaller.ps1";
                    const bool scriptExists = std::filesystem::exists(visualInstallerScript, ec);
                    ec.clear();

                    if (scriptExists)
                    {
                        std::vector<std::string> visualInstallerCommand{
                            "powershell",
                            "-ExecutionPolicy",
                            "Bypass",
                            "-File",
                            visualInstallerScript.string(),
                            "-Version",
                            version,
                            "-PackageRoot",
                            packageRoot.string(),
                            "-OutputDir",
                            outputDir.string()
                        };

                        if (!innoCompiler.empty())
                        {
                            visualInstallerCommand.push_back("-InnoCompiler");
                            visualInstallerCommand.push_back(innoCompiler);
                        }

                        const int visualInstallerResult = runShellCommand(visualInstallerCommand, *repoRoot);
                        if (visualInstallerResult != 0)
                        {
                            if (explicitVisualInstaller)
                                return visualInstallerResult;

                            std::cerr << "Warning: visual installer build was skipped because the installer toolchain failed.\n";
                        }
                        else
                        {
                            visualInstallerPath = outputDir / ("WioSetup-" + version + "-windows-x64.exe");
                        }
                    }
                    else if (explicitVisualInstaller)
                    {
                        throw std::runtime_error("Could not find tools/Build-WioVisualInstaller.ps1 for '--visual-installer'.");
                    }
                }
#endif

                std::cout << "Wio package root : " << packageRoot.string() << '\n';
                if (!noZip)
                    std::cout << "Wio package zip  : " << archivePath.string() << '\n';
                if (!bootstrapInstallerPath.empty())
                    std::cout << "Wio installer    : " << bootstrapInstallerPath.string() << '\n';
                if (!visualInstallerPath.empty())
                    std::cout << "Wio setup exe    : " << visualInstallerPath.string() << '\n';
                std::cout << "Next steps:\n";
                std::cout << "  1. Open " << (packageRoot / "QUICKSTART.md").string() << '\n';
                if (!visualInstallerPath.empty())
                    std::cout << "  2. Upload " << visualInstallerPath.string() << " as the primary Windows installer\n";
                else if (!bootstrapInstallerPath.empty())
                    std::cout << "  2. Run " << bootstrapInstallerPath.string() << '\n';
                else
                    std::cout << "  2. Run " << (packageRoot / "Install-Wio.ps1").string() << '\n';
                std::cout << "  3. Portable use stays available via " << (packageRoot / "bin" / "wio.exe").string() << '\n';

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
