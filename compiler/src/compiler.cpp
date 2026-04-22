#include "compiler.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <argonaut.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "wio/codegen/cpp_generator.h"
#include "wio/common/filesystem/filesystem.h"
#include "wio/common/logger.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"

namespace wio
{
    struct RequiredCppHeader
    {
        std::string header;
        common::Location location;
        std::filesystem::path sourcePath;
        std::string origin;
    };

    struct AppData
    {
        std::filesystem::path basePath;
        std::filesystem::path executablePath;
        CompilerFlags flags;
        Argonaut::Parser argParser;
        sema::TypeContext typeContext_;
        std::unordered_set<std::string> loadedModules;
        std::vector<RequiredCppHeader> requiredCppHeaders;
        BuildTarget buildTarget = BuildTarget::Executable;
    };
    
    namespace
    {
        AppData gAppData;

        std::filesystem::path getCompileTimeDefaultRootDir()
        {
#ifdef WIO_DEFAULT_ROOT_DIR
            return { WIO_DEFAULT_ROOT_DIR };
#else
            return {};
#endif
        }

        std::optional<std::filesystem::path> tryGetEnvironmentToolchainRoot()
        {
            const char* envNames[] = { "WIO_ROOT", "WIO_HOME" };
            for (const char* envName : envNames)
            {
                const char* rawValue = std::getenv(envName);
                if (rawValue == nullptr || *rawValue == '\0')
                    continue;

                std::filesystem::path candidate = std::filesystem::absolute(std::filesystem::path(rawValue)).make_preferred();
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec) && !ec)
                    return candidate;
            }

