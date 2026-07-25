#include "tooling_cli.h"
#include "binding_cli.h"
#include "cli_common.h"
#include "env_cli.h"
#include "file_cli.h"
#include "package_cli.h"
#include "perf_cli.h"
#include "process_cli.h"

#include <argonaut.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace wio::tooling
{
    namespace
    {
        constexpr std::string_view kWioCliVersion = WIO_VERSION;

        bool isHelpToken(const std::string_view value)
        {
            return cli::IsHelpToken(value);
        }

        bool isVersionToken(const std::string_view value)
        {
            return cli::IsVersionToken(value);
        }

        std::optional<std::filesystem::path> tryResolveExecutableOnPath(const std::string& executableName)
        {
            if (executableName.empty())
                return std::nullopt;

#if defined(_WIN32)
            std::vector<char> buffer(MAX_PATH, '\0');
            const DWORD copied = SearchPathA(nullptr, executableName.c_str(), nullptr, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
            if (copied > 0 && copied < buffer.size())
                return std::filesystem::path(buffer.data()).make_preferred();

            if (!std::filesystem::path(executableName).has_extension())
            {
                const std::string withExe = executableName + ".exe";
                const DWORD copiedWithExe = SearchPathA(nullptr, withExe.c_str(), nullptr, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
                if (copiedWithExe > 0 && copiedWithExe < buffer.size())
                    return std::filesystem::path(buffer.data()).make_preferred();
            }

            if (const char* pathValue = std::getenv("Path"))
            {
                std::stringstream stream(pathValue);
                std::string segment;
                while (std::getline(stream, segment, ';'))
                {
                    if (segment.empty())
                        continue;

                    std::error_code ec;
                    std::filesystem::path candidate = std::filesystem::path(segment) / executableName;
                    if (std::filesystem::exists(candidate, ec) && !ec &&
                        std::filesystem::is_regular_file(candidate, ec) && !ec)
                    {
                        return std::filesystem::absolute(candidate).make_preferred();
                    }

                    if (!std::filesystem::path(executableName).has_extension())
                    {
                        candidate = std::filesystem::path(segment) / (executableName + ".exe");
                        ec.clear();
                        if (std::filesystem::exists(candidate, ec) && !ec &&
                            std::filesystem::is_regular_file(candidate, ec) && !ec)
                        {
                            return std::filesystem::absolute(candidate).make_preferred();
                        }
                    }
                }
            }
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

        void printToolingUsage()
        {
            std::cout
                << "Wio command line interface\n"
                << "\n"
                << "Usage:\n"
                << "\n"
                << "  wio help      [COMMAND [SUBCOMMAND]]\n"
                << "  wio run       [PROJECT] [project options...] [-- application args...]\n"
                << "  wio build     [--build-dir DIR] [--config CFG] [--configure] [--test]\n"
                << "  wio test      [--build-dir DIR] [--config CFG] [--filter REGEX] [--list] [--configure]\n"
                << "  wio file run    [FILE] [compiler args...] [-- program args...]\n"
                << "  wio file check  [FILE] [extra compiler args...]\n"
                << "  wio file tokens [FILE] [extra compiler args...]\n"
                << "  wio file ast    [FILE] [extra compiler args...]\n"
                << "  wio project new <NAME> [--output-dir DIR] [--template NAME] [--force]\n"
                << "  wio project describe [PROJECT] [--project PATH] [--config CFG] [--build-dir DIR]\n"
                << "  wio project build    [PROJECT] [--project PATH] [--config CFG] [--build-dir DIR] [--rebuild]\n"
                << "  wio project run      [PROJECT] [--project PATH] [--config CFG] [--build-dir DIR] [--no-build] [-- application args...]\n"
                << "  wio bind new         --manifest FILE [--output FILE]\n"
                << "  wio bind import      --header FILE --realm NAME [--output FILE] [--header-include FILE] [--prefer-flagset]\n"
                << "  wio env print        [--wio-root DIR] [--shell powershell|cmd|sh] [--add-path]\n"
                << "  wio env setup        [--wio-root DIR] [--set-user] [--no-prompt] [--add-path]\n"
                << "  wio env status       [--wio-root DIR]\n"
                << "  wio env remove       [--wio-root DIR] [--shell powershell|cmd|sh] [--set-user] [--no-prompt] [--remove-path]\n"
                << "  wio env doctor       [--wio-root DIR]\n"
                << "  wio package          [--build-dir DIR] [--config CFG] [--output-dir DIR] [--version-suffix TAG] [--generator NAME] [--no-zip] [--clean]\n"
                << "  wio perf smoke       [--iterations N] [--scratch-dir DIR] [--keep-scratch]\n"
                << "\n"
                << "Alias forms:\n"
                << "\n"
                << "  wio dev build [--build-dir DIR] [--config CFG] [--configure] [--test]\n"
                << "  wio dev test  [--build-dir DIR] [--config CFG] [--filter REGEX] [--list] [--configure]\n"
                << "\n"
                << "Project commands discover wio.makewio in the current directory or its ancestors.\n"
                << "Use '<command> --help' for command-specific options.\n";
        }

        void printProjectUsage(std::ostream& stream)
        {
            stream
                << "Wio project commands\n"
                << "\n"
                << "Usage:\n"
                << "  wio project new <NAME> [--output-dir DIR] [--template NAME] [--force]\n"
                << "  wio project describe [PROJECT] [--config CFG] [--build-dir DIR]\n"
                << "  wio project build    [PROJECT] [--config CFG] [--build-dir DIR] [--rebuild]\n"
                << "  wio project run      [PROJECT] [--config CFG] [--build-dir DIR] [--no-build] [-- application args...]\n"
                << "\n"
                << "PROJECT may be a directory or a makewio manifest. When omitted, Wio searches\n"
                << "the current directory and its ancestors for wio.makewio or makewio.\n";
        }

        std::filesystem::path resolveBuildDir(const std::filesystem::path& repoRoot, const std::string& buildDir)
        {
            std::filesystem::path rawPath = buildDir;
            if (rawPath.is_absolute())
                return rawPath.make_preferred();
            return std::filesystem::absolute(repoRoot / rawPath).make_preferred();
        }

        std::string toSafeIdentifier(std::string value)
        {
            std::string safe;
            safe.reserve(value.size());

            for (const unsigned char ch : value)
            {
                if (std::isalnum(ch) != 0)
                    safe.push_back(static_cast<char>(std::tolower(ch)));
                else
                    safe.push_back('_');
            }

            while (!safe.empty() && safe.front() == '_')
                safe.erase(safe.begin());
            while (!safe.empty() && safe.back() == '_')
                safe.pop_back();

            if (safe.empty())
                return "wio_project";

            std::string collapsed;
            collapsed.reserve(safe.size());
            bool lastWasUnderscore = false;
            for (const char ch : safe)
            {
                if (ch == '_')
                {
                    if (!lastWasUnderscore)
                        collapsed.push_back(ch);
                    lastWasUnderscore = true;
                }
                else
                {
                    collapsed.push_back(ch);
                    lastWasUnderscore = false;
                }
            }

            return collapsed.empty() ? "wio_project" : collapsed;
        }

        void writeUtf8File(const std::filesystem::path& path, const std::string& content)
        {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);

            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
                throw std::runtime_error("Could not open file for writing: " + path.string());

            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!stream.good())
                throw std::runtime_error("Could not write file: " + path.string());
        }

        std::string escapeManifestString(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 4);
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
                default:
                    escaped.push_back(ch);
                    break;
                }
            }
            return escaped;
        }

        std::string quoteManifestString(const std::string& value)
        {
            return "\"" + escapeManifestString(value) + "\"";
        }

        std::string formatManifestArray(const std::vector<std::string>& values)
        {
            std::ostringstream stream;
            stream << '[';
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << quoteManifestString(values[i]);
            }
            stream << ']';
            return stream.str();
        }

        struct ProjectTemplateSpec
        {
            std::string wioTarget = "shared";
            bool hostEnabled = true;
            bool runPassLibraryPath = true;
            std::string wioEntry = "wio/module.wio";
            std::string wioSource;
            std::string hostSource;
            std::string nativeHeaderName;
            std::string nativeHeaderContent;
            std::string nativeSourceName;
            std::string nativeSourceContent;
            std::string description;
        };

        std::optional<ProjectTemplateSpec> tryMakeTemplateSpec(const std::string& templateName)
        {
            ProjectTemplateSpec spec;

            if (templateName == "hybrid-module")
            {
                spec.description = "Shared Wio module plus native C++ host.";
                spec.wioSource = R"(use std::console as console;

mut gCounter: i32 = 0;

@ModuleApiVersion
fn RuntimeAbi() -> u32 {
    return 1;
}

@ModuleLoad
fn BootModule() -> i32 {
    gCounter = 10;
    console::Print!("BootModule");
    return 0;
}

@ModuleUpdate
fn TickModule(deltaTime: f32) {
    gCounter += deltaTime fit i32;
}

@ModuleUnload
fn StopModule() {
    console::Print!("StopModule");
}

@Export
@Command("counter.get")
@CppName(WioGetCounter)
fn GetCounter() -> i32 {
    return gCounter;
}

@Export
@Command("counter.add")
@CppName(WioAddToCounter)
fn AddToCounter(delta: i32) -> i32 {
    gCounter += delta;
    return gCounter;
}

@Export
@Event("game.tick")
@CppName(WioApplyScriptTick)
fn ApplyScriptTick(deltaTime: f32) {
    gCounter += deltaTime fit i32;
}
)";

                spec.hostSource = R"(#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Expected a Wio module library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        module.update(1.0f);

        auto getCounter = module.load_command<std::int32_t()>("counter.get");
        auto addCounter = module.load_command<std::int32_t(std::int32_t)>("counter.add");
        auto broadcastTick = module.load_event<void(float)>("game.tick");

        const std::int32_t before = getCounter();
        const std::int32_t afterAdd = addCounter(5);
        broadcastTick(2.0f);
        const std::int32_t afterTick = getCounter();

        std::cout << "Hybrid project: before=" << before
                  << " afterAdd=" << afterAdd
                  << " afterTick=" << afterTick
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
)";
                return spec;
            }

            if (templateName == "wio-app")
            {
                spec.wioTarget = "exe";
                spec.hostEnabled = false;
                spec.runPassLibraryPath = false;
                spec.description = "Plain Wio executable.";
                spec.wioSource = R"(use std::console as console;

fn Entry(args: string[]) -> i32 {
    console::Print!("Hello from a plain Wio application.");
    if (args.Count() > 1usize) {
        let applicationArguments = args.Skip(1usize).Join("|");
        console::Print!($"Application arguments: ${applicationArguments}");
    }
    return 0;
}
)";
                return spec;
            }

            if (templateName == "wio-native-app")
            {
                spec.wioTarget = "exe";
                spec.hostEnabled = false;
                spec.runPassLibraryPath = false;
                spec.description = "Plain Wio executable plus an example native bridge.";
                spec.wioSource = R"(use std::console as console;

realm ffi {
    @Native
    @CppHeader("native_math.h")
    @CppName(native_math::Multiply)
    fn Multiply(lhs: i32, rhs: i32) -> i32;
}

fn Entry(args: string[]) -> i32 {
    console::Print!("Native multiply:");
    console::Print!(ffi::Multiply(6, 7));
    if (args.Count() > 1usize) {
        let applicationArguments = args.Skip(1usize).Join("|");
        console::Print!($"Application arguments: ${applicationArguments}");
    }
    return 0;
}
)";

                spec.nativeHeaderName = "native_math.h";
                spec.nativeHeaderContent = R"(#pragma once

