#include "compiler.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <optional>
#include <string_view>
#include <sstream>
#include <unordered_set>
#include <argonaut.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
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
            Lexer lexer(source);
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
            gAppData.loadedModules.insert(std::filesystem::absolute(sourcePath).string());
            collectRequiredCppHeaders(program->statements, std::filesystem::absolute(sourcePath).make_preferred(), gAppData.requiredCppHeaders);

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

                auto compileObject = [&](const std::filesystem::path& inputPath, size_t index) -> int
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

                    // NOLINTNEXTLINE(concurrency-mt-unsafe)
                    return std::system(compileCmd.str().c_str());
                };

                exitCode = compileObject(std::filesystem::absolute(cppPath).make_preferred(), 0);
                if (exitCode == 0)
                {
                    for (size_t i = 0; i < backendSourceFiles.size(); ++i)
                    {
                        exitCode = compileObject(std::filesystem::absolute(backendSourceFiles[i]).make_preferred(), i + 1);
                        if (exitCode != 0)
                            break;
                    }
                }

                if (exitCode != 0)
                {
                    WIO_LOG_FATAL("Backend object compilation failed with code: {}", exitCode);
                    return EXIT_FAILURE;
                }

                std::stringstream archiveCmd;
                archiveCmd << quoteCommand(getBackendArchiver());
                archiveCmd << " rcs " << quotePath(outputPath);
                for (const auto& objectFile : objectFiles)
                    archiveCmd << " " << quotePath(objectFile);

                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                exitCode = std::system(archiveCmd.str().c_str());
                if (exitCode != 0)
                {
                    WIO_LOG_FATAL("Static library archive creation failed with code: {}", exitCode);
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
                
                // NOLINTNEXTLINE(concurrency-mt-unsafe)
                exitCode = std::system(cmd.str().c_str());
                
                if (exitCode != 0)
                {
                    WIO_LOG_FATAL("Backend compilation failed with code: {}", exitCode);
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

        Lexer lexer(source);
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