            return std::nullopt;
        }

        std::optional<std::filesystem::path> tryGetExecutablePathFromSystem()
        {
#if defined(_WIN32)
            std::wstring buffer(MAX_PATH, L'\0');
            for (;;)
            {
                const DWORD copiedLength = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (copiedLength == 0)
                    return std::nullopt;

                if (copiedLength < buffer.size())
                {
                    buffer.resize(copiedLength);
                    return std::filesystem::path(buffer).make_preferred();
                }

                buffer.resize(buffer.size() * 2);
            }
#elif defined(__APPLE__)
            std::uint32_t bufferSize = 0;
            if (_NSGetExecutablePath(nullptr, &bufferSize) != -1 || bufferSize == 0)
                return std::nullopt;

            std::string buffer(bufferSize, '\0');
            if (_NSGetExecutablePath(buffer.data(), &bufferSize) != 0)
                return std::nullopt;

            return std::filesystem::weakly_canonical(std::filesystem::path(buffer.c_str())).make_preferred();
#else
            std::vector<char> buffer(1024, '\0');
            for (;;)
            {
                const ssize_t copiedLength = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
                if (copiedLength < 0)
                    return std::nullopt;

                if (static_cast<size_t>(copiedLength) < buffer.size() - 1)
                {
                    buffer[static_cast<size_t>(copiedLength)] = '\0';
                    return std::filesystem::path(buffer.data()).make_preferred();
                }

                buffer.resize(buffer.size() * 2);
            }
#endif
        }

        std::filesystem::path getExecutablePath()
        {
            if (!gAppData.executablePath.empty())
                return gAppData.executablePath;

            if (auto detectedPath = tryGetExecutablePathFromSystem(); detectedPath.has_value())
                return detectedPath->make_preferred();

            return {};
        }

        std::vector<std::filesystem::path> getToolchainRootCandidates()
        {
            std::vector<std::filesystem::path> candidates;

            auto appendCandidate = [&](const std::filesystem::path& candidate)
            {
                if (candidate.empty())
                    return;

                std::filesystem::path absoluteCandidate = std::filesystem::absolute(candidate).make_preferred();
                if (std::ranges::find(candidates, absoluteCandidate) == candidates.end())
                    candidates.push_back(std::move(absoluteCandidate));
            };

            if (auto envRoot = tryGetEnvironmentToolchainRoot(); envRoot.has_value())
                appendCandidate(*envRoot);

            if (const std::filesystem::path executablePath = getExecutablePath(); !executablePath.empty())
            {
                const std::filesystem::path executableDir = executablePath.parent_path();
                appendCandidate(executableDir);
                appendCandidate(executableDir.parent_path());
            }

            appendCandidate(getCompileTimeDefaultRootDir());
            return candidates;
        }

        std::filesystem::path resolveToolchainDirectory(const std::filesystem::path& compiledPath,
                                                        std::initializer_list<std::filesystem::path> relativeCandidates)
        {
            auto tryCandidate = [](const std::filesystem::path& candidate) -> std::optional<std::filesystem::path>
            {
                if (candidate.empty())
                    return std::nullopt;

                std::error_code ec;
                if (std::filesystem::exists(candidate, ec) && !ec &&
                    std::filesystem::is_directory(candidate, ec) && !ec)
                {
                    return std::filesystem::absolute(candidate).make_preferred();
                }

                return std::nullopt;
            };

            for (const auto& rootCandidate : getToolchainRootCandidates())
            {
                for (const auto& relativeCandidate : relativeCandidates)
                {
                    if (auto resolved = tryCandidate((rootCandidate / relativeCandidate).make_preferred()); resolved.has_value())
                        return *resolved;
                }
            }

            if (auto resolved = tryCandidate(compiledPath); resolved.has_value())
                return *resolved;

            return {};
        }

        std::filesystem::path resolveToolchainFile(const std::filesystem::path& compiledPath,
                                                   std::initializer_list<std::filesystem::path> relativeCandidates)
        {
            auto tryCandidate = [](const std::filesystem::path& candidate) -> std::optional<std::filesystem::path>
            {
                if (candidate.empty())
                    return std::nullopt;

                std::error_code ec;
                if (std::filesystem::exists(candidate, ec) && !ec &&
                    std::filesystem::is_regular_file(candidate, ec) && !ec)
                {
                    return std::filesystem::absolute(candidate).make_preferred();
                }

                return std::nullopt;
            };

            for (const auto& rootCandidate : getToolchainRootCandidates())
            {
                for (const auto& relativeCandidate : relativeCandidates)
                {
                    if (auto resolved = tryCandidate((rootCandidate / relativeCandidate).make_preferred()); resolved.has_value())
                        return *resolved;
                }
            }

            if (auto resolved = tryCandidate(compiledPath); resolved.has_value())
                return *resolved;

            return {};
        }

        std::filesystem::path getRuntimeIncludeDir()
        {
            return resolveToolchainDirectory(
#ifdef WIO_RUNTIME_INCLUDE_DIR
                std::filesystem::path{ WIO_RUNTIME_INCLUDE_DIR },
#else
                std::filesystem::path{},
#endif
                {
                    std::filesystem::path("runtime") / "include"
                }
            );
        }

        std::filesystem::path getSdkIncludeDir()
        {
            return resolveToolchainDirectory(
#ifdef WIO_SDK_INCLUDE_DIR
                std::filesystem::path{ WIO_SDK_INCLUDE_DIR },
#else
                std::filesystem::path{},
#endif
                {
                    std::filesystem::path("sdk") / "include"
                }
            );
        }

        std::filesystem::path getStdSourceDir()
        {
            return resolveToolchainDirectory(
#ifdef WIO_STD_SOURCE_DIR
                std::filesystem::path{ WIO_STD_SOURCE_DIR },
#else
                std::filesystem::path{},
#endif
                {
                    "std"
                }
            );
        }

        std::string getBackendCompiler()
        {
#ifdef WIO_BACKEND_CXX_COMPILER
            return WIO_BACKEND_CXX_COMPILER;
#else
            return "g++";
#endif
        }

        std::string getBackendArchiver()
        {
#ifdef WIO_BACKEND_AR
            return WIO_BACKEND_AR;
#else
            return "ar";
#endif
        }

        std::filesystem::path getRuntimeLibraryFile()
        {
            return resolveToolchainFile(
#ifdef WIO_RUNTIME_LIBRARY_FILE
                std::filesystem::path{ WIO_RUNTIME_LIBRARY_FILE },
#else
                std::filesystem::path{},
#endif
                {
                    std::filesystem::path("runtime") / "lib" / "libwio_runtime.a",
                    std::filesystem::path("lib") / "libwio_runtime.a",
                    std::filesystem::path("build") / "runtime" / "backend" / "libwio_runtime.a",
                    std::filesystem::path("build") / "runtime" / "libwio_runtime.a"
                }
            );
        }

        std::vector<std::filesystem::path> getBackendSystemIncludeDirs(const std::filesystem::path& runtimeIncludeDir,
                                                                       const std::filesystem::path& sdkIncludeDir)
        {
            std::vector<std::filesystem::path> includeDirs;
            includeDirs.reserve(2);

            if (!runtimeIncludeDir.empty())
                includeDirs.push_back(runtimeIncludeDir);

            if (!sdkIncludeDir.empty())
                includeDirs.push_back(sdkIncludeDir);

            return includeDirs;
        }

        std::string normalizeLowercase(std::string value)
        {
            std::ranges::transform(value, value.begin(), [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        bool isSourceFilePath(std::string_view value)
        {
            return value.ends_with(".c") ||
                   value.ends_with(".cc") ||
                   value.ends_with(".cpp") ||
                   value.ends_with(".cxx");
        }

        BuildTarget parseBuildTarget(const std::string& value)
        {
            std::string normalized = normalizeLowercase(value);
            if (normalized.empty() || normalized == "exe" || normalized == "executable")
                return BuildTarget::Executable;
            if (normalized == "static" || normalized == "staticlib" || normalized == "static-library")
                return BuildTarget::StaticLibrary;
            if (normalized == "shared" || normalized == "sharedlib" || normalized == "shared-library" || normalized == "dll")
                return BuildTarget::SharedLibrary;

            throw std::invalid_argument("Unknown build target: " + value + ". Expected one of: exe, static, shared.");
        }

        std::filesystem::path makeDefaultOutputPath(const std::filesystem::path& sourcePath, BuildTarget target)
        {
            std::filesystem::path outputPath = sourcePath.parent_path() / sourcePath.stem();

            switch (target)
            {
            case BuildTarget::Executable:
#ifdef _WIN32
                outputPath.replace_extension(".exe");
#endif
                break;
            case BuildTarget::StaticLibrary:
                outputPath.replace_extension(".a");
                break;
            case BuildTarget::SharedLibrary:
#ifdef _WIN32
                outputPath.replace_extension(".dll");
#elif defined(__APPLE__)
                outputPath.replace_extension(".dylib");
#else
                outputPath.replace_extension(".so");
#endif
                break;
            }

            return outputPath;
        }

        std::string quotePath(const std::filesystem::path& path)
        {
            return "\"" + path.string() + "\"";
        }

        std::string quoteCommand(const std::string& value)
        {
            if (value.find_first_of(" \t") == std::string::npos)
                return value;
            
            return "\"" + value + "\"";
        }

        std::string escapeTokenValueForDisplay(std::string_view value)
        {
            return common::wioStringToEscapedCppString(std::string(value));
        }

        bool isLibraryFilePath(std::string_view libraryValue)
        {
            return libraryValue.find('\\') != std::string_view::npos ||
                   libraryValue.find('/') != std::string_view::npos ||
                   libraryValue.ends_with(".lib") ||
                   libraryValue.ends_with(".a") ||
                   libraryValue.ends_with(".so") ||
                   libraryValue.ends_with(".dll") ||
                   libraryValue.ends_with(".dylib") ||
                   libraryValue.ends_with(".o") ||
                   libraryValue.ends_with(".obj");
        }

        void appendBackendArguments(std::stringstream& cmd, const std::vector<std::string>& backendArgs)
        {
            for (const auto& backendArg : backendArgs)
            {
                cmd << " " << quoteCommand(backendArg);
            }
        }

        void appendIncludeDirectories(std::stringstream& cmd, const std::vector<std::string>& includeDirs)
        {
            for (const auto& includeDir : includeDirs)
            {
                cmd << " -I" << quotePath(includeDir);
            }
        }

        void appendIncludeDirectories(std::stringstream& cmd, const std::vector<std::filesystem::path>& includeDirs)
        {
            for (const auto& includeDir : includeDirs)
            {
                cmd << " -I" << quotePath(includeDir);
            }
        }

        void appendLinkDirectories(std::stringstream& cmd, const std::vector<std::string>& linkDirs)
        {
            for (const auto& linkDir : linkDirs)
            {
                cmd << " -L" << quotePath(linkDir);
            }
        }

        void appendLinkLibraries(std::stringstream& cmd, const std::vector<std::string>& linkLibraries)
        {
            for (const auto& linkLibrary : linkLibraries)
            {
                cmd << " ";

                if (linkLibrary.starts_with("-"))
                {
                    cmd << linkLibrary;
                }
                else if (isLibraryFilePath(linkLibrary))
                {
                    cmd << quoteCommand(linkLibrary);
                }
                else
                {
                    cmd << "-l" << linkLibrary;
                }
            }
        }

        struct CommandResult
        {
            int exitCode = EXIT_FAILURE;
            std::string output;
            std::string command;
        };

        enum class BackendDiagnosticSeverity : uint8_t
        {
            Error,
            Warning,
            Note
        };

        enum class BackendDiagnosticDomain : uint8_t
        {
            Unknown,
            Compiler,
            Linker,
            Archiver
        };

        struct BackendDiagnostic
        {
            BackendDiagnosticSeverity severity = BackendDiagnosticSeverity::Error;
            BackendDiagnosticDomain domain = BackendDiagnosticDomain::Unknown;
            common::Location location;
            std::string sourceLabel;
            std::string code;
            std::string message;
        };

        CommandResult runCommandCaptureOutput(const std::string& command)
        {
            CommandResult result;
            result.command = command;
            const std::string redirectedCommand = command + " 2>&1";

#if defined(_WIN32)
            FILE* pipe = _popen(redirectedCommand.c_str(), "r");
#else
            FILE* pipe = popen(redirectedCommand.c_str(), "r");
#endif
            if (pipe == nullptr)
            {
                result.output = "Failed to start backend command.";
                return result;
            }

            std::string buffer(4096, '\0');
            while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
                result.output += buffer.data();

#if defined(_WIN32)
            result.exitCode = _pclose(pipe);
#else
            const int closeResult = pclose(pipe);
            result.exitCode = closeResult;
            if (closeResult != -1 && WIFEXITED(closeResult))
                result.exitCode = WEXITSTATUS(closeResult);
#endif

            return result;
        }

        std::string trimWhitespace(std::string_view value)
        {
            size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
                ++start;

            size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
                --end;

            return std::string(value.substr(start, end - start));
        }

        std::string toLowerAscii(std::string_view value)
        {
            std::string result(value);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
            return result;
        }

        bool looksLikeSourceFilePath(std::string_view value)
        {
            if (value.empty())
                return false;

            const std::string lower = toLowerAscii(trimWhitespace(value));
            return lower.ends_with(".wio") ||
                   lower.ends_with(".h") ||
                   lower.ends_with(".hh") ||
                   lower.ends_with(".hpp") ||
                   lower.ends_with(".hxx") ||
                   lower.ends_with(".c") ||
                   lower.ends_with(".cc") ||
                   lower.ends_with(".cpp") ||
                   lower.ends_with(".cxx") ||
                   lower.ends_with(".ixx");
        }

        bool isWioSourceFilePath(std::string_view value)
        {
            if (value.empty())
                return false;

            return toLowerAscii(trimWhitespace(value)).ends_with(".wio");
        }

        bool isGeneratedBackendFilePath(std::string_view value)
        {
            if (value.empty())
                return false;

            const std::string trimmed = trimWhitespace(value);
            if (trimmed == "<wio-generated>")
                return true;

            return toLowerAscii(trimmed).ends_with(".wio.cpp");
        }

        std::string normalizeDiagnosticFilePath(std::string_view rawPath)
        {
            std::string pathText = trimWhitespace(rawPath);
            if (pathText.empty() || pathText.starts_with("<"))
                return pathText;

            std::error_code ec;
            std::filesystem::path path(pathText);
            std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
            if (!ec)
                return absolutePath.make_preferred().string();

            return path.make_preferred().string();
        }

        std::string unescapeCppLineDirectiveString(std::string_view rawValue)
        {
            std::string result;
            result.reserve(rawValue.size());

            bool isEscaping = false;
            for (char ch : rawValue)
            {
                if (isEscaping)
                {
                    switch (ch)
                    {
                    case '\\': result.push_back('\\'); break;
                    case '"': result.push_back('"'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    default:
                        result.push_back(ch);
                        break;
                    }

                    isEscaping = false;
                    continue;
                }

                if (ch == '\\')
                {
                    isEscaping = true;
                    continue;
                }

                result.push_back(ch);
            }

            if (isEscaping)
                result.push_back('\\');

            return result;
        }

        std::optional<common::Location> remapGeneratedCppLocation(const common::Location& location)
        {
            if (location.file.empty() || !toLowerAscii(location.file).ends_with(".wio.cpp") || location.line == 0)
                return std::nullopt;

            std::ifstream generatedFile(location.file);
            if (!generatedFile.is_open())
                return std::nullopt;

            static const std::regex lineDirectivePattern(R"wio(^\s*#line\s+([0-9]+)\s+"(.*)"\s*$)wio");

            std::string currentVirtualFile;
            uint64_t currentVirtualLine = 0;
            std::string physicalLineText;
            uint64_t physicalLine = 0;

            while (std::getline(generatedFile, physicalLineText))
            {
                ++physicalLine;

                std::smatch match;
                if (std::regex_match(physicalLineText, match, lineDirectivePattern))
                {
                    currentVirtualFile = normalizeDiagnosticFilePath(unescapeCppLineDirectiveString(match[2].str()));
                    currentVirtualLine = static_cast<uint64_t>(std::stoull(match[1].str())) - 1;

                    if (physicalLine == location.line)
                    {
                        return common::Location{
                            .file = currentVirtualFile,
                            .line = currentVirtualLine + 1,
                            .column = location.column
                        };
                    }

                    continue;
                }

                if (!currentVirtualFile.empty())
                    ++currentVirtualLine;

                if (physicalLine == location.line)
                {
                    if (currentVirtualFile.empty())
                        return std::nullopt;

                    return common::Location{
                        .file = currentVirtualFile,
                        .line = currentVirtualLine,
                        .column = location.column
                    };
                }
            }

            return std::nullopt;
        }

        common::Location normalizeBackendLocation(common::Location location)
        {
            if (auto remappedLocation = remapGeneratedCppLocation(location); remappedLocation.has_value())
                return *remappedLocation;

            return location;
        }

        std::vector<std::string> splitLines(std::string_view text)
        {
            std::vector<std::string> lines;
            size_t start = 0;
            while (start < text.size())
            {
                size_t end = text.find('\n', start);
                if (end == std::string_view::npos)
                    end = text.size();

                std::string line(text.substr(start, end - start));
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                lines.push_back(std::move(line));
                start = end + 1;
            }

            return lines;
        }

        BackendDiagnosticSeverity parseBackendSeverity(const std::string& severityText)
        {
            if (severityText == "warning")
                return BackendDiagnosticSeverity::Warning;

            if (severityText == "note")
                return BackendDiagnosticSeverity::Note;

            return BackendDiagnosticSeverity::Error;
        }

        BackendDiagnosticDomain classifyBackendTool(std::string_view toolText)
        {
            const std::string lower = toLowerAscii(trimWhitespace(toolText));

            if (lower.find("link") != std::string::npos ||
                lower.find("collect2") != std::string::npos ||
                lower.ends_with("ld") ||
                lower.find("ld.exe") != std::string::npos)
            {
                return BackendDiagnosticDomain::Linker;
            }

            if (lower == "ar" || lower.ends_with("/ar") || lower.ends_with("\\ar") ||
                lower == "lib" || lower.find("lib.exe") != std::string::npos)
            {
                return BackendDiagnosticDomain::Archiver;
            }

            if (lower.find("clang") != std::string::npos ||
                lower.find("g++") != std::string::npos ||
                lower.find("gcc") != std::string::npos ||
                lower == "cl" || lower.find("cl.exe") != std::string::npos ||
                looksLikeSourceFilePath(lower))
            {
                return BackendDiagnosticDomain::Compiler;
            }

            return BackendDiagnosticDomain::Unknown;
        }

        std::string backendDomainLabel(BackendDiagnosticDomain domain)
        {
            switch (domain)
            {
            case BackendDiagnosticDomain::Compiler:
                return "backend:compiler";
            case BackendDiagnosticDomain::Linker:
                return "backend:linker";
            case BackendDiagnosticDomain::Archiver:
                return "backend:archiver";
            case BackendDiagnosticDomain::Unknown:
                return "backend";
            }

            return "backend";
        }

        std::string backendDomainPrefix(BackendDiagnosticDomain domain)
        {
            switch (domain)
            {
            case BackendDiagnosticDomain::Compiler:
                return "Native C++ compiler";
            case BackendDiagnosticDomain::Linker:
                return "Native linker";
            case BackendDiagnosticDomain::Archiver:
                return "Native archiver";
            case BackendDiagnosticDomain::Unknown:
                return "Native backend";
            }

            return "Native backend";
        }

        std::string normalizeBackendMessage(BackendDiagnosticDomain domain, std::string_view rawMessage)
        {
            std::string message = trimWhitespace(rawMessage);

            if (domain != BackendDiagnosticDomain::Linker)
                return message;

            static const std::regex linkerPrimaryPatterns[] = {
                std::regex(R"(^.*?(undefined reference to .*)$)"),
                std::regex(R"(^.*?(unresolved external symbol .*)$)", std::regex::icase),
                std::regex(R"(^.*?(multiple definition of .*)$)"),
                std::regex(R"(^.*?(duplicate symbol .*)$)", std::regex::icase),
                std::regex(R"(^.*?(cannot find .*)$)", std::regex::icase),
                std::regex(R"(^.*?(symbol\(s\) not found.*)$)", std::regex::icase)
            };

            std::smatch match;
            for (const auto& pattern : linkerPrimaryPatterns)
            {
                if (std::regex_match(message, match, pattern))
                    return trimWhitespace(match[1].str());
            }

            return message;
        }

        bool isGenericBackendSummaryMessage(std::string_view message)
        {
            const std::string lower = toLowerAscii(trimWhitespace(message));
            return lower.find("ld returned 1 exit status") != std::string::npos ||
                   lower.find("linker command failed with exit code") != std::string::npos ||
                   lower.find("1 unresolved externals") != std::string::npos ||
                   lower == "compilation terminated.";
        }

        common::Location buildBackendDiagnosticLocation(std::string fileText,
                                                       const std::string& lineText,
                                                       const std::string& columnText)
        {
            common::Location location;
            location.file = normalizeDiagnosticFilePath(fileText);
            location.line = static_cast<uint64_t>(std::stoull(lineText));
            if (!columnText.empty())
                location.column = static_cast<uint64_t>(std::stoull(columnText));
            return normalizeBackendLocation(std::move(location));
        }

        std::optional<BackendDiagnostic> parseBackendContextNoteLine(std::string_view rawLine)
        {
            static const std::regex includeStackLeadPattern(R"(^In file included from (.*):([0-9]+)(?::([0-9]+))?[,:]?$)");
            static const std::regex includeStackContinuationPattern(R"(^\s+from (.*):([0-9]+)(?::([0-9]+))?[,:]?$)");
            static const std::regex locationContextWithColumnPattern(
                R"(^(.*):([0-9]+):([0-9]+):\s+(required from here|required from .*|instantiated from here|declared here)$)");
            static const std::regex locationContextWithoutColumnPattern(
                R"(^(.*):([0-9]+):\s+(required from here|required from .*|instantiated from here|declared here)$)");

            const std::string line(rawLine);
            std::smatch match;

            auto buildNote = [](std::string fileText,
                                const std::string& lineText,
                                const std::string& columnText,
                                std::string_view message)
            {
                return BackendDiagnostic{
                    .severity = BackendDiagnosticSeverity::Note,
                    .domain = BackendDiagnosticDomain::Compiler,
                    .location = buildBackendDiagnosticLocation(std::move(fileText), lineText, columnText),
                    .message = trimWhitespace(message)
                };
            };

            if (std::regex_match(line, match, includeStackLeadPattern))
                return buildNote(match[1].str(), match[2].str(), match[3].str(), "Included from here");

            if (std::regex_match(line, match, includeStackContinuationPattern))
                return buildNote(match[1].str(), match[2].str(), match[3].str(), "Included from here");

            if (const size_t fileContextPos = line.find(": In "); fileContextPos != std::string::npos)
            {
                const std::string fileText = trimWhitespace(line.substr(0, fileContextPos));
                if (looksLikeSourceFilePath(fileText))
                {
                    BackendDiagnostic diagnostic;
                    diagnostic.severity = BackendDiagnosticSeverity::Note;
                    diagnostic.domain = BackendDiagnosticDomain::Compiler;
                    diagnostic.location.file = normalizeDiagnosticFilePath(fileText);
                    diagnostic.location = normalizeBackendLocation(std::move(diagnostic.location));
                    diagnostic.message = trimWhitespace(line.substr(fileContextPos + 2));
                    return diagnostic;
                }
            }

            if (std::regex_match(line, match, locationContextWithColumnPattern) &&
                looksLikeSourceFilePath(trimWhitespace(match[1].str())))
            {
                return buildNote(match[1].str(), match[2].str(), match[3].str(), match[4].str());
            }

            if (std::regex_match(line, match, locationContextWithoutColumnPattern) &&
                looksLikeSourceFilePath(trimWhitespace(match[1].str())))
            {
                return buildNote(match[1].str(), match[2].str(), "", match[3].str());
            }

            return std::nullopt;
        }

        std::optional<BackendDiagnostic> parseBackendDiagnosticLine(std::string_view rawLine)
        {
            static const std::regex gccWithColumnPattern(R"(^(.*):([0-9]+):([0-9]+):\s*(fatal error|error|warning|note):\s*(.*)$)");
            static const std::regex gccWithoutColumnPattern(R"(^(.*):([0-9]+):\s*(fatal error|error|warning|note):\s*(.*)$)");
            static const std::regex msvcPattern(R"(^(.*)\(([0-9]+)(?:,([0-9]+))?\):\s*(fatal error|error|warning|note)(?:\s+([A-Za-z]+[0-9]+))?:\s*(.*)$)");
            static const std::regex toolSeverityPattern(R"(^(.*?)(?:\s*:\s*|\s+)(fatal error|error|warning|note)(?:\s+([A-Za-z]+[0-9]+))?:\s*(.*)$)");
            static const std::regex linkerToolPattern(R"(^(.*?(?:ld(?:\.exe)?|collect2|link(?:\.exe)?|LINK))(?:\s*:\s*|\s+)(.*)$)", std::regex::icase);

            const std::string line(rawLine);
            std::smatch match;

            if (std::regex_match(line, match, gccWithColumnPattern))
            {
                const BackendDiagnosticDomain domain = classifyBackendTool(match[1].str());
                return BackendDiagnostic{
                    .severity = parseBackendSeverity(match[4].str()),
                    .domain = domain == BackendDiagnosticDomain::Unknown ? BackendDiagnosticDomain::Compiler : domain,
                    .location = buildBackendDiagnosticLocation(match[1].str(), match[2].str(), match[3].str()),
                    .message = normalizeBackendMessage(domain == BackendDiagnosticDomain::Unknown ? BackendDiagnosticDomain::Compiler : domain, match[5].str())
                };
            }

            if (std::regex_match(line, match, gccWithoutColumnPattern))
            {
                const BackendDiagnosticDomain domain = classifyBackendTool(match[1].str());
                return BackendDiagnostic{
                    .severity = parseBackendSeverity(match[3].str()),
                    .domain = domain == BackendDiagnosticDomain::Unknown ? BackendDiagnosticDomain::Compiler : domain,
                    .location = buildBackendDiagnosticLocation(match[1].str(), match[2].str(), ""),
                    .message = normalizeBackendMessage(domain == BackendDiagnosticDomain::Unknown ? BackendDiagnosticDomain::Compiler : domain, match[4].str())
                };
            }

            if (std::regex_match(line, match, msvcPattern))
            {
                const BackendDiagnosticDomain domain = classifyBackendTool(match[1].str());
                return BackendDiagnostic{
                    .severity = parseBackendSeverity(match[4].str()),
                    .domain = domain == BackendDiagnosticDomain::Unknown ? BackendDiagnosticDomain::Compiler : domain,
                    .location = buildBackendDiagnosticLocation(match[1].str(), match[2].str(), match[3].str()),
                    .code = trimWhitespace(match[5].str()),
                    .message = normalizeBackendMessage(domain == BackendDiagnosticDomain::Unknown ? BackendDiagnosticDomain::Compiler : domain, match[6].str())
                };
            }

            if (auto contextNote = parseBackendContextNoteLine(line); contextNote.has_value())
                return contextNote;

            if (std::regex_match(line, match, toolSeverityPattern))
            {
                const std::string toolText = trimWhitespace(match[1].str());
                const BackendDiagnosticDomain domain = classifyBackendTool(toolText);

                BackendDiagnostic diagnostic{
                    .severity = parseBackendSeverity(match[2].str()),
                    .domain = domain,
                    .code = trimWhitespace(match[3].str()),
                    .message = normalizeBackendMessage(domain, match[4].str())
                };

                if (looksLikeSourceFilePath(toolText))
                {
                    diagnostic.location.file = normalizeDiagnosticFilePath(toolText);
                    diagnostic.location = normalizeBackendLocation(std::move(diagnostic.location));
                }
                else
                    diagnostic.sourceLabel = backendDomainLabel(domain);

                return diagnostic;
            }

            if (std::regex_match(line, match, linkerToolPattern))
            {
                return BackendDiagnostic{
                    .severity = BackendDiagnosticSeverity::Error,
                    .domain = BackendDiagnosticDomain::Linker,
                    .sourceLabel = backendDomainLabel(BackendDiagnosticDomain::Linker),
                    .message = normalizeBackendMessage(BackendDiagnosticDomain::Linker, match[2].str())
                };
            }

            return std::nullopt;
        }

        bool isIgnorableBackendOutputLine(std::string_view rawLine)
        {
            const std::string line = trimWhitespace(rawLine);
            if (line.empty())
                return true;

            if (line.starts_with("^") || line.starts_with("|") || line.starts_with("~"))
                return true;

            if (const size_t pipePos = line.find('|'); pipePos != std::string::npos)
            {
                bool allDigitsBeforePipe = pipePos > 0;
                for (size_t i = 0; i < pipePos; ++i)
                {
                    if (!std::isspace(static_cast<unsigned char>(line[i])) && !std::isdigit(static_cast<unsigned char>(line[i])))
                    {
                        allDigitsBeforePipe = false;
                        break;
                    }
                }

                if (allDigitsBeforePipe)
                    return true;
            }

            return line.find("In function") != std::string::npos ||
                   line.find("In member function") != std::string::npos ||
                   line.find("required from") != std::string::npos ||
                   line == "compilation terminated.";
        }

        void logBackendDiagnostic(const BackendDiagnostic& diagnostic)
        {
            std::string message = diagnostic.message;
            if (!diagnostic.code.empty())
                message = diagnostic.code + ": " + message;

            const bool hasLocationLabel = diagnostic.location.hasFile();
            const bool isGeneratedSource = isGeneratedBackendFilePath(diagnostic.location.file);
            const bool isWioSource = isWioSourceFilePath(diagnostic.location.file);

            std::string label;
            if (hasLocationLabel)
            {
                if (isGeneratedSource)
                {
                    label = "backend:generated-cpp";
                    if (diagnostic.location.line != 0)
                    {
                        label += ":" + std::to_string(diagnostic.location.line);
                        if (diagnostic.location.column != 0)
                            label += ":" + std::to_string(diagnostic.location.column);
                    }
                }
                else
                {
                    label = diagnostic.location.toDiagnosticString();
                }
            }
            else
            {
                label = !diagnostic.sourceLabel.empty() ? diagnostic.sourceLabel : backendDomainLabel(diagnostic.domain);
            }

            if (isGeneratedSource)
                message = "Generated C++ wrapper: " + message;
            else if (!isWioSource)
                message = backendDomainPrefix(diagnostic.domain) + ": " + message;

            switch (diagnostic.severity)
            {
            case BackendDiagnosticSeverity::Warning:
                Logger::get().addWarning("Warning [{}]: {}", label, message);
                break;
            case BackendDiagnosticSeverity::Note:
                Logger::get().getLogger().info("Note [{}]: {}", label, message);
                break;
            case BackendDiagnosticSeverity::Error:
                Logger::get().addError("Error [{}]: {}", label, message);
                break;
            }
        }

        std::string buildBackendFailureSummary(std::string_view fallbackSummary,
                                               const std::vector<BackendDiagnostic>& diagnostics)
        {
            if (diagnostics.empty())
                return std::string(fallbackSummary);

            bool sawGeneratedCompilerDiagnostic = false;
            int compilerCount = 0;
            int linkerCount = 0;
            int archiverCount = 0;

            for (const auto& diagnostic : diagnostics)
            {
                if (diagnostic.severity != BackendDiagnosticSeverity::Error)
                    continue;

                switch (diagnostic.domain)
                {
                case BackendDiagnosticDomain::Compiler:
                    ++compilerCount;
                    if (isGeneratedBackendFilePath(diagnostic.location.file))
                        sawGeneratedCompilerDiagnostic = true;
                    break;
                case BackendDiagnosticDomain::Linker:
                    ++linkerCount;
                    break;
                case BackendDiagnosticDomain::Archiver:
                    ++archiverCount;
                    break;
                case BackendDiagnosticDomain::Unknown:
                    break;
                }
            }

            if (sawGeneratedCompilerDiagnostic)
                return "Generated C++ wrapper compilation failed";

            if (linkerCount > compilerCount && linkerCount >= archiverCount)
                return "Native linker failed";

            if (archiverCount > 0 && archiverCount >= compilerCount && archiverCount >= linkerCount)
                return "Native archiver failed";

            if (compilerCount > 0)
                return "Native C++ compiler failed";

            return std::string(fallbackSummary);
        }

        void reportBackendCommandFailure(std::string_view summary,
                                         int exitCode,
                                         const std::string& output,
                                         std::string_view command = {})
        {
            std::vector<BackendDiagnostic> diagnostics;
            diagnostics.reserve(8);

            for (const auto& line : splitLines(output))
            {
                if (auto diagnostic = parseBackendDiagnosticLine(line); diagnostic.has_value())
                    diagnostics.push_back(std::move(*diagnostic));
            }

            const bool hasSpecificDiagnostics = std::ranges::any_of(diagnostics, [](const BackendDiagnostic& diagnostic)
            {
                return !isGenericBackendSummaryMessage(diagnostic.message);
            });

            for (const auto& diagnostic : diagnostics)
            {
                if (hasSpecificDiagnostics && isGenericBackendSummaryMessage(diagnostic.message))
                    continue;

                logBackendDiagnostic(diagnostic);
            }

            const bool emittedStructuredDiagnostics = !diagnostics.empty();
            const std::string trimmedOutput = trimWhitespace(output);
            if (!emittedStructuredDiagnostics)
            {
                if (trimmedOutput.empty())
                    WIO_LOG_FATAL("{} with code: {}", summary, exitCode);
                else
                    WIO_LOG_FATAL("{} with code: {}\n{}", summary, exitCode, trimmedOutput);
                return;
            }

            std::vector<std::string> extraLines;
            for (const auto& line : splitLines(output))
            {
                if (parseBackendDiagnosticLine(line).has_value() || isIgnorableBackendOutputLine(line))
                    continue;

                extraLines.push_back(trimWhitespace(line));
            }

            if (!extraLines.empty())
            {
                std::string extraContext;
                for (const auto& line : extraLines)
                {
                    if (!extraContext.empty())
                        extraContext += "\n";
                    extraContext += line;
                }
                WIO_LOG_INFO("Backend context:\n{}", extraContext);
            }

            if (!trimWhitespace(command).empty())
                WIO_LOG_INFO("Backend command:\n{}", command);

            WIO_LOG_FATAL("{} with code: {}", buildBackendFailureSummary(summary, diagnostics), exitCode);
        }

        void dumpTokens(const std::vector<Token>& tokens)
        {
            for (const auto& token : tokens)
            {
                WIO_LOG_INFO(
                    "[{}:{}] {:<24} '{}'",
                    token.loc.line,
                    token.loc.column,
                    tokenTypeToString(token.type),
                    escapeTokenValueForDisplay(token.value)
                );
            }
        }

        std::string getTopLevelDeclarationName(const NodePtr<Statement>& statement)
        {
            if (!statement)
                return {};

            if (const auto* varDecl = statement->as<VariableDeclaration>())
                return varDecl->name ? varDecl->name->token.value : "";

            if (const auto* fnDecl = statement->as<FunctionDeclaration>())
                return fnDecl->name ? fnDecl->name->token.value : "";

            if (const auto* interfaceDecl = statement->as<InterfaceDeclaration>())
                return interfaceDecl->name ? interfaceDecl->name->token.value : "";

            if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                return componentDecl->name ? componentDecl->name->token.value : "";

            if (const auto* objectDecl = statement->as<ObjectDeclaration>())
                return objectDecl->name ? objectDecl->name->token.value : "";

            if (const auto* enumDecl = statement->as<EnumDeclaration>())
                return enumDecl->name ? enumDecl->name->token.value : "";

            if (const auto* flagsetDecl = statement->as<FlagsetDeclaration>())
                return flagsetDecl->name ? flagsetDecl->name->token.value : "";

            if (const auto* flagDecl = statement->as<FlagDeclaration>())
                return flagDecl->name ? flagDecl->name->token.value : "";

            if (const auto* realmDecl = statement->as<RealmDeclaration>())
                return realmDecl->name ? realmDecl->name->token.value : "";

            return {};
        }

        std::vector<std::string> collectExportedSymbols(const std::vector<NodePtr<Statement>>& statements)
        {
            std::vector<std::string> exportedSymbols;
            std::unordered_set<std::string> seenSymbols;

            for (const auto& statement : statements)
            {
                std::string symbolName = getTopLevelDeclarationName(statement);
                if (symbolName.empty())
                    continue;

                if (seenSymbols.insert(symbolName).second)
                    exportedSymbols.push_back(std::move(symbolName));
            }

            return exportedSymbols;
        }

        bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute attribute)
        {
            return std::ranges::any_of(attributes, [attribute](const NodePtr<AttributeStatement>& stmt)
            {
                return stmt && stmt->attribute == attribute;
            });
        }

        std::optional<std::string> getCppHeaderAttributeValue(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            for (const auto& attribute : attributes)
            {
                if (!attribute || attribute->attribute != Attribute::CppHeader)
                    continue;

                if (attribute->args.size() == 1 && attribute->args.front().type == TokenType::stringLiteral)
                    return attribute->args.front().value;

                return std::nullopt;
            }

            return std::nullopt;
        }

        void collectRequiredCppHeaders(const std::vector<NodePtr<Statement>>& statements,
                                       const std::filesystem::path& sourcePath,
                                       std::vector<RequiredCppHeader>& headers)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectRequiredCppHeaders(realmDecl->statements, sourcePath, headers);
                    continue;
                }

                if (const auto* useStmt = statement->as<UseStatement>())
                {
                    if (useStmt->isCppHeader && !useStmt->modulePath.empty())
                    {
                        headers.push_back({
                            .header = useStmt->modulePath,
                            .location = useStmt->location(),
                            .sourcePath = sourcePath,
                            .origin = "use @CppHeader"
                        });
                    }
                    continue;
                }

                if (const auto* fnDecl = statement->as<FunctionDeclaration>())
                {
                    if (!hasAttribute(fnDecl->attributes, Attribute::Native))
                        continue;

                    if (auto headerValue = getCppHeaderAttributeValue(fnDecl->attributes); headerValue.has_value())
                    {
                        headers.push_back({
                            .header = *headerValue,
                            .location = fnDecl->location(),
                            .sourcePath = sourcePath,
                            .origin = common::formatString(
                                "@CppHeader on native function '{}'",
                                fnDecl->name ? fnDecl->name->token.value : "<anonymous>"
                            )
                        });
                    }
                }
            }
        }

        std::vector<std::filesystem::path> buildHeaderSearchRoots(const std::filesystem::path& sourceDir,
                                                                  const std::vector<std::filesystem::path>& systemIncludeDirs,
                                                                  const std::vector<std::string>& includeDirs)
        {
            std::vector<std::filesystem::path> roots;

            auto appendRoot = [&](const std::filesystem::path& root)
            {
                if (root.empty())
                    return;

                std::filesystem::path absoluteRoot = std::filesystem::absolute(root).make_preferred();
                if (std::ranges::find(roots, absoluteRoot) == roots.end())
                    roots.push_back(std::move(absoluteRoot));
            };

            appendRoot(sourceDir);

            for (const auto& systemIncludeDir : systemIncludeDirs)
                appendRoot(systemIncludeDir);

            for (const auto& includeDir : includeDirs)
                appendRoot(includeDir);

            return roots;
        }

        std::string formatSearchRoots(const std::vector<std::filesystem::path>& roots)
        {
            std::string formattedRoots;
            for (size_t i = 0; i < roots.size(); ++i)
            {
                formattedRoots += roots[i].string();
                if (i + 1 < roots.size())
                    formattedRoots += ", ";
            }
            return formattedRoots;
        }

        bool canResolveCppHeader(const std::string& header, const std::vector<std::filesystem::path>& searchRoots)
        {
            std::filesystem::path headerPath(header);
            std::error_code ec;

            if (headerPath.is_absolute())
                return std::filesystem::exists(headerPath, ec) && !ec;

            for (const auto& root : searchRoots)
            {
                std::filesystem::path candidate = (root / headerPath).make_preferred();
                if (std::filesystem::exists(candidate, ec) && !ec)
                    return true;
            }

            return false;
        }

        bool validateRequiredCppHeaders(const std::vector<RequiredCppHeader>& headers,
                                        const std::filesystem::path& sourceDir,
                                        const std::vector<std::filesystem::path>& systemIncludeDirs,
                                        const std::vector<std::string>& includeDirs)
        {
            if (headers.empty())
                return true;

            const auto searchRoots = buildHeaderSearchRoots(sourceDir, systemIncludeDirs, includeDirs);
            const std::string formattedRoots = formatSearchRoots(searchRoots);
            bool isValid = true;

            for (const auto& header : headers)
            {
                if (canResolveCppHeader(header.header, searchRoots))
                    continue;

                WIO_LOG_ADD_ERROR(
                    header.location,
                    "Native C++ header '{}' referenced by {} in '{}' was not found in include search paths: {}",
                    header.header,
                    header.origin.empty() ? "@CppHeader" : header.origin,
                    header.sourcePath.empty() ? "<unknown>" : header.sourcePath.string(),
                    formattedRoots
                );
                isValid = false;
            }

            return isValid;
        }

        bool validateRequiredSystemIncludeDirectories(const std::vector<std::filesystem::path>& systemIncludeDirs)
        {
            bool isValid = true;

            for (const auto& includeDir : systemIncludeDirs)
            {
                if (includeDir.empty())
                    continue;

                std::filesystem::path resolvedPath = std::filesystem::absolute(includeDir).make_preferred();
                std::error_code ec;
                const bool exists = std::filesystem::exists(resolvedPath, ec) && !ec;
                const bool isDirectory = exists && std::filesystem::is_directory(resolvedPath, ec) && !ec;

                if (!exists || !isDirectory)
                {
                    Logger::get().addError(
                        "Required backend include directory '{}' was not found or is not a directory.",
                        resolvedPath.string()
                    );
                    isValid = false;
                }
            }

            return isValid;
        }

        bool validateSearchDirectories(const std::vector<std::string>& paths,
                                       std::string_view label)
        {
            bool isValid = true;

            for (const auto& rawPath : paths)
            {
                if (rawPath.empty())
                    continue;

                std::filesystem::path resolvedPath = std::filesystem::absolute(std::filesystem::path(rawPath)).make_preferred();
                std::error_code ec;
                const bool exists = std::filesystem::exists(resolvedPath, ec) && !ec;
                const bool isDirectory = exists && std::filesystem::is_directory(resolvedPath, ec) && !ec;

                if (!exists || !isDirectory)
                {
                    Logger::get().addError("{} '{}' was not found or is not a directory.", label, resolvedPath.string());
                    isValid = false;
                }
            }

            return isValid;
        }

        bool validateBackendFileInputs(const std::vector<std::string>& backendArgs,
                                       const std::vector<std::string>& linkLibraries)
        {
            bool isValid = true;

            for (const auto& backendArg : backendArgs)
            {
                if (!isSourceFilePath(backendArg))
                    continue;

                std::filesystem::path resolvedPath = std::filesystem::absolute(std::filesystem::path(backendArg)).make_preferred();
                std::error_code ec;
                const bool exists = std::filesystem::exists(resolvedPath, ec) && !ec;
                const bool isRegularFile = exists && std::filesystem::is_regular_file(resolvedPath, ec) && !ec;

                if (!exists || !isRegularFile)
                {
                    Logger::get().addError("Backend source file '{}' was not found.", resolvedPath.string());
                    isValid = false;
                }
            }

            for (const auto& linkLibrary : linkLibraries)
            {
                if (!isLibraryFilePath(linkLibrary) || linkLibrary.starts_with("-"))
                    continue;

                std::filesystem::path resolvedPath = std::filesystem::absolute(std::filesystem::path(linkLibrary)).make_preferred();
                std::error_code ec;
                const bool exists = std::filesystem::exists(resolvedPath, ec) && !ec;
                const bool isRegularFile = exists && std::filesystem::is_regular_file(resolvedPath, ec) && !ec;

                if (!exists || !isRegularFile)
                {
                    Logger::get().addError("Linked library file '{}' was not found.", resolvedPath.string());
                    isValid = false;
                }
            }

            return isValid;
        }

        bool validateBackendConfiguration(const std::vector<RequiredCppHeader>& requiredCppHeaders,
                                          const std::filesystem::path& sourceDir,
                                          const std::vector<std::filesystem::path>& systemIncludeDirs)
        {
            std::vector<std::string> moduleDirs = gAppData.argParser.GetValuesOf<std::string>("MODULE-DIR");
            std::vector<std::string> includeDirs = gAppData.argParser.GetValuesOf<std::string>("INCLUDE-DIR");
            std::vector<std::string> linkDirs = gAppData.argParser.GetValuesOf<std::string>("LINK-DIR");
            std::vector<std::string> linkLibraries = gAppData.argParser.GetValuesOf<std::string>("LINK-LIB");
            std::vector<std::string> backendArgs = gAppData.argParser.GetValuesOf<std::string>("BACKEND-ARG");

            bool isValid = true;
            isValid = validateRequiredSystemIncludeDirectories(systemIncludeDirs) && isValid;
            isValid = validateSearchDirectories(moduleDirs, "Module search directory") && isValid;
            isValid = validateSearchDirectories(includeDirs, "Include directory") && isValid;
            isValid = validateSearchDirectories(linkDirs, "Link directory") && isValid;
            isValid = validateBackendFileInputs(backendArgs, linkLibraries) && isValid;
            isValid = validateRequiredCppHeaders(requiredCppHeaders, sourceDir, systemIncludeDirs, includeDirs) && isValid;
            return isValid;
        }

        std::vector<std::filesystem::path> getUserModuleSearchDirs()
        {
            std::vector<std::filesystem::path> searchDirs;
            std::vector<std::string> configuredDirs = gAppData.argParser.GetValuesOf<std::string>("MODULE-DIR");
            searchDirs.reserve(configuredDirs.size());

            for (const auto& configuredDir : configuredDirs)
            {
                if (configuredDir.empty())
                    continue;

                searchDirs.push_back(std::filesystem::absolute(std::filesystem::path(configuredDir)).make_preferred());
            }

            return searchDirs;
        }

        std::vector<std::filesystem::path> buildModuleSearchRoots(bool isStdLib, const std::filesystem::path& currentDir)
        {
            std::vector<std::filesystem::path> roots;

            if (isStdLib)
            {
                std::filesystem::path stdSourceDir = getStdSourceDir();
                if (!stdSourceDir.empty())
                    roots.push_back(std::filesystem::absolute(stdSourceDir).make_preferred());

                return roots;
            }

            roots.push_back(std::filesystem::absolute(currentDir).make_preferred());

            for (const auto& searchDir : getUserModuleSearchDirs())
            {
                if (std::ranges::find(roots, searchDir) == roots.end())
                    roots.push_back(searchDir);
            }

            return roots;
        }

        std::optional<std::filesystem::path> resolveModuleSourcePath(const std::string& modulePath, bool isStdLib, const std::filesystem::path& currentDir)
        {
            const std::filesystem::path relativeModulePath = std::filesystem::path(modulePath + ".wio").make_preferred();

            for (const auto& searchRoot : buildModuleSearchRoots(isStdLib, currentDir))
            {
                std::filesystem::path candidatePath = (searchRoot / relativeModulePath).make_preferred();
                std::error_code ec;
                if (std::filesystem::exists(candidatePath, ec) && !ec)
                    return std::filesystem::absolute(candidatePath).make_preferred();
            }

            return std::nullopt;
        }

        std::string formatModuleSearchRoots(bool isStdLib, const std::filesystem::path& currentDir)
        {
            std::string formattedRoots;
            const auto roots = buildModuleSearchRoots(isStdLib, currentDir);

            for (size_t i = 0; i < roots.size(); ++i)
            {
                formattedRoots += roots[i].string();
                if (i + 1 < roots.size())
                    formattedRoots += ", ";
            }

            return formattedRoots;
        }
    }
    
    Compiler::Compiler()
    {
        gAppData.argParser = Argonaut::Parser();
        gAppData.argParser
            .Add(
                Argonaut::Argument("FILE")
                    .Required()
                    .SetDescription("File to be compiled.")
            )
            .Add(
                Argonaut::Argument("SINGLE-FILE")
                    .AddAlias("-S")
                    .AddAlias("--single-file")
                    .Flag()
                    .SetDescription("Imports are ignored (Built-in imports are not). It is treated as a single file.")
            )
            .Add(
                Argonaut::Argument("SHOW-TOKENS")
                    .AddAlias("-t")
                    .AddAlias("--show-tokens")
                    .Flag()
                    .SetDescription("Shows tokens.")
            )
            .Add(
                Argonaut::Argument("SHOW-AST")
                    .AddAlias("-a")
                    .AddAlias("--show-ast")
                    .Flag()
                    .SetDescription("Shows AST.")
            )
            .Add(
                Argonaut::Argument("DRY-RUN")
                    .AddAlias("-d")
                    .AddAlias("--dry-run")
                    .Flag()
                    .SetDescription("Builds the AST but does not execute the program.")
            )
            .Add(
                Argonaut::Argument("NO-BUILTIN")
                    .AddAlias("-B")
                    .AddAlias("--no-builtin")
                    .Flag()
                    .SetDescription("Wio built-in library imports are ignored.")
            )
            .Add(
                Argonaut::Argument("WARN-AS-ERROR")
                    .AddAlias("-w")
                    .AddAlias("--warn-as-error")
                    .Flag()
                    .SetDescription("Treat all warnings as errors.")
            )
            .Add(
                Argonaut::Argument("RUN")
                    .AddAlias("-r")
                    .AddAlias("--run")
                    .Flag()
                    .SetDescription("Compiles and then runs the output executable.")
            )
            .Add(
                Argonaut::Argument("TARGET")
                    .AddAlias("--target")
                    .SetDescription("Selects backend output kind: exe, static, or shared.")
            )
            .Add(
                Argonaut::Argument("OUTPUT")
                    .AddAlias("-o")
                    .AddAlias("--output")
                    .SetDescription("Overrides the backend output path.")
            )
            .Add(
                Argonaut::Argument("INCLUDE-DIR")
                    .AddAlias("--include-dir")
                    .MultiValue()
                    .SetDescription("Adds an extra include directory for the backend C++ compiler.")
            )
            .Add(
                Argonaut::Argument("MODULE-DIR")
                    .AddAlias("--module-dir")
                    .MultiValue()
                    .SetDescription("Adds an extra Wio module search directory for user imports.")
            )
            .Add(
                Argonaut::Argument("LINK-DIR")
                    .AddAlias("--link-dir")
                    .MultiValue()
                    .SetDescription("Adds an extra library search directory for the backend C++ compiler.")
            )
            .Add(
                Argonaut::Argument("LINK-LIB")
                    .AddAlias("--link-lib")
                    .MultiValue()
                    .SetDescription("Adds an extra library or library file for the backend C++ linker.")
            )
            .Add(
                Argonaut::Argument("BACKEND-ARG")
                    .AddAlias("--backend-arg")
                    .MultiValue()
                    .SetDescription("Passes an extra raw argument to the backend C++ compiler.")
            )
            .AutoHelp()
            .AutoVersion()
            .SetVersion("0.0.1-alpha");
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    void Compiler::loadArgs(int argc, char* argv[]) const
    {
        try
        {
            gAppData.executablePath.clear();
            if (argc > 0 && argv != nullptr && argv[0] != nullptr && argv[0][0] != '\0')
            {
                std::error_code ec;
                gAppData.executablePath = std::filesystem::weakly_canonical(std::filesystem::absolute(std::filesystem::path(argv[0])), ec);
                if (ec)
                    gAppData.executablePath = std::filesystem::absolute(std::filesystem::path(argv[0])).make_preferred();
                else
                    gAppData.executablePath = gAppData.executablePath.make_preferred();
            }

            gAppData.flags.setAll(false);
            gAppData.argParser.Parse(argc, argv);
        }
        catch (const Argonaut::HelpRequestedException& e)
        {
            WIO_LOG_INFO("{}", e.what());
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_SUCCESS);
        }
        catch (const Argonaut::VersionRequestedException& e)
        {
            WIO_LOG_INFO("{}", e.what());
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_SUCCESS);
        }
        catch (const Argonaut::ParsePrepException& e)
        {
            WIO_LOG_ERROR("Argument parse prep failed: {}", e.what());
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_FAILURE);
        }
        catch (const Argonaut::ParseException& e)
        {
            WIO_LOG_ERROR("Argument parsing failed: {}", e.what());
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_FAILURE);
        }
        catch (const std::exception& e)
        {
            WIO_LOG_ERROR("Unexpected Error: {}", e.what());
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_FAILURE);
        }
        catch (...)
        {
            WIO_LOG_ERROR("Unknown Error!");
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_FAILURE);
        }
        
        try
        {

#define DEFINE_FLAG_VALUE(FLAG_ID, FLAG_NAME)                                           \
    auto FLAG_NAME##_Vec = gAppData.argParser.GetValuesOf<bool>(FLAG_ID);               \
    bool FLAG_NAME##_Val = (FLAG_NAME##_Vec).empty() ? false : (FLAG_NAME##_Vec).at(0); \
    gAppData.flags.set_##FLAG_NAME(FLAG_NAME##_Val)
            
            DEFINE_FLAG_VALUE("SINGLE-FILE", SingleFile);
            DEFINE_FLAG_VALUE("SHOW-TOKENS", ShowTokens);
            DEFINE_FLAG_VALUE("SHOW-AST", ShowAst);
            DEFINE_FLAG_VALUE("DRY-RUN", DryRun);
            DEFINE_FLAG_VALUE("NO-BUILTIN", NoBuiltin);
            DEFINE_FLAG_VALUE("WARN-AS-ERROR", WarnAsError);
            DEFINE_FLAG_VALUE("RUN", Run);
            
#undef DEFINE_FLAG_VALUE

            std::vector<std::string> buildTargetValues = gAppData.argParser.GetValuesOf<std::string>("TARGET");
            std::string buildTargetValue = buildTargetValues.empty() ? "exe" : buildTargetValues.front();
            gAppData.buildTarget = parseBuildTarget(buildTargetValue);
        }
        catch (const std::exception& e)
        {
            WIO_LOG_ERROR("Unexpected Error: {}", e.what());
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            exit(EXIT_FAILURE);
        }
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    int Compiler::compile() const
    {
        try
        {
            std::vector<std::string> filePaths = gAppData.argParser.GetValuesOf<std::string>("FILE");
            std::string filePathStr = filePaths.at(0);
            std::filesystem::path sourcePath(filePathStr);
            gAppData.basePath = std::filesystem::absolute(sourcePath).parent_path();
            
            std::string source = filesystem::readFile(sourcePath);
            
            if (source.empty())
            {
                WIO_LOG_ERROR("File is empty or not found: {}", filePathStr);
                return EXIT_FAILURE;
            }

            filesystem::stripBOM(source);

            // 1. Lexer
            const std::string sourceDisplayPath = std::filesystem::absolute(sourcePath).make_preferred().string();
            Lexer lexer(source, sourceDisplayPath);
            auto tokens = lexer.lex();

            if (gAppData.flags.get_ShowTokens())
            {
                dumpTokens(tokens);
            }

            // 2. Parser
            Parser parser(std::move(tokens));
            auto program = parser.parseProgram();
            
            gAppData.loadedModules.clear();
            gAppData.requiredCppHeaders.clear();
            gAppData.loadedModules.insert(sourceDisplayPath);
            collectRequiredCppHeaders(program->statements, std::filesystem::path(sourceDisplayPath), gAppData.requiredCppHeaders);

            std::vector<NodePtr<Statement>> finalStatements;

            for (auto& stmt : program->statements)
            {
                if (stmt->is<UseStatement>())
                {
                    auto useStmt = stmt->as<UseStatement>();
                    if (useStmt->isStdLib)
                    {
                        if (!gAppData.flags.get_NoBuiltin())
                        {
                            std::vector<std::string> importedSymbols;
                            auto moduleProg = parseAndMerge(useStmt->modulePath, true, sourcePath.parent_path(), &importedSymbols);
                            for (auto& modStmt : moduleProg->statements)
                                finalStatements.push_back(std::move(modStmt));

                            if (!useStmt->aliasName.empty())
                            {
                                useStmt->importedSymbols = std::move(importedSymbols);
                                finalStatements.push_back(std::move(stmt));
                            }
                        }
                    }
                    else
                    {
                        if (!gAppData.flags.get_SingleFile())
                        {
                            if (useStmt->isCppHeader)
                            {
                                finalStatements.push_back(std::move(stmt));
                            }
                            else
                            {
                                std::vector<std::string> importedSymbols;
                                auto moduleProg = parseAndMerge(useStmt->modulePath, useStmt->isStdLib, sourcePath.parent_path(), &importedSymbols);
                                for (auto& modStmt : moduleProg->statements)
                                {
                                    finalStatements.push_back(std::move(modStmt));
                                }

                                if (!useStmt->aliasName.empty())
                                {
                                    useStmt->importedSymbols = std::move(importedSymbols);
                                    finalStatements.push_back(std::move(stmt));
                                }
                            }
                        }
                    }
                }
                else
                {
                    finalStatements.push_back(std::move(stmt));
                }
            }
            
            program->statements = std::move(finalStatements);

            // 3. Semantic Analysis
            sema::SemanticAnalyzer analyzer;
            analyzer.analyze(program);

            std::filesystem::path runtimeIncludeDir = getRuntimeIncludeDir();
            std::filesystem::path sdkIncludeDir = getSdkIncludeDir();
            const std::vector<std::filesystem::path> systemIncludeDirs =
                getBackendSystemIncludeDirs(runtimeIncludeDir, sdkIncludeDir);
            validateBackendConfiguration(gAppData.requiredCppHeaders, sourcePath.parent_path(), systemIncludeDirs);

            WIO_LOG_PROCESS_WARNINGS();
            WIO_LOG_PROCESS_ERRORS(CompilationError);

            if (gAppData.flags.get_DryRun())
            {
                WIO_LOG_INFO("Dry run completed successfully.");
                return EXIT_SUCCESS;
            }

            // 4. Code Generation
            codegen::CppGenerator generator;
            std::string cppCode = generator.generate(program);

            // main.wio -> main.wio.cpp
            auto cppPath = sourcePath;
            cppPath += ".cpp";
            
            if (!filesystem::writeFilepath(cppCode, cppPath))
            {
                WIO_LOG_FATAL("Generated C++ output could not be written to: {}", cppPath.string());
                return EXIT_FAILURE;
            }
            
            if (runtimeIncludeDir.empty() || !std::filesystem::exists(runtimeIncludeDir))
            {
                WIO_LOG_FATAL("Runtime headers were not found. Expected directory: {}", runtimeIncludeDir.string());
                return EXIT_FAILURE;
            }

            if (sdkIncludeDir.empty() || !std::filesystem::exists(sdkIncludeDir))
            {
                WIO_LOG_FATAL("SDK headers were not found. Expected directory: {}", sdkIncludeDir.string());
                return EXIT_FAILURE;
            }

            std::filesystem::path runtimeLibraryPath = getRuntimeLibraryFile();
            if ((gAppData.buildTarget == BuildTarget::Executable || gAppData.buildTarget == BuildTarget::SharedLibrary) &&
                (runtimeLibraryPath.empty() || !std::filesystem::exists(runtimeLibraryPath)))
            {
                WIO_LOG_FATAL("Runtime library was not found. Expected file: {}", runtimeLibraryPath.string());
                return EXIT_FAILURE;
            }

            if (gAppData.flags.get_Run() && gAppData.buildTarget != BuildTarget::Executable)
            {
                WIO_LOG_FATAL("--run is only supported when --target exe is active.");
                return EXIT_FAILURE;
            }

            std::vector<std::string> linkDirs = gAppData.argParser.GetValuesOf<std::string>("LINK-DIR");
            std::vector<std::string> linkLibraries = gAppData.argParser.GetValuesOf<std::string>("LINK-LIB");
            std::vector<std::string> backendArgs = gAppData.argParser.GetValuesOf<std::string>("BACKEND-ARG");
            std::vector<std::string> outputPaths = gAppData.argParser.GetValuesOf<std::string>("OUTPUT");
            std::vector<std::string> includeDirs = gAppData.argParser.GetValuesOf<std::string>("INCLUDE-DIR");

            std::filesystem::path outputPath =
                outputPaths.empty()
                    ? makeDefaultOutputPath(sourcePath, gAppData.buildTarget)
                    : std::filesystem::path(outputPaths.front());

            outputPath = std::filesystem::absolute(outputPath).make_preferred();
            std::filesystem::create_directories(outputPath.parent_path());

            std::string backendCompiler = getBackendCompiler();
            int exitCode = EXIT_SUCCESS;

            if (gAppData.buildTarget == BuildTarget::StaticLibrary)
            {
                std::vector<std::string> backendSourceFiles;
                std::vector<std::string> backendCompilerArgs;
                for (const auto& backendArg : backendArgs)
                {
                    if (isSourceFilePath(backendArg))
                        backendSourceFiles.push_back(backendArg);
                    else
                        backendCompilerArgs.push_back(backendArg);
                }

                std::vector<std::filesystem::path> objectFiles;
                objectFiles.reserve(1 + backendSourceFiles.size());

                auto buildObjectPath = [&](const std::filesystem::path& inputPath, size_t index)
                {
                    std::string stem = inputPath.stem().string();
                    std::filesystem::path objectPath = outputPath.parent_path() /
                        (outputPath.stem().string() + "." + stem + "." + std::to_string(index) + ".o");
                    return objectPath.make_preferred();
                };

                auto compileObject = [&](const std::filesystem::path& inputPath, size_t index) -> CommandResult
                {
                    std::filesystem::path objectPath = buildObjectPath(inputPath, index);
                    objectFiles.push_back(objectPath);

                    std::stringstream compileCmd;
                    compileCmd << quoteCommand(backendCompiler);
                    compileCmd << " -std=c++20 -c ";
                    compileCmd << quotePath(inputPath);
                    appendIncludeDirectories(compileCmd, systemIncludeDirs);
                    appendIncludeDirectories(compileCmd, includeDirs);
                    appendBackendArguments(compileCmd, backendCompilerArgs);
                    compileCmd << " -o " << quotePath(objectPath);

                    return runCommandCaptureOutput(compileCmd.str());
                };

                CommandResult compileResult = compileObject(std::filesystem::absolute(cppPath).make_preferred(), 0);
                exitCode = compileResult.exitCode;
                if (exitCode == 0)
                {
                    for (size_t i = 0; i < backendSourceFiles.size(); ++i)
                    {
                        compileResult = compileObject(std::filesystem::absolute(backendSourceFiles[i]).make_preferred(), i + 1);
                        exitCode = compileResult.exitCode;
                        if (exitCode != 0)
                            break;
                    }
                }

                if (exitCode != 0)
                {
                    reportBackendCommandFailure("Backend object compilation failed", exitCode, compileResult.output, compileResult.command);
                    return EXIT_FAILURE;
                }

                std::stringstream archiveCmd;
                archiveCmd << quoteCommand(getBackendArchiver());
                archiveCmd << " rcs " << quotePath(outputPath);
                for (const auto& objectFile : objectFiles)
                    archiveCmd << " " << quotePath(objectFile);

                const CommandResult archiveResult = runCommandCaptureOutput(archiveCmd.str());
                exitCode = archiveResult.exitCode;
                if (exitCode != 0)
                {
                    reportBackendCommandFailure("Static library archive creation failed", exitCode, archiveResult.output, archiveResult.command);
                    return EXIT_FAILURE;
                }
            }
            else
            {
                std::stringstream cmd;
                cmd << quoteCommand(backendCompiler);
                cmd << " -std=c++20 ";

                if (gAppData.buildTarget == BuildTarget::SharedLibrary)
                {
#ifndef _WIN32
                    cmd << "-fPIC ";
#endif
                    cmd << "-shared ";
                }

                cmd << quotePath(cppPath);
                appendIncludeDirectories(cmd, systemIncludeDirs);
                appendIncludeDirectories(cmd, includeDirs);
                appendBackendArguments(cmd, backendArgs);
                appendLinkDirectories(cmd, linkDirs);
                appendLinkLibraries(cmd, linkLibraries);
                cmd << " " << quotePath(runtimeLibraryPath);
                cmd << " -o " << quotePath(outputPath);

                const CommandResult backendResult = runCommandCaptureOutput(cmd.str());
                exitCode = backendResult.exitCode;
                
                if (exitCode != 0)
                {
                    reportBackendCommandFailure("Backend compilation failed", exitCode, backendResult.output, backendResult.command);
                    return EXIT_FAILURE;
                }
            }

            WIO_LOG_INFO("Generated backend output: {}", outputPath.filename().string());

            if (gAppData.flags.get_Run())
            {
                WIO_LOG_INFO("Running {} ...", outputPath.filename().string());
                
                std::stringstream runCmd;

                const std::filesystem::path& finalExePath = outputPath;
                
                runCmd << "\"" << finalExePath.string() << "\"";
                
                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                int runExitCode = std::system(runCmd.str().c_str());
                if (runExitCode != 0)
                {
                    WIO_LOG_WARN("Program exited with code: {}", runExitCode);
                    return runExitCode;
                }
            }

            return EXIT_SUCCESS;
        }
        catch (const std::exception& e)
        {
            WIO_LOG_FATAL("Compiling failed: {}", e.what());
            return EXIT_FAILURE;
        }
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    sema::TypeContext& Compiler::getTypeContext() const
    {
        return gAppData.typeContext_;
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    CompilerFlags Compiler::getFlags() const
    {
        return gAppData.flags;
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    BuildTarget Compiler::getBuildTarget() const
    {
        return gAppData.buildTarget;
    }

    Compiler& Compiler::get()
    {
        static Compiler instance;
        return instance;
    }

    Ref<Program> Compiler::parseAndMerge(const std::string& modulePath, bool isStdLib, const std::filesystem::path& currentDir, std::vector<std::string>* exportedSymbols)
    {
        std::optional<std::filesystem::path> resolvedModulePath = resolveModuleSourcePath(modulePath, isStdLib, currentDir);
        if (!resolvedModulePath.has_value())
        {
            WIO_LOG_ERROR(
                "Module file was not found: {}.wio (searched in: {})",
                modulePath,
                formatModuleSearchRoots(isStdLib, currentDir)
            );

            if (exportedSymbols)
                exportedSymbols->clear();
            return makeNodePtr<Program>(std::vector<NodePtr<Statement>>{});
        }

        std::filesystem::path actualPath = filesystem::getCanonicalPath(*resolvedModulePath).make_preferred();
        std::string absolutePath = actualPath.string();

        if (gAppData.loadedModules.contains(absolutePath))
        {
            return makeNodePtr<Program>(std::vector<NodePtr<Statement>>{});
        }
        gAppData.loadedModules.insert(absolutePath);

        std::string source = filesystem::readFile(actualPath);
        if (source.empty())
        {
            WIO_LOG_ERROR("Module file is empty or not found: {}", actualPath.string());
            return makeNodePtr<Program>(std::vector<NodePtr<Statement>>{});
        }
        filesystem::stripBOM(source);

        Lexer lexer(source, actualPath.string());
        Parser parser(lexer.lex());
        auto subProgram = parser.parseProgram();
        collectRequiredCppHeaders(subProgram->statements, actualPath, gAppData.requiredCppHeaders);

        std::vector<std::string> moduleExportedSymbols = collectExportedSymbols(subProgram->statements);
        if (exportedSymbols)
            *exportedSymbols = moduleExportedSymbols;

        std::vector<NodePtr<Statement>> mergedStatements;

        for (auto& stmt : subProgram->statements)
        {
            if (stmt->is<UseStatement>())
            {
                auto useStmt = stmt->as<UseStatement>();
                if (useStmt->isStdLib)
                {
                    if (!gAppData.flags.get_NoBuiltin())
                    {
                        mergedStatements.push_back(std::move(stmt));
                    }
                }
                else
                {
                    if (!gAppData.flags.get_SingleFile())
                    {
                        if (useStmt->isCppHeader)
                        {
                            mergedStatements.push_back(std::move(stmt));
                        }
                        else
                        {
                            std::vector<std::string> childExportedSymbols;
                            auto childProgram = parseAndMerge(useStmt->modulePath, useStmt->isStdLib, actualPath.parent_path(), &childExportedSymbols);
                            for (auto& childStmt : childProgram->statements)
                            {
                                mergedStatements.push_back(std::move(childStmt));
                            }

                            if (!useStmt->aliasName.empty())
                            {
                                useStmt->importedSymbols = std::move(childExportedSymbols);
                                mergedStatements.push_back(std::move(stmt));
                            }
                        }
                    }
                }
            }
            else
            {
                mergedStatements.push_back(std::move(stmt));
            }
        }

        return makeNodePtr<Program>(std::move(mergedStatements));
    }
}