namespace native_math
{
    int Multiply(int lhs, int rhs);
}
)";

                spec.nativeSourceName = "native_math.cpp";
                spec.nativeSourceContent = R"(#include "native_math.h"

namespace native_math
{
    int Multiply(int lhs, int rhs)
    {
        return lhs * rhs;
    }
}
)";
                return spec;
            }

            if (templateName == "wio-module")
            {
                spec.description = "Exported Wio shared module without a host.";
                spec.hostEnabled = false;
                spec.runPassLibraryPath = false;
                spec.wioSource = R"(@Export
@CppName(WioAddNumbers)
fn AddNumbers(lhs: i32, rhs: i32) -> i32 {
    return lhs + rhs;
}
)";
                return spec;
            }

            return std::nullopt;
        }

        std::string renderMakeWioManifest(const std::string& name,
                                          const std::string& templateName,
                                          const std::string& safeName,
                                          const ProjectTemplateSpec& spec)
        {
            std::ostringstream stream;
            stream
                << "schemaVersion = 1\n"
                << "name = " << quoteManifestString(name) << "\n"
                << "template = " << quoteManifestString(templateName) << "\n\n"
                << "[toolchain]\n"
                << "buildDir = \"build\"\n"
                << "config = \"Debug\"\n\n"
                << "[wio]\n"
                << "entry = " << quoteManifestString(spec.wioEntry) << "\n"
                << "target = " << quoteManifestString(spec.wioTarget) << "\n"
                << "sourceRoots = " << formatManifestArray({ "wio" }) << "\n"
                << "usePaths = " << formatManifestArray({ "wio" }) << "\n"
                << "includeDirs = " << formatManifestArray({ "native/include" }) << "\n"
                << "linkDirs = " << formatManifestArray({ "native/lib" }) << "\n"
                << "linkLibraries = []\n"
                << "backendArgs = []\n\n"
                << "[host]\n"
                << "enabled = " << (spec.hostEnabled ? "true" : "false") << "\n"
                << "compiler = \"g++\"\n"
                << "sourceRoots = " << formatManifestArray({ "host" }) << "\n"
                << "includeDirs = " << formatManifestArray({ "host/include", "native/include" }) << "\n"
                << "linkDirs = " << formatManifestArray({ "native/lib" }) << "\n"
                << "linkLibraries = []\n"
                << "compilerArgs = []\n\n"
                << "[build]\n"
                << "buildDir = \".wio-build\"\n"
                << "config = \"Debug\"\n\n"
                << "[outputs]\n"
                << "directory = \".wio-build/interop\"\n"
                << "baseName = " << quoteManifestString(safeName) << "\n"
                << "wioName = " << quoteManifestString(safeName) << "\n"
                << "hostName = " << quoteManifestString(safeName + ".host") << "\n\n"
                << "[run]\n"
                << "passLibraryPath = " << (spec.runPassLibraryPath ? "true" : "false") << "\n"
                << "args = []\n"
                << "workingDirectory = \".\"\n";
            return stream.str();
        }

        std::string renderCMakeLists(const std::string& safeName)
        {
            std::ostringstream stream;
            stream
                << "cmake_minimum_required(VERSION 3.24)\n"
                << "project(" << safeName << " LANGUAGES NONE)\n\n"
                << "set(WIO_ROOT \"$ENV{WIO_ROOT}\" CACHE PATH \"Path to the Wio toolchain root\")\n"
                << "if(NOT WIO_ROOT)\n"
                << "    message(FATAL_ERROR \"Set WIO_ROOT to the Wio toolchain root before configuring this project.\")\n"
                << "endif()\n\n"
                << "if(NOT EXISTS \"${WIO_ROOT}/cmake/WioProject.cmake\")\n"
                << "    message(FATAL_ERROR \"WioProject.cmake was not found under WIO_ROOT='${WIO_ROOT}'.\")\n"
                << "endif()\n\n"
                << "include(\"${WIO_ROOT}/cmake/WioProject.cmake\")\n\n"
                << "wio_add_project_targets(" << safeName << "\n"
                << "    WIO_ROOT \"${WIO_ROOT}\"\n"
                << ")\n";
            return stream.str();
        }

        std::string renderReadme(const std::string& name, const std::string& templateName, const ProjectTemplateSpec& spec)
        {
            std::ostringstream stream;
            stream
                << "# " << name << "\n\n"
                << "Template:\n\n"
                << "- `" << templateName << "`\n\n"
                << "Description:\n\n"
                << "- " << spec.description << "\n\n"
                << "## Files\n\n"
                << "- `wio/`: Wio source files\n"
                << "- `host/`: optional host-side C++ code\n"
                << "- `native/include/`: headers visible to `@CppHeader(...)`\n"
                << "- `native/src/`: extra native sources compiled into the Wio target\n"
                << "- `native/lib/`: prebuilt native libraries referenced by the manifest\n"
                << "- `wio.makewio`: primary Wio project manifest\n"
                << "- `CMakeLists.txt`: optional CMake integration through `WioProject.cmake`\n\n"
                << "## Current Workflow\n\n"
                << "This project was generated by `wio project new`.\n\n"
                << "From this directory, the direct project workflow is:\n\n"
                << "```powershell\n"
                << "wio project describe\n"
                << "wio project build\n"
                << "wio project run\n"
                << "wio project run -- first-argument \"two words\"\n"
                << "```\n\n"
                << "`wio project run` discovers this manifest from the current directory or a child directory.\n"
                << "Arguments after `--` are passed to the generated executable without reinterpretation.\n";
            return stream.str();
        }

        std::string renderGitIgnore()
        {
            return ".wio-build/\nbuild/\ncmake-build*/\n";
        }

        std::string renderNativeReadme()
        {
            return
                "Place user-native integration files here.\n\n"
                "- `include/`: headers for `@CppHeader(...)`\n"
                "- `src/`: extra native `.c/.cc/.cpp/.cxx` files compiled with the Wio target\n"
                "- `lib/`: prebuilt native libraries referenced from the manifest\n";
        }

        std::string trim(std::string_view value)
        {
            const size_t begin = value.find_first_not_of(" \t\r\n");
            if (begin == std::string_view::npos)
                return {};

            const size_t end = value.find_last_not_of(" \t\r\n");
            return std::string(value.substr(begin, end - begin + 1));
        }

        std::string stripMakeWioComment(std::string_view value)
        {
            std::string result;
            result.reserve(value.size());

            char quote = '\0';
            bool escape = false;

            for (size_t index = 0; index < value.size(); ++index)
            {
                const char current = value[index];

                if (escape)
                {
                    result.push_back(current);
                    escape = false;
                    continue;
                }

                if (quote != '\0')
                {
                    result.push_back(current);
                    if (current == '\\')
                        escape = true;
                    else if (current == quote)
                        quote = '\0';
                    continue;
                }

                if (current == '"' || current == '\'')
                {
                    quote = current;
                    result.push_back(current);
                    continue;
                }

                if (current == '#')
                    break;

                if (current == '/' && (index + 1) < value.size() && value[index + 1] == '/')
                    break;

                result.push_back(current);
            }

            return trim(result);
        }

        std::string unescapeQuoted(std::string_view value)
        {
            std::string result;
            result.reserve(value.size());

            bool escape = false;
            for (const char ch : value)
            {
                if (!escape)
                {
                    if (ch == '\\')
                    {
                        escape = true;
                        continue;
                    }

                    result.push_back(ch);
                    continue;
                }

                switch (ch)
                {
                case 'n':
                    result.push_back('\n');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                case '"':
                    result.push_back('"');
                    break;
                case '\'':
                    result.push_back('\'');
                    break;
                default:
                    result.push_back(ch);
                    break;
                }

                escape = false;
            }

            return result;
        }

        std::string parseScalarValue(std::string_view value)
        {
            const std::string trimmed = trim(value);
            if (trimmed.size() >= 2 &&
                ((trimmed.front() == '"' && trimmed.back() == '"') ||
                 (trimmed.front() == '\'' && trimmed.back() == '\'')))
            {
                return unescapeQuoted(std::string_view(trimmed).substr(1, trimmed.size() - 2));
            }

            return trimmed;
        }

        std::vector<std::string> parseArrayValue(std::string_view value)
        {
            std::string trimmed = trim(value);
            if (trimmed == "[]")
                return {};

            if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
                return { parseScalarValue(trimmed) };

            trimmed = trim(std::string_view(trimmed).substr(1, trimmed.size() - 2));
            if (trimmed.empty())
                return {};

            std::vector<std::string> values;
            std::string current;
            char quote = '\0';
            bool escape = false;

            for (const char ch : trimmed)
            {
                if (escape)
                {
                    current.push_back(ch);
                    escape = false;
                    continue;
                }

                if (quote != '\0')
                {
                    current.push_back(ch);
                    if (ch == '\\')
                        escape = true;
                    else if (ch == quote)
                        quote = '\0';
                    continue;
                }

                if (ch == '"' || ch == '\'')
                {
                    quote = ch;
                    current.push_back(ch);
                    continue;
                }

                if (ch == ',')
                {
                    values.push_back(parseScalarValue(current));
                    current.clear();
                    continue;
                }

                current.push_back(ch);
            }

            if (!trim(current).empty())
                values.push_back(parseScalarValue(current));

            return values;
        }

        using ManifestMap = std::map<std::string, std::string>;

        ManifestMap parseMakeWioManifest(const std::filesystem::path& manifestFile)
        {
            std::ifstream stream(manifestFile, std::ios::binary);
            if (!stream.is_open())
                throw std::runtime_error("Could not open manifest: " + manifestFile.string());

            ManifestMap values;
            std::string currentSection;
            std::string rawLine;
            size_t lineNumber = 0;

            while (std::getline(stream, rawLine))
            {
                ++lineNumber;
                const std::string line = stripMakeWioComment(rawLine);
                if (line.empty())
                    continue;

                if (line.front() == '[' && line.back() == ']')
                {
                    currentSection = trim(std::string_view(line).substr(1, line.size() - 2));
                    continue;
                }

                const size_t equalsIndex = line.find('=');
                if (equalsIndex == std::string::npos)
                    throw std::runtime_error("Invalid makewio syntax at line " + std::to_string(lineNumber) + " in '" + manifestFile.string() + "'.");

                const std::string key = trim(std::string_view(line).substr(0, equalsIndex));
                const std::string value = trim(std::string_view(line).substr(equalsIndex + 1));

                if (key.empty())
                    throw std::runtime_error("Invalid empty key at line " + std::to_string(lineNumber) + " in '" + manifestFile.string() + "'.");

                const std::string fullKey =
                    (!currentSection.empty() && key.find('.') == std::string::npos)
                        ? currentSection + "." + key
                        : key;

                values[fullKey] = value;
            }

            return values;
        }

        bool manifestHas(const ManifestMap& manifest, const std::string& key)
        {
            return manifest.contains(key);
        }

        std::string manifestString(const ManifestMap& manifest, const std::string& key, const std::string& fallback = {})
        {
            if (const auto it = manifest.find(key); it != manifest.end())
                return parseScalarValue(it->second);
            return fallback;
        }

        bool manifestBool(const ManifestMap& manifest, const std::string& key, bool fallback)
        {
            if (const auto it = manifest.find(key); it != manifest.end())
            {
                const std::string normalized = parseScalarValue(it->second);
                if (normalized == "true")
                    return true;
                if (normalized == "false")
                    return false;
            }
            return fallback;
        }

        std::vector<std::string> manifestArray(const ManifestMap& manifest, const std::string& key)
        {
            if (const auto it = manifest.find(key); it != manifest.end())
                return parseArrayValue(it->second);
            return {};
        }

        std::filesystem::path resolveProjectPath(const std::filesystem::path& projectRoot, const std::string& value)
        {
            std::filesystem::path path = value;
            if (path.is_absolute())
                return path.lexically_normal().make_preferred();
            return std::filesystem::absolute(projectRoot / path).lexically_normal().make_preferred();
        }

        void addUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path)
        {
            const std::filesystem::path normalized =
                std::filesystem::absolute(path).lexically_normal().make_preferred();
            for (const auto& existing : paths)
            {
                if (existing == normalized)
                    return;
            }
            paths.push_back(normalized);
        }

        std::vector<std::filesystem::path> existingDirectories(const std::filesystem::path& projectRoot,
                                                               const std::vector<std::string>& candidates)
        {
            std::vector<std::filesystem::path> directories;
            for (const auto& candidate : candidates)
            {
                if (candidate.empty())
                    continue;

                const std::filesystem::path path = resolveProjectPath(projectRoot, candidate);
                std::error_code ec;
                if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec))
                    addUniquePath(directories, path);
            }
            return directories;
        }

        std::vector<std::filesystem::path> explicitFiles(const std::filesystem::path& projectRoot,
                                                         const std::vector<std::string>& files,
                                                         const std::string& label)
        {
            std::vector<std::filesystem::path> resolved;
            for (const auto& file : files)
            {
                if (file.empty())
                    continue;

                const std::filesystem::path path = resolveProjectPath(projectRoot, file);
                std::error_code ec;
                if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec))
                    throw std::runtime_error(label + " file not found: " + path.string());

                addUniquePath(resolved, path);
            }
            return resolved;
        }

        std::vector<std::filesystem::path> filesByExtensions(const std::vector<std::filesystem::path>& directories,
                                                             const std::vector<std::string>& extensions)
        {
            std::vector<std::filesystem::path> files;
            for (const auto& directory : directories)
            {
                std::error_code ec;
                if (!std::filesystem::exists(directory, ec))
                    continue;

                for (std::filesystem::recursive_directory_iterator it(directory, ec), end; it != end && !ec; it.increment(ec))
                {
                    if (ec || !it->is_regular_file())
                        continue;

                    const std::string extension = it->path().extension().string();
                    for (const auto& expected : extensions)
                    {
                        if (extension == expected)
                        {
                            addUniquePath(files, it->path());
                            break;
                        }
                    }
                }
            }
            return files;
        }

        std::filesystem::path resolveProjectManifestFile(const std::string& projectValue)
        {
            std::filesystem::path base =
                projectValue.empty()
                    ? std::filesystem::current_path()
                    : std::filesystem::absolute(std::filesystem::path(projectValue)).make_preferred();

            std::error_code ec;
            if (!std::filesystem::exists(base, ec))
                throw std::runtime_error("Project path does not exist: " + base.string());

            if (std::filesystem::is_regular_file(base, ec))
            {
                const std::string fileName = base.filename().string();
                if (fileName == "wio.project.json")
                    throw std::runtime_error("The direct Wio CLI currently supports makewio manifests. Use wio.makewio or the compatibility script for legacy JSON manifests.");
                return std::filesystem::absolute(base).make_preferred();
            }

            const std::vector<std::string> candidates{ "wio.makewio", "makewio", "wio.project.json" };
            const std::filesystem::path searchStart = std::filesystem::absolute(base).make_preferred();
            base = searchStart;

            while (!base.empty())
            {
                for (const auto& candidate : candidates)
                {
                    const std::filesystem::path manifestPath = base / candidate;
                    ec.clear();
                    if (std::filesystem::exists(manifestPath, ec) && std::filesystem::is_regular_file(manifestPath, ec))
                    {
                        if (candidate == "wio.project.json")
                            throw std::runtime_error("The direct Wio CLI currently supports makewio manifests. Use wio.makewio or the compatibility script for legacy JSON manifests.");
                        return std::filesystem::absolute(manifestPath).make_preferred();
                    }
                }

                const std::filesystem::path parent = base.parent_path();
                if (parent == base)
                    break;
                base = parent;
            }

            throw std::runtime_error(
                "No project manifest was found from '" + searchStart.string() +
                "' upward. Expected wio.makewio or makewio."
            );
        }

        std::filesystem::path getDefaultWioEntry(const std::filesystem::path& projectRoot)
        {
            const std::vector<std::string> preferred{
                "wio/module.wio",
                "wio/main.wio",
                "src/module.wio",
                "src/main.wio",
                "main.wio"
            };

            for (const auto& candidate : preferred)
            {
                const std::filesystem::path path = resolveProjectPath(projectRoot, candidate);
                std::error_code ec;
                if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec))
                    return path;
            }

            const auto roots = existingDirectories(projectRoot, { "wio", "src" });
            auto files = filesByExtensions(roots.empty() ? std::vector<std::filesystem::path>{ projectRoot } : roots, { ".wio" });
            if (files.size() == 1)
                return files.front();

            throw std::runtime_error("Unable to infer the Wio entry file. Set 'wio.entry' explicitly in the manifest.");
        }

        std::filesystem::path currentExecutablePath()
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

        std::optional<std::filesystem::path> environmentToolchainRoot()
        {
            const char* names[] = { "WIO_ROOT", "WIO_HOME" };
            for (const char* name : names)
            {
                const char* value = std::getenv(name);
                if (value == nullptr || *value == '\0')
                    continue;

                const std::filesystem::path path = std::filesystem::absolute(std::filesystem::path(value)).make_preferred();
                std::error_code ec;
                if (std::filesystem::exists(path, ec))
                    return path;
            }
            return std::nullopt;
        }

        bool looksLikeToolchainRoot(const std::filesystem::path& root)
        {
            std::error_code ec;
            return std::filesystem::exists(root / "std", ec) &&
                   std::filesystem::exists(root / "sdk" / "include", ec) &&
                   std::filesystem::exists(root / "runtime" / "include", ec);
        }

        std::optional<std::filesystem::path> findToolchainRoot()
        {
            std::vector<std::filesystem::path> candidates;

            if (const auto env = environmentToolchainRoot(); env.has_value())
                candidates.push_back(*env);

            const std::filesystem::path exePath = currentExecutablePath();
            if (!exePath.empty())
            {
                candidates.push_back(exePath.parent_path());
                candidates.push_back(exePath.parent_path().parent_path());
                candidates.push_back(exePath.parent_path().parent_path().parent_path());
            }

            if (const auto repoRoot = tryFindRepoRoot(); repoRoot.has_value())
                candidates.push_back(*repoRoot);

            for (const auto& candidate : candidates)
            {
                if (!candidate.empty() && looksLikeToolchainRoot(candidate))
                    return std::filesystem::absolute(candidate).make_preferred();
            }

            return std::nullopt;
        }

        bool looksLikeExplicitCommandPath(std::string_view value)
        {
            return value.find('\\') != std::string_view::npos ||
                   value.find('/') != std::string_view::npos;
        }

        bool compilerUsesGnuStyleDriver(const std::filesystem::path& compilerPath)
        {
            std::string lower = compilerPath.filename().string();
            for (char& ch : lower)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            return lower.find("g++") != std::string::npos ||
                   lower.find("gcc") != std::string::npos ||
                   lower.find("clang++") != std::string::npos ||
                   lower.find("clang") != std::string::npos;
        }

        std::filesystem::path getCompileTimeDefaultToolchainRoot()
        {
#ifdef WIO_DEFAULT_ROOT_DIR
            return std::filesystem::path(WIO_DEFAULT_ROOT_DIR).make_preferred();
#else
            return {};
#endif
        }

        std::vector<std::filesystem::path> getBundledToolchainRootCandidates(const std::filesystem::path& preferredRoot)
        {
            std::vector<std::filesystem::path> candidates;

            auto appendCandidate = [&](const std::filesystem::path& candidate)
            {
                if (candidate.empty())
                    return;

                const std::filesystem::path absoluteCandidate = std::filesystem::absolute(candidate).make_preferred();
                for (const auto& existing : candidates)
                {
                    if (existing == absoluteCandidate)
                        return;
                }

                candidates.push_back(absoluteCandidate);
            };

            appendCandidate(preferredRoot);

            if (const auto envRoot = environmentToolchainRoot(); envRoot.has_value())
                appendCandidate(*envRoot);

            const std::filesystem::path executablePath = currentExecutablePath();
            if (!executablePath.empty())
            {
                const std::filesystem::path executableDir = executablePath.parent_path();
                appendCandidate(executableDir);
                appendCandidate(executableDir.parent_path());
                appendCandidate(executableDir.parent_path().parent_path());
            }

            appendCandidate(getCompileTimeDefaultToolchainRoot());

#if defined(_WIN32)
            if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData != nullptr && *localAppData != '\0')
                appendCandidate(std::filesystem::path(localAppData) / "Programs" / "Wio");

            if (const char* userProfile = std::getenv("USERPROFILE"); userProfile != nullptr && *userProfile != '\0')
                appendCandidate(std::filesystem::path(userProfile) / "AppData" / "Local" / "Programs" / "Wio");
#endif

            return candidates;
        }

        std::optional<std::filesystem::path> tryResolveBundledBackendCompiler(const std::filesystem::path& toolchainRoot,
                                                                              std::string_view configuredCompiler)
        {
            const size_t start = configuredCompiler.find_first_not_of(" \t\r\n");
            if (start == std::string_view::npos)
                return std::nullopt;

            const size_t end = configuredCompiler.find_last_not_of(" \t\r\n");
            const std::string compilerName(configuredCompiler.substr(start, end - start + 1));
            if (compilerName.empty())
                return std::nullopt;

            auto tryRelative = [&](const std::filesystem::path& relativePath) -> std::optional<std::filesystem::path>
            {
                for (const auto& rootCandidate : getBundledToolchainRootCandidates(toolchainRoot))
                {
                    const std::filesystem::path candidate = std::filesystem::absolute(rootCandidate / relativePath).make_preferred();
                    std::error_code ec;
                    if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
                        return candidate;
                }
                return std::nullopt;
            };

            const bool prefersBundledGnu =
                compilerName == "bundled" ||
                compilerName == "g++" ||
                compilerName == "gcc" ||
                compilerName == "c++";

#if defined(_WIN32)
            if (prefersBundledGnu)
            {
                if (auto bundled = tryRelative(std::filesystem::path("toolchains") / "windows-x64-mingw" / "bin" / "g++.exe"); bundled.has_value())
                    return bundled;
            }
#else
            if (prefersBundledGnu)
            {
                if (auto bundled = tryRelative(std::filesystem::path("toolchains") / "host-backend" / "bin" / "g++"); bundled.has_value())
                    return bundled;
            }
#endif

            if (compilerName == "bundled")
                return std::nullopt;

            if (looksLikeExplicitCommandPath(compilerName))
                return std::nullopt;

            return tryResolveExecutableOnPath(compilerName);
        }

        std::filesystem::path findRuntimeStaticLibrary(const std::filesystem::path& toolchainRoot,
                                                       const std::string& toolchainBuildDir)
        {
            const std::vector<std::filesystem::path> candidates{
                toolchainRoot / "runtime" / "lib" / "libwio_runtime.a",
                toolchainRoot / toolchainBuildDir / "runtime" / "backend" / "libwio_runtime.a",
                toolchainRoot / toolchainBuildDir / "runtime" / "libwio_runtime.a"
            };

            std::error_code ec;
            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
                    return std::filesystem::absolute(candidate).make_preferred();
            }

            return {};
        }

        std::string outputExtension(std::string_view kind)
        {
            if (kind == "shared")
                return ".dll";
            if (kind == "static")
                return ".a";
            if (kind == "exe")
                return ".exe";
            return {};
        }

        bool looksLikeLibraryFile(std::string_view value)
        {
            return value.find('/') != std::string_view::npos ||
                   value.find('\\') != std::string_view::npos ||
                   value.ends_with(".lib") ||
                   value.ends_with(".a") ||
                   value.ends_with(".dll") ||
                   value.ends_with(".so") ||
                   value.ends_with(".dylib") ||
                   value.ends_with(".obj") ||
                   value.ends_with(".o");
        }

        std::vector<std::string> resolveLinkLibraries(const std::filesystem::path& projectRoot,
                                                      const std::vector<std::string>& libraries)
        {
            std::vector<std::string> resolved;
            resolved.reserve(libraries.size());

            for (const auto& library : libraries)
            {
                if (library.empty())
                    continue;

                if (!library.empty() && library.front() == '-')
                {
                    resolved.push_back(library);
                    continue;
                }

                if (looksLikeLibraryFile(library))
                    resolved.push_back(resolveProjectPath(projectRoot, library).string());
                else
                    resolved.push_back(library);
            }

            return resolved;
        }

        std::vector<std::filesystem::path> collectFilesForTimestampCheck(const std::vector<std::filesystem::path>& inputs)
        {
            std::vector<std::filesystem::path> files;
            for (const auto& input : inputs)
            {
                std::error_code ec;
                if (!std::filesystem::exists(input, ec))
                    continue;

                if (std::filesystem::is_regular_file(input, ec))
                {
                    addUniquePath(files, input);
                    continue;
                }

                for (std::filesystem::recursive_directory_iterator it(input, ec), end; it != end && !ec; it.increment(ec))
                {
                    if (!ec && it->is_regular_file())
                        addUniquePath(files, it->path());
                }
            }
            return files;
        }

        bool outputUpToDate(const std::filesystem::path& outputPath, const std::vector<std::filesystem::path>& inputs)
        {
            std::error_code ec;
            if (!std::filesystem::exists(outputPath, ec) || !std::filesystem::is_regular_file(outputPath, ec))
                return false;

            const auto outputTime = std::filesystem::last_write_time(outputPath, ec);
            if (ec)
                return false;

            for (const auto& file : collectFilesForTimestampCheck(inputs))
            {
                const auto inputTime = std::filesystem::last_write_time(file, ec);
                if (ec || inputTime > outputTime)
                    return false;
            }

            return true;
        }

        std::string jsonEscape(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 4);
            for (const char ch : value)
            {
                switch (ch)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped.push_back(ch); break;
                }
            }
            return escaped;
        }

        std::string jsonString(const std::string& value)
        {
            return "\"" + jsonEscape(value) + "\"";
        }

        std::string jsonPathArray(const std::vector<std::filesystem::path>& values)
        {
            std::ostringstream stream;
            stream << '[';
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << jsonString(values[i].string());
            }
            stream << ']';
            return stream.str();
        }

        std::string jsonStringArray(const std::vector<std::string>& values)
        {
            std::ostringstream stream;
            stream << '[';
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << jsonString(values[i]);
            }
            stream << ']';
            return stream.str();
        }

        struct ProjectInfo
        {
            std::filesystem::path manifestFile;
            std::filesystem::path projectRoot;
            std::filesystem::path toolchainRoot;
            std::filesystem::path toolchainExecutable;
            std::filesystem::path projectBuildDirectory;
            std::filesystem::path outputDirectory;
            std::filesystem::path runWorkingDirectory;
            std::filesystem::path sdkIncludeDirectory;
            std::filesystem::path runtimeStaticLibrary;
            std::filesystem::path wioEntry;
            std::filesystem::path wioOutput;
            std::filesystem::path hostOutput;
            std::filesystem::path hostCompilerExecutable;
            std::string name;
            std::string templateName;
            std::string config;
            std::string toolchainBuildDir;
            std::string toolchainConfig;
            std::string wioTarget;
            std::string hostCompiler;
            bool hostEnabled = false;
            bool passLibraryPath = false;
            std::vector<std::filesystem::path> wioSourceRoots;
            std::vector<std::filesystem::path> wioIncludeDirs;
            std::vector<std::filesystem::path> wioLinkDirs;
            std::vector<std::filesystem::path> wioNativeSources;
            std::vector<std::string> wioLinkLibraries;
            std::vector<std::string> wioBackendArgs;
            std::vector<std::filesystem::path> hostSourceFiles;
            std::vector<std::filesystem::path> hostIncludeDirs;
            std::vector<std::filesystem::path> hostLinkDirs;
            std::vector<std::string> hostLinkLibraries;
            std::vector<std::string> hostCompilerArgs;
            std::vector<std::string> runArgs;
        };

        ProjectInfo resolveProjectInfo(const std::string& projectValue,
                                       const std::string& configOverride,
                                       const std::string& buildDirOverride)
        {
            ProjectInfo info;
            info.manifestFile = resolveProjectManifestFile(projectValue);
            info.projectRoot = info.manifestFile.parent_path().make_preferred();

            const ManifestMap manifest = parseMakeWioManifest(info.manifestFile);
            info.name = manifestString(manifest, "name");
            if (info.name.empty())
                throw std::runtime_error("Project manifest is missing a non-empty 'name'.");

            info.templateName = manifestString(manifest, "template");
            info.config = configOverride.empty() ? manifestString(manifest, "build.config", "Debug") : configOverride;
            info.toolchainBuildDir = manifestString(manifest, "toolchain.buildDir", "build");
            info.toolchainConfig = manifestString(manifest, "toolchain.config", info.config);
            const std::string projectBuildDirValue = buildDirOverride.empty() ? manifestString(manifest, "build.buildDir", ".wio-build") : buildDirOverride;
            info.projectBuildDirectory = resolveProjectPath(info.projectRoot, projectBuildDirValue);

            const std::string outputDirectoryValue = manifestString(manifest, "outputs.directory", "");
            info.outputDirectory = outputDirectoryValue.empty()
                ? (info.projectBuildDirectory / "interop").make_preferred()
                : resolveProjectPath(info.projectRoot, outputDirectoryValue);

            info.wioTarget = manifestString(manifest, "wio.target", "shared");
            if (info.wioTarget != "exe" && info.wioTarget != "shared" && info.wioTarget != "static")
                throw std::runtime_error("Unsupported wio.target '" + info.wioTarget + "'. Expected one of: exe, shared, static.");

            const std::string entryValue = manifestString(manifest, "wio.entry", "");
            info.wioEntry = entryValue.empty() ? getDefaultWioEntry(info.projectRoot) : resolveProjectPath(info.projectRoot, entryValue);

            auto sourceRoots = manifestArray(manifest, "wio.sourceRoots");
            if (sourceRoots.empty() && !manifestHas(manifest, "wio.sourceRoots"))
                sourceRoots = manifestArray(manifest, "wio.usePaths");
            if (sourceRoots.empty() && !manifestHas(manifest, "wio.sourceRoots") && !manifestHas(manifest, "wio.usePaths"))
                sourceRoots = manifestArray(manifest, "wio.moduleDirs");

            if (!sourceRoots.empty() || manifestHas(manifest, "wio.sourceRoots") || manifestHas(manifest, "wio.usePaths") || manifestHas(manifest, "wio.moduleDirs"))
            {
                for (const auto& root : sourceRoots)
                    addUniquePath(info.wioSourceRoots, resolveProjectPath(info.projectRoot, root));
            }
            else
            {
                for (const auto& root : existingDirectories(info.projectRoot, { "wio", "src" }))
                    addUniquePath(info.wioSourceRoots, root);
                addUniquePath(info.wioSourceRoots, info.wioEntry.parent_path());
            }

            if (manifestHas(manifest, "wio.includeDirs"))
            {
                for (const auto& dir : manifestArray(manifest, "wio.includeDirs"))
                    addUniquePath(info.wioIncludeDirs, resolveProjectPath(info.projectRoot, dir));
            }
            else
            {
                for (const auto& dir : existingDirectories(info.projectRoot, { "native/include" }))
                    addUniquePath(info.wioIncludeDirs, dir);
            }

            if (manifestHas(manifest, "wio.linkDirs"))
            {
                for (const auto& dir : manifestArray(manifest, "wio.linkDirs"))
                    addUniquePath(info.wioLinkDirs, resolveProjectPath(info.projectRoot, dir));
            }
            else
            {
                for (const auto& dir : existingDirectories(info.projectRoot, { "native/lib" }))
                    addUniquePath(info.wioLinkDirs, dir);
            }

            if (manifestHas(manifest, "wio.nativeSources"))
            {
                info.wioNativeSources = explicitFiles(info.projectRoot, manifestArray(manifest, "wio.nativeSources"), "Wio native source");
            }
            else
            {
                info.wioNativeSources = filesByExtensions(existingDirectories(info.projectRoot, { "native/src" }), { ".c", ".cc", ".cpp", ".cxx" });
            }

            info.wioLinkLibraries = resolveLinkLibraries(info.projectRoot, manifestArray(manifest, "wio.linkLibraries"));
            info.wioBackendArgs = manifestArray(manifest, "wio.backendArgs");

            info.hostEnabled = manifestBool(manifest, "host.enabled", info.templateName == "hybrid-module");
            if (info.wioTarget == "exe" && info.hostEnabled)
                throw std::runtime_error("host.enabled cannot be true when wio.target is 'exe'.");

            info.hostCompiler = manifestString(manifest, "host.compiler", "g++");

            if (info.hostEnabled)
            {
                auto hostSourceValues = manifestArray(manifest, "host.sources");
                if (hostSourceValues.empty() && manifestHas(manifest, "host.source"))
                    hostSourceValues.push_back(manifestString(manifest, "host.source"));

                if (!hostSourceValues.empty() || manifestHas(manifest, "host.sources") || manifestHas(manifest, "host.source"))
                {
                    info.hostSourceFiles = explicitFiles(info.projectRoot, hostSourceValues, "Host source");
                }
                else
                {
                    auto hostRoots = manifestArray(manifest, "host.sourceRoots");
                    std::vector<std::filesystem::path> hostSourceRoots;
                    if (!hostRoots.empty() || manifestHas(manifest, "host.sourceRoots"))
                    {
                        for (const auto& root : hostRoots)
                            addUniquePath(hostSourceRoots, resolveProjectPath(info.projectRoot, root));
                    }
                    else
                    {
                        for (const auto& root : existingDirectories(info.projectRoot, { "host", "host/src" }))
                            addUniquePath(hostSourceRoots, root);
                    }

                    info.hostSourceFiles = filesByExtensions(hostSourceRoots, { ".c", ".cc", ".cpp", ".cxx" });
                }

                if (info.hostSourceFiles.empty())
                    throw std::runtime_error("No host source files were found. Set host.sources or create sources under host/.");
            }

            if (manifestHas(manifest, "host.includeDirs"))
            {
                for (const auto& dir : manifestArray(manifest, "host.includeDirs"))
                    addUniquePath(info.hostIncludeDirs, resolveProjectPath(info.projectRoot, dir));
            }
            else
            {
                for (const auto& dir : existingDirectories(info.projectRoot, { "host/include", "native/include" }))
                    addUniquePath(info.hostIncludeDirs, dir);
            }

            if (manifestHas(manifest, "host.linkDirs"))
            {
                for (const auto& dir : manifestArray(manifest, "host.linkDirs"))
                    addUniquePath(info.hostLinkDirs, resolveProjectPath(info.projectRoot, dir));
            }
            else
            {
                for (const auto& dir : existingDirectories(info.projectRoot, { "native/lib" }))
                    addUniquePath(info.hostLinkDirs, dir);
            }

            info.hostLinkLibraries = resolveLinkLibraries(info.projectRoot, manifestArray(manifest, "host.linkLibraries"));
            info.hostCompilerArgs = manifestArray(manifest, "host.compilerArgs");

            info.passLibraryPath = manifestBool(manifest, "run.passLibraryPath", info.wioTarget == "shared");
            info.runArgs = manifestArray(manifest, "run.args");
            const std::string workingDirectoryValue = manifestString(manifest, "run.workingDirectory", ".");
            info.runWorkingDirectory = resolveProjectPath(info.projectRoot, workingDirectoryValue);

            std::string baseName = manifestString(manifest, "outputs.baseName");
            if (baseName.empty())
                baseName = toSafeIdentifier(info.name);

            std::string wioName = manifestString(manifest, "outputs.wioName");
            if (wioName.empty())
                wioName = baseName;

            std::string hostName = manifestString(manifest, "outputs.hostName");
            if (hostName.empty())
                hostName = baseName + ".host";

            info.wioOutput = info.outputDirectory / (wioName + outputExtension(info.wioTarget));
            info.hostOutput = info.outputDirectory / (hostName + outputExtension("exe"));

            info.toolchainExecutable = currentExecutablePath();
            if (info.toolchainExecutable.empty())
                throw std::runtime_error("Could not resolve the current Wio executable path.");

            const auto toolchainRoot = findToolchainRoot();
            if (!toolchainRoot.has_value())
                throw std::runtime_error("Could not resolve the Wio toolchain root. Set WIO_ROOT or run from a packaged/source toolchain layout.");

            info.toolchainRoot = *toolchainRoot;
            info.sdkIncludeDirectory = info.toolchainRoot / "sdk" / "include";
            info.runtimeStaticLibrary = findRuntimeStaticLibrary(info.toolchainRoot, info.toolchainBuildDir);

            if (looksLikeExplicitCommandPath(info.hostCompiler))
            {
                info.hostCompilerExecutable = resolveProjectPath(info.projectRoot, info.hostCompiler);
            }
            else if (auto resolvedHostCompiler = tryResolveBundledBackendCompiler(info.toolchainRoot, info.hostCompiler); resolvedHostCompiler.has_value())
            {
                info.hostCompilerExecutable = *resolvedHostCompiler;
            }

            if (info.hostEnabled)
            {
                if (info.hostCompilerExecutable.empty())
                {
                    throw std::runtime_error(
                        "Could not resolve the host compiler '" + info.hostCompiler +
                        "'. Ensure it is available on PATH or bundled with the Wio toolchain."
                    );
                }

                std::error_code compilerEc;
                if (!std::filesystem::exists(info.hostCompilerExecutable, compilerEc) ||
                    !std::filesystem::is_regular_file(info.hostCompilerExecutable, compilerEc))
                {
                    throw std::runtime_error("Resolved host compiler does not exist: " + info.hostCompilerExecutable.string());
                }

                if (!compilerUsesGnuStyleDriver(info.hostCompilerExecutable))
                {
                    throw std::runtime_error(
                        "The current direct project host build path expects a GNU-style C++ driver (g++, gcc, or clang++). "
                        "Resolved host compiler: " + info.hostCompilerExecutable.string()
                    );
                }
            }

            return info;
        }

        void printProjectInfoJson(const ProjectInfo& info)
        {
            std::cout
                << "{\n"
                << "  \"manifestFile\": " << jsonString(info.manifestFile.string()) << ",\n"
                << "  \"projectRoot\": " << jsonString(info.projectRoot.string()) << ",\n"
                << "  \"name\": " << jsonString(info.name) << ",\n"
                << "  \"template\": " << jsonString(info.templateName) << ",\n"
                << "  \"config\": " << jsonString(info.config) << ",\n"
                << "  \"toolchainRoot\": " << jsonString(info.toolchainRoot.string()) << ",\n"
                << "  \"buildDirectory\": " << jsonString(info.projectBuildDirectory.string()) << ",\n"
                << "  \"outputDirectory\": " << jsonString(info.outputDirectory.string()) << ",\n"
                << "  \"wio\": {\n"
                << "    \"entry\": " << jsonString(info.wioEntry.string()) << ",\n"
                << "    \"target\": " << jsonString(info.wioTarget) << ",\n"
                << "    \"output\": " << jsonString(info.wioOutput.string()) << ",\n"
                << "    \"sourceRoots\": " << jsonPathArray(info.wioSourceRoots) << ",\n"
                << "    \"includeDirs\": " << jsonPathArray(info.wioIncludeDirs) << ",\n"
                << "    \"linkDirs\": " << jsonPathArray(info.wioLinkDirs) << ",\n"
                << "    \"nativeSources\": " << jsonPathArray(info.wioNativeSources) << ",\n"
                << "    \"linkLibraries\": " << jsonStringArray(info.wioLinkLibraries) << "\n"
                << "  },\n"
                << "  \"host\": {\n"
                << "    \"enabled\": " << (info.hostEnabled ? "true" : "false") << ",\n"
                << "    \"compiler\": " << jsonString(info.hostCompiler) << ",\n"
                << "    \"resolvedCompiler\": " << jsonString(info.hostCompilerExecutable.string()) << ",\n"
                << "    \"output\": " << jsonString(info.hostOutput.string()) << ",\n"
                << "    \"sourceFiles\": " << jsonPathArray(info.hostSourceFiles) << ",\n"
                << "    \"includeDirs\": " << jsonPathArray(info.hostIncludeDirs) << ",\n"
                << "    \"linkDirs\": " << jsonPathArray(info.hostLinkDirs) << ",\n"
                << "    \"linkLibraries\": " << jsonStringArray(info.hostLinkLibraries) << "\n"
                << "  },\n"
                << "  \"run\": {\n"
                << "    \"workingDirectory\": " << jsonString(info.runWorkingDirectory.string()) << ",\n"
                << "    \"passLibraryPath\": " << (info.passLibraryPath ? "true" : "false") << ",\n"
                << "    \"args\": " << jsonStringArray(info.runArgs) << "\n"
                << "  }\n"
                << "}\n";
        }

        bool getFlagValue(Argonaut::Parser& parser, const std::string& id)
        {
            auto values = parser.GetValuesOf<bool>(id);
            return !values.empty() && values.front();
        }

        std::string getProjectValue(Argonaut::Parser& parser)
        {
            const auto positionalValues = parser.GetValuesOf<std::string>("PROJECT-PATH");
            if (!positionalValues.empty())
            {
                if (parser.WasProvided("PROJECT"))
                    throw std::runtime_error("Specify the project either positionally or with --project, not both.");
                return positionalValues.front();
            }

            const auto projectValues = parser.GetValuesOf<std::string>("PROJECT");
            return projectValues.empty() ? "." : projectValues.front();
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
                std::cerr << "Tooling CLI setup failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const Argonaut::ParseException& e)
            {
                std::cerr << "Tooling argument parsing failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Unhandled tooling CLI error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        Argonaut::Parser makeBuildParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("BUILD-DIR")
                        .AddAlias("--build-dir")
                        .SetDefaultValue("build")
                        .SetDescription("CMake build directory for the Wio repo.")
                )
                .Add(
                    Argonaut::Argument("CONFIG")
                        .AddAlias("--config")
                        .SetDefaultValue("Debug")
                        .SetDescription("Build configuration to pass to CMake.")
                )
                .Add(
                    Argonaut::Argument("CONFIGURE")
                        .AddAlias("--configure")
                        .Flag()
                        .SetDescription("Run CMake configure before building.")
                )
                .Add(
                    Argonaut::Argument("TEST")
                        .AddAlias("--test")
                        .Flag()
                        .SetDescription("Run ctest after a successful build.")
                )
                .AutoHelp()
                .HelpOnEmpty(false)
                .AutoVersion()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        Argonaut::Parser makeTestParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("BUILD-DIR")
                        .AddAlias("--build-dir")
                        .SetDefaultValue("build")
                        .SetDescription("CMake build directory for the Wio repo.")
                )
                .Add(
                    Argonaut::Argument("CONFIG")
                        .AddAlias("--config")
                        .SetDefaultValue("Debug")
                        .SetDescription("CTest configuration to execute.")
                )
                .Add(
                    Argonaut::Argument("FILTER")
                        .AddAlias("--filter")
                        .SetDescription("Regex filter passed through to ctest -R.")
                )
                .Add(
                    Argonaut::Argument("LIST")
                        .AddAlias("--list")
                        .Flag()
                        .SetDescription("List matching tests instead of executing them.")
                )
                .Add(
                    Argonaut::Argument("CONFIGURE")
                        .AddAlias("--configure")
                        .Flag()
                        .SetDescription("Run CMake configure before invoking ctest.")
                )
                .AutoHelp()
                .HelpOnEmpty(false)
                .AutoVersion()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        Argonaut::Parser makeProjectNewParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("NAME")
                        .Required()
                        .SetDescription("Project name and generated directory name.")
                )
                .Add(
                    Argonaut::Argument("OUTPUT-DIR")
                        .AddAlias("--output-dir")
                        .SetDefaultValue(".")
                        .SetDescription("Directory under which the project folder will be created.")
                )
                .Add(
                    Argonaut::Argument("TEMPLATE")
                        .AddAlias("--template")
                        .SetDefaultValue("hybrid-module")
                        .SetDescription("Template to generate: hybrid-module, wio-app, wio-native-app, wio-module.")
                )
                .Add(
                    Argonaut::Argument("FORCE")
                        .AddAlias("--force")
                        .Flag()
                        .SetDescription("Allow generating into an existing non-empty directory.")
                )
                .AutoHelp()
                .AutoVersion()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        Argonaut::Parser makeProjectActionParser(const std::string& actionDescription)
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("PROJECT-PATH")
                        .SetDescription("Optional project directory or makewio manifest path.")
                )
                .Add(
                    Argonaut::Argument("PROJECT")
                        .AddAlias("--project")
                        .SetDefaultValue(".")
                        .SetDescription("Project directory or makewio manifest path to " + actionDescription + ".")
                )
                .Add(
                    Argonaut::Argument("CONFIG")
                        .AddAlias("--config")
                        .SetDefaultValue("")
                        .SetDescription("Optional project build configuration override.")
                )
                .Add(
                    Argonaut::Argument("BUILD-DIR")
                        .AddAlias("--build-dir")
                        .SetDefaultValue("")
                        .SetDescription("Optional override for the project build directory.")
                );

            if (actionDescription != "describe")
            {
                parser.Add(
                    Argonaut::Argument("CONFIGURE")
                        .AddAlias("--rebuild")
                        .AddAlias("--configure")
                        .Flag()
                        .SetDescription("Force recompilation even when project outputs are up to date. --configure is retained as a compatibility alias.")
                );
            }

            if (actionDescription == "run")
            {
                parser
                    .Add(
                        Argonaut::Argument("NO-BUILD")
                            .AddAlias("--no-build")
                            .Flag()
                            .SetDescription("Run the existing executable without checking or rebuilding project outputs.")
                    )
                    .Add(
                        Argonaut::Argument("WORKING-DIRECTORY")
                            .AddAlias("--cwd")
                            .AddAlias("--working-directory")
                            .SetDefaultValue("")
                            .SetDescription("Override the run working directory; relative paths resolve from the project root.")
                    )
                    .Add(
                        Argonaut::Argument("NO-MANIFEST-ARGS")
                            .AddAlias("--no-manifest-args")
                            .Flag()
                            .SetDescription("Do not prepend arguments declared in run.args.")
                    )
                    .Add(
                        Argonaut::Argument("RUN-ARG")
                            .AddAlias("--arg")
                            .MultiValue()
                            .SetDescription("Append one application argument. May be repeated; '--' is preferred for arbitrary argument lists.")
                    )
                    .Add(
                        Argonaut::Argument("PRINT-COMMAND")
                            .AddAlias("--print-command")
                            .Flag()
                            .SetDescription("Print the resolved executable, working directory, and arguments before launching.")
                    )
                    .SetUsageSuffix(" [-- <application-args...>]");
            }

            parser
                .AutoHelp()
                .HelpOnEmpty(false)
                .AutoVersion()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        int handleBuildCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeBuildParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            const auto repoRoot = tryFindRepoRoot();
            if (!repoRoot.has_value())
            {
                std::cerr << "Could not resolve the Wio repository root for 'wio build'.\n";
                return EXIT_FAILURE;
            }

            const std::string buildDir = parser.GetValuesOf<std::string>("BUILD-DIR").front();
            const std::string config = parser.GetValuesOf<std::string>("CONFIG").front();
            const bool configure = getFlagValue(parser, "CONFIGURE");
            const bool test = getFlagValue(parser, "TEST");

            const std::filesystem::path resolvedBuildDir = resolveBuildDir(*repoRoot, buildDir);

            if (configure)
            {
                if (const int configureResult = process::Run({
                        "cmake",
                        "-S", repoRoot->string(),
                        "-B", resolvedBuildDir.string()
                    });
                    configureResult != 0)
                {
                    return configureResult;
                }
            }

            if (const int buildResult = process::Run({
                    "cmake",
                    "--build", resolvedBuildDir.string(),
                    "--config", config
                });
                buildResult != 0)
            {
                return buildResult;
            }

            if (test)
            {
                return process::Run({
                    "ctest",
                    "--test-dir", resolvedBuildDir.string(),
                    "-C", config,
                    "--output-on-failure"
                });
            }

            return EXIT_SUCCESS;
        }

        int handleTestCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeTestParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            const auto repoRoot = tryFindRepoRoot();
            if (!repoRoot.has_value())
            {
                std::cerr << "Could not resolve the Wio repository root for 'wio test'.\n";
                return EXIT_FAILURE;
            }

            const std::string buildDir = parser.GetValuesOf<std::string>("BUILD-DIR").front();
            const std::string config = parser.GetValuesOf<std::string>("CONFIG").front();
            const auto filterValues = parser.GetValuesOf<std::string>("FILTER");
            const std::string filter = filterValues.empty() ? "" : filterValues.front();
            const bool list = getFlagValue(parser, "LIST");
            const bool configure = getFlagValue(parser, "CONFIGURE");

            const std::filesystem::path resolvedBuildDir = resolveBuildDir(*repoRoot, buildDir);

            if (configure)
            {
                if (const int configureResult = process::Run({
                        "cmake",
                        "-S", repoRoot->string(),
                        "-B", resolvedBuildDir.string()
                    });
                    configureResult != 0)
                {
                    return configureResult;
                }
            }

            std::vector<std::string> ctestCommand{
                "ctest",
                "--test-dir", resolvedBuildDir.string(),
                "-C", config
            };

            if (list)
                ctestCommand.push_back("-N");
            else
                ctestCommand.push_back("--output-on-failure");

            if (!filter.empty())
            {
                ctestCommand.push_back("-R");
                ctestCommand.push_back(filter);
            }

            return process::Run(ctestCommand);
        }

        int handleProjectNewCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeProjectNewParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            const std::string name = parser.GetValuesOf<std::string>("NAME").front();
            const std::string outputDir = parser.GetValuesOf<std::string>("OUTPUT-DIR").front();
            const std::string templateName = parser.GetValuesOf<std::string>("TEMPLATE").front();
            const bool force = getFlagValue(parser, "FORCE");

            const auto templateSpec = tryMakeTemplateSpec(templateName);
            if (!templateSpec.has_value())
            {
                std::cerr << "Unknown project template: " << templateName
                          << ". Expected one of: hybrid-module, wio-app, wio-native-app, wio-module.\n";
                return EXIT_FAILURE;
            }

            const std::string safeName = toSafeIdentifier(name);
            const std::filesystem::path projectRoot =
                std::filesystem::absolute(std::filesystem::path(outputDir) / name).make_preferred();

            std::error_code ec;
            if (std::filesystem::exists(projectRoot, ec))
            {
                if (!std::filesystem::is_directory(projectRoot, ec))
                {
                    std::cerr << "Project path already exists and is not a directory: " << projectRoot.string() << '\n';
                    return EXIT_FAILURE;
                }

                const bool hasContents =
                    std::filesystem::directory_iterator(projectRoot, ec) != std::filesystem::directory_iterator();
                if (!force && hasContents)
                {
                    std::cerr << "Project directory already exists and is not empty: " << projectRoot.string()
                              << "\nUse --force if you want to generate into it anyway.\n";
                    return EXIT_FAILURE;
                }
            }
            else
            {
                std::filesystem::create_directories(projectRoot, ec);
                if (ec)
                {
                    std::cerr << "Could not create project directory: " << projectRoot.string() << '\n';
                    return EXIT_FAILURE;
                }
            }

            try
            {
                const auto& spec = *templateSpec;
                const std::filesystem::path wioDir = projectRoot / "wio";
                const std::filesystem::path hostDir = projectRoot / "host";
                const std::filesystem::path hostIncludeDir = hostDir / "include";
                const std::filesystem::path nativeDir = projectRoot / "native";
                const std::filesystem::path nativeIncludeDir = nativeDir / "include";
                const std::filesystem::path nativeSourceDir = nativeDir / "src";
                const std::filesystem::path nativeLibDir = nativeDir / "lib";

                std::filesystem::create_directories(wioDir);
                std::filesystem::create_directories(hostIncludeDir);
                std::filesystem::create_directories(nativeIncludeDir);
                std::filesystem::create_directories(nativeSourceDir);
                std::filesystem::create_directories(nativeLibDir);

                writeUtf8File(projectRoot / "wio.makewio", renderMakeWioManifest(name, templateName, safeName, spec));
                writeUtf8File(projectRoot / "CMakeLists.txt", renderCMakeLists(safeName));
                writeUtf8File(projectRoot / "README.md", renderReadme(name, templateName, spec));
                writeUtf8File(projectRoot / ".gitignore", renderGitIgnore());
                writeUtf8File(nativeDir / "README.md", renderNativeReadme());
                writeUtf8File(projectRoot / spec.wioEntry, spec.wioSource);
                writeUtf8File(hostIncludeDir / ".gitkeep", "# placeholder\n");
                writeUtf8File(nativeIncludeDir / ".gitkeep", "# placeholder\n");
                writeUtf8File(nativeSourceDir / ".gitkeep", "# placeholder\n");
                writeUtf8File(nativeLibDir / ".gitkeep", "# placeholder\n");

                if (spec.hostEnabled && !spec.hostSource.empty())
                    writeUtf8File(hostDir / "main.cpp", spec.hostSource);

                if (!spec.nativeHeaderName.empty() && !spec.nativeHeaderContent.empty())
                    writeUtf8File(nativeIncludeDir / spec.nativeHeaderName, spec.nativeHeaderContent);

                if (!spec.nativeSourceName.empty() && !spec.nativeSourceContent.empty())
                    writeUtf8File(nativeSourceDir / spec.nativeSourceName, spec.nativeSourceContent);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Project generation failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }

            std::cout
                << "Generated project: " << projectRoot.string() << '\n'
                << "Template         : " << templateName << '\n'
                << "Manifest         : " << (projectRoot / "wio.makewio").string() << '\n';
            return EXIT_SUCCESS;
        }

        int buildProject(const ProjectInfo& info, bool forceRebuild)
        {
            std::error_code ec;
            std::filesystem::create_directories(info.outputDirectory, ec);

            std::vector<std::string> wioCommand{
                info.toolchainExecutable.string(),
                info.wioEntry.string(),
                "--target", info.wioTarget,
                "--output", info.wioOutput.string()
            };

            for (const auto& moduleDir : info.wioSourceRoots)
            {
                wioCommand.push_back("--module-dir");
                wioCommand.push_back(moduleDir.string());
            }

            for (const auto& includeDir : info.wioIncludeDirs)
            {
                wioCommand.push_back("--include-dir");
                wioCommand.push_back(includeDir.string());
            }

            for (const auto& linkDir : info.wioLinkDirs)
            {
                wioCommand.push_back("--link-dir");
                wioCommand.push_back(linkDir.string());
            }

            for (const auto& linkLibrary : info.wioLinkLibraries)
            {
                wioCommand.push_back("--link-lib");
                wioCommand.push_back(linkLibrary);
            }

            for (const auto& nativeSource : info.wioNativeSources)
            {
                wioCommand.push_back("--backend-arg");
                wioCommand.push_back(nativeSource.string());
            }

            for (const auto& backendArg : info.wioBackendArgs)
            {
                wioCommand.push_back("--backend-arg");
                wioCommand.push_back(backendArg);
            }

            std::vector<std::filesystem::path> wioInputs{
                info.manifestFile,
                info.toolchainExecutable,
                info.toolchainRoot / "std",
                info.toolchainRoot / "runtime" / "include",
                info.toolchainRoot / "sdk" / "include",
                info.wioEntry
            };

            for (const auto& path : info.wioSourceRoots)
                wioInputs.push_back(path);
            for (const auto& path : info.wioIncludeDirs)
                wioInputs.push_back(path);
            for (const auto& path : info.wioNativeSources)
                wioInputs.push_back(path);
            for (const auto& linkLibrary : info.wioLinkLibraries)
            {
                if (looksLikeLibraryFile(linkLibrary))
                    wioInputs.push_back(std::filesystem::path(linkLibrary));
            }

            if (forceRebuild || !outputUpToDate(info.wioOutput, wioInputs))
            {
                if (const int result = process::Run(wioCommand, info.projectRoot); result != 0)
                    return result;
            }
            else
            {
                std::cout << "Project Wio compile: up to date, skipping compiler invocation.\n";
            }

            if (!info.hostEnabled)
                return EXIT_SUCCESS;

            if (info.hostSourceFiles.empty())
                return EXIT_FAILURE;

            std::vector<std::string> hostCommand{
                info.hostCompilerExecutable.string(),
                "-std=c++20",
                "-I", info.sdkIncludeDirectory.string()
            };

            for (const auto& sourceFile : info.hostSourceFiles)
                hostCommand.push_back(sourceFile.string());

            for (const auto& includeDir : info.hostIncludeDirs)
            {
                hostCommand.push_back("-I");
                hostCommand.push_back(includeDir.string());
            }

            for (const auto& linkDir : info.hostLinkDirs)
            {
                hostCommand.push_back("-L");
                hostCommand.push_back(linkDir.string());
            }

            for (const auto& linkLibrary : info.hostLinkLibraries)
            {
                if (!linkLibrary.empty() && linkLibrary.front() == '-')
                    hostCommand.push_back(linkLibrary);
                else if (looksLikeLibraryFile(linkLibrary))
                    hostCommand.push_back(linkLibrary);
                else
                    hostCommand.push_back("-l" + linkLibrary);
            }

            if (info.wioTarget == "static")
            {
                if (info.runtimeStaticLibrary.empty())
                {
                    std::cerr << "The runtime static library could not be resolved for a static Wio project build.\n";
                    return EXIT_FAILURE;
                }

                hostCommand.push_back(info.wioOutput.string());
                hostCommand.push_back(info.runtimeStaticLibrary.string());
            }

#if defined(_WIN32)
            hostCommand.push_back("-static");
#endif

            for (const auto& arg : info.hostCompilerArgs)
                hostCommand.push_back(arg);

            hostCommand.push_back("-o");
            hostCommand.push_back(info.hostOutput.string());

            std::vector<std::filesystem::path> hostInputs{
                info.manifestFile,
                info.wioOutput,
                info.sdkIncludeDirectory
            };

            for (const auto& path : info.hostSourceFiles)
                hostInputs.push_back(path);
            for (const auto& path : info.hostIncludeDirs)
                hostInputs.push_back(path);
            for (const auto& linkLibrary : info.hostLinkLibraries)
            {
                if (looksLikeLibraryFile(linkLibrary))
                    hostInputs.push_back(std::filesystem::path(linkLibrary));
            }
            if (!info.runtimeStaticLibrary.empty())
                hostInputs.push_back(info.runtimeStaticLibrary);

            if (forceRebuild || !outputUpToDate(info.hostOutput, hostInputs))
            {
                const std::vector<std::filesystem::path> extraPathEntries{
                    info.hostCompilerExecutable.parent_path()
                };
                if (const int result = process::Run(hostCommand, info.projectRoot, extraPathEntries); result != 0)
                    return result;
            }
            else
            {
                std::cout << "Project host compile: up to date, skipping host compiler invocation.\n";
            }

            return EXIT_SUCCESS;
        }

        int handleProjectDescribeCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeProjectActionParser("describe");
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            try
            {
                const ProjectInfo info = resolveProjectInfo(
                    getProjectValue(parser),
                    parser.GetValuesOf<std::string>("CONFIG").front(),
                    parser.GetValuesOf<std::string>("BUILD-DIR").front()
                );
                printProjectInfoJson(info);
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Project describe failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        int handleProjectBuildCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeProjectActionParser("build");
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            try
            {
                const ProjectInfo info = resolveProjectInfo(
                    getProjectValue(parser),
                    parser.GetValuesOf<std::string>("CONFIG").front(),
                    parser.GetValuesOf<std::string>("BUILD-DIR").front()
                );
                return buildProject(info, getFlagValue(parser, "CONFIGURE"));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Project build failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        struct ProjectRunInvocation
        {
            std::vector<std::string> commandArgs;
            std::vector<std::string> applicationArgs;
        };

        ProjectRunInvocation splitProjectRunArguments(const std::vector<std::string>& args)
        {
            ProjectRunInvocation invocation;
            invocation.commandArgs.reserve(args.size());
            invocation.applicationArgs.reserve(args.size());

            if (args.empty())
                return invocation;

            invocation.commandArgs.push_back(args.front());
            bool applicationArgumentsStarted = false;

            for (size_t index = 1; index < args.size(); ++index)
            {
                if (!applicationArgumentsStarted && args[index] == "--")
                {
                    applicationArgumentsStarted = true;
                    continue;
                }

                if (applicationArgumentsStarted)
                    invocation.applicationArgs.push_back(args[index]);
                else
                    invocation.commandArgs.push_back(args[index]);
            }

            return invocation;
        }

        int handleProjectRunCommand(std::vector<std::string> args)
        {
            ProjectRunInvocation invocation = splitProjectRunArguments(args);
            Argonaut::Parser parser = makeProjectActionParser("run");
            if (const auto parseResult = parseWithHandling(parser, invocation.commandArgs); parseResult.has_value())
                return *parseResult;

            try
            {
                const ProjectInfo info = resolveProjectInfo(
                    getProjectValue(parser),
                    parser.GetValuesOf<std::string>("CONFIG").front(),
                    parser.GetValuesOf<std::string>("BUILD-DIR").front()
                );

                const bool noBuild = getFlagValue(parser, "NO-BUILD");
                const bool forceRebuild = getFlagValue(parser, "CONFIGURE");
                if (noBuild && forceRebuild)
                    throw std::runtime_error("--no-build cannot be combined with --rebuild or --configure.");

                if (!noBuild)
                {
                    if (const int buildResult = buildProject(info, forceRebuild); buildResult != 0)
                        return buildResult;
                }

                std::vector<std::string> runCommand;
                std::filesystem::path executablePath;
                if (info.hostEnabled)
                {
                    executablePath = info.hostOutput;
                    runCommand.push_back(executablePath.string());
                    if (info.passLibraryPath && info.wioTarget == "shared")
                        runCommand.push_back(info.wioOutput.string());
                }
                else if (info.wioTarget == "exe")
                {
                    executablePath = info.wioOutput;
                    runCommand.push_back(executablePath.string());
                }

                if (runCommand.empty())
                {
                    std::cerr << "This project does not define a runnable executable path for 'wio project run'.\n";
                    return EXIT_FAILURE;
                }

                if (noBuild)
                {
                    std::error_code ec;
                    if (!std::filesystem::exists(executablePath, ec) ||
                        !std::filesystem::is_regular_file(executablePath, ec))
                    {
                        throw std::runtime_error(
                            "--no-build was requested, but the expected executable does not exist: " +
                            executablePath.string()
                        );
                    }
                }

                if (!getFlagValue(parser, "NO-MANIFEST-ARGS"))
                {
                    for (const auto& arg : info.runArgs)
                        runCommand.push_back(arg);
                }

                for (const auto& arg : parser.GetValuesOf<std::string>("RUN-ARG"))
                    runCommand.push_back(arg);

                for (const auto& arg : invocation.applicationArgs)
                    runCommand.push_back(arg);

                std::filesystem::path runWorkingDirectory = info.runWorkingDirectory;
                const std::string workingDirectoryOverride =
                    parser.GetValuesOf<std::string>("WORKING-DIRECTORY").front();
                if (!workingDirectoryOverride.empty())
                    runWorkingDirectory = resolveProjectPath(info.projectRoot, workingDirectoryOverride);

                std::error_code ec;
                if (!std::filesystem::exists(runWorkingDirectory, ec) ||
                    !std::filesystem::is_directory(runWorkingDirectory, ec))
                {
                    throw std::runtime_error(
                        "Run working directory does not exist or is not a directory: " +
                        runWorkingDirectory.string()
                    );
                }

                if (getFlagValue(parser, "PRINT-COMMAND"))
                {
                    std::cout << "Working directory: " << runWorkingDirectory.string() << '\n';
                    std::cout << "Command: " << process::FormatCommand(runCommand) << '\n';
                    std::cout.flush();
                }

                return process::Run(runCommand, runWorkingDirectory);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Project run failed: " << e.what() << '\n';
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

        bool looksLikeDirectCompilerInvocation(const std::string_view command)
        {
            if (command.empty())
                return false;
            if (command.front() == '-')
                return true;

            const std::filesystem::path path{ std::string(command) };
            if (path.extension() == ".wio")
                return true;

            std::error_code ec;
            return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
        }

        size_t editDistance(const std::string_view left, const std::string_view right)
        {
            std::vector<size_t> previous(right.size() + 1);
            std::vector<size_t> current(right.size() + 1);
            for (size_t index = 0; index <= right.size(); ++index)
                previous[index] = index;

            for (size_t leftIndex = 0; leftIndex < left.size(); ++leftIndex)
            {
                current[0] = leftIndex + 1;
                for (size_t rightIndex = 0; rightIndex < right.size(); ++rightIndex)
                {
                    const size_t substitutionCost = left[leftIndex] == right[rightIndex] ? 0 : 1;
                    current[rightIndex + 1] = std::min({
                        current[rightIndex] + 1,
                        previous[rightIndex + 1] + 1,
                        previous[rightIndex] + substitutionCost
                    });
                }
                previous.swap(current);
            }

            return previous.back();
        }

        std::optional<std::string_view> suggestTopLevelCommand(const std::string_view command)
        {
            static constexpr std::string_view commands[]{
                "help", "run", "build", "test", "file", "project", "bind", "env", "package", "perf", "dev"
            };

            std::optional<std::string_view> best;
            size_t bestDistance = 3;
            for (const std::string_view candidate : commands)
            {
                const size_t distance = editDistance(command, candidate);
                if (distance < bestDistance)
                {
                    best = candidate;
                    bestDistance = distance;
                }
            }
            return best;
        }
    }

    std::optional<int> tryHandleToolingCommand(int argc, char* argv[])
    {
        if (argv == nullptr)
            return std::nullopt;

        if (argc < 2 || argv[1] == nullptr)
        {
            printToolingUsage();
            return EXIT_SUCCESS;
        }

        const std::string_view command = argv[1];
        if (command == "help" && argc > 2)
        {
            std::vector<std::string> rewrittenArgs;
            rewrittenArgs.reserve(static_cast<size_t>(argc));
            rewrittenArgs.emplace_back(argv[0] != nullptr ? argv[0] : "wio");
            for (int index = 2; index < argc; ++index)
            {
                if (argv[index] != nullptr)
                    rewrittenArgs.emplace_back(argv[index]);
            }
            rewrittenArgs.emplace_back("--help");

            std::vector<char*> rewrittenArgv = buildArgvView(rewrittenArgs);
            return tryHandleToolingCommand(
                static_cast<int>(rewrittenArgv.size()),
                rewrittenArgv.data()
            );
        }

        if (isHelpToken(command))
        {
            printToolingUsage();
            return EXIT_SUCCESS;
        }

        if (isVersionToken(command))
        {
            std::cout << kWioCliVersion << '\n';
            return EXIT_SUCCESS;
        }

        if (argc == 3 && argv[2] != nullptr && isVersionToken(argv[2]) &&
            (command == "file" || command == "project" || command == "bind" ||
             command == "env" || command == "package" || command == "perf" ||
             command == "dev"))
        {
            std::cout << kWioCliVersion << '\n';
            return EXIT_SUCCESS;
        }

        if (command == "run")
            return handleProjectRunCommand(collectCommandArgs("wio run", argc, argv, 2));

        if (command == "build")
            return handleBuildCommand(collectCommandArgs("wio build", argc, argv, 2));

        if (command == "test")
            return handleTestCommand(collectCommandArgs("wio test", argc, argv, 2));

        if (command == "file")
            return file::tryHandleFileCommand(argc, argv);

        if (command == "project")
        {
            if (argc < 3 || argv[2] == nullptr)
            {
                printProjectUsage(std::cout);
                return EXIT_SUCCESS;
            }

            const std::string_view subcommand = argv[2];
            if (isHelpToken(subcommand))
            {
                printProjectUsage(std::cout);
                return EXIT_SUCCESS;
            }
            if (subcommand == "new")
                return handleProjectNewCommand(collectCommandArgs("wio project new", argc, argv, 3));
            if (subcommand == "describe")
                return handleProjectDescribeCommand(collectCommandArgs("wio project describe", argc, argv, 3));
            if (subcommand == "build")
                return handleProjectBuildCommand(collectCommandArgs("wio project build", argc, argv, 3));
            if (subcommand == "run")
                return handleProjectRunCommand(collectCommandArgs("wio project run", argc, argv, 3));

            std::cerr << "Unknown project subcommand: " << subcommand << '\n';
            if (const auto suggestion = cli::SuggestCommand(
                    subcommand,
                    { "new", "describe", "build", "run" });
                suggestion.has_value())
            {
                std::cerr << "Did you mean 'wio project " << *suggestion << "'?\n";
            }
            std::cerr << "Run 'wio project --help' to list available project commands.\n";
            return EXIT_FAILURE;
        }

        if (command == "bind")
            return binding::tryHandleBindCommand(argc, argv);

        if (command == "env")
            return env::tryHandleEnvCommand(argc, argv);

        if (command == "package")
            return package::tryHandlePackageCommand(argc, argv);

        if (command == "perf")
            return perf::tryHandlePerfCommand(argc, argv);

        if (command != "dev")
        {
            if (looksLikeDirectCompilerInvocation(command))
                return std::nullopt;

            std::cerr << "Unknown Wio command: " << command << '\n';
            if (const auto suggestion = suggestTopLevelCommand(command); suggestion.has_value())
                std::cerr << "Did you mean 'wio " << *suggestion << "'?\n";
            std::cerr << "Run 'wio --help' to list available commands.\n";
            return EXIT_FAILURE;
        }

        if (argc < 3 || argv[2] == nullptr)
        {
            printToolingUsage();
            return EXIT_SUCCESS;
        }

        const std::string_view subcommand = argv[2];
        if (isHelpToken(subcommand))
        {
            printToolingUsage();
            return EXIT_SUCCESS;
        }
        if (subcommand == "build")
            return handleBuildCommand(collectCommandArgs("wio dev build", argc, argv, 3));

        if (subcommand == "test")
            return handleTestCommand(collectCommandArgs("wio dev test", argc, argv, 3));

        std::cerr << "Unknown developer subcommand: " << subcommand << '\n';
        if (const auto suggestion = cli::SuggestCommand(subcommand, { "build", "test" });
            suggestion.has_value())
        {
            std::cerr << "Did you mean 'wio dev " << *suggestion << "'?\n";
        }
        printToolingUsage();
        return EXIT_FAILURE;
    }
}
