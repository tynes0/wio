#include "compiler.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <argonaut.h>

#include "wio/codegen/cpp_generator.h"
#include "wio/common/filesystem/filesystem.h"
#include "wio/common/logger.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"

namespace wio
{
    struct AppData
    {
        std::filesystem::path basePath;
        CompilerFlags flags;
        Argonaut::Parser argParser;
        sema::TypeContext typeContext_;
        std::unordered_set<std::string> loadedModules;
        BuildTarget buildTarget = BuildTarget::Executable;
    };
    
    namespace
    {
        AppData gAppData;

        std::filesystem::path getRuntimeIncludeDir()
        {
#ifdef WIO_RUNTIME_INCLUDE_DIR
            return std::filesystem::path(WIO_RUNTIME_INCLUDE_DIR);
#else
            return {};
#endif
        }

        std::filesystem::path getStdSourceDir()
        {
#ifdef WIO_STD_SOURCE_DIR
            return std::filesystem::path(WIO_STD_SOURCE_DIR);
#else
            return {};
#endif
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
            gAppData.loadedModules.insert(std::filesystem::absolute(sourcePath).string());

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
                else
                {
                    finalStatements.push_back(std::move(stmt));
                }
            }
            
            program->statements = std::move(finalStatements);

            // 3. Semantic Analysis
            sema::SemanticAnalyzer analyzer;
            analyzer.analyze(program);

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
            
            std::filesystem::path runtimePath = getRuntimeIncludeDir();
            if (runtimePath.empty() || !std::filesystem::exists(runtimePath))
            {
                WIO_LOG_FATAL("Runtime headers were not found. Expected directory: {}", runtimePath.string());
                return EXIT_FAILURE;
            }

            if (gAppData.flags.get_Run() && gAppData.buildTarget != BuildTarget::Executable)
            {
                WIO_LOG_FATAL("--run is only supported when --target exe is active.");
                return EXIT_FAILURE;
            }

            std::vector<std::string> includeDirs = gAppData.argParser.GetValuesOf<std::string>("INCLUDE-DIR");
            std::vector<std::string> linkDirs = gAppData.argParser.GetValuesOf<std::string>("LINK-DIR");
            std::vector<std::string> linkLibraries = gAppData.argParser.GetValuesOf<std::string>("LINK-LIB");
            std::vector<std::string> backendArgs = gAppData.argParser.GetValuesOf<std::string>("BACKEND-ARG");
            std::vector<std::string> outputPaths = gAppData.argParser.GetValuesOf<std::string>("OUTPUT");

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
                    compileCmd << " -I" << quotePath(runtimePath);
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
                cmd << " -I" << quotePath(runtimePath);
                appendIncludeDirectories(cmd, includeDirs);
                appendBackendArguments(cmd, backendArgs);
                appendLinkDirectories(cmd, linkDirs);
                appendLinkLibraries(cmd, linkLibraries);
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
                
                std::filesystem::path finalExePath = outputPath;
                
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
            else
            {
                mergedStatements.push_back(std::move(stmt));
            }
        }

        return makeNodePtr<Program>(std::move(mergedStatements));
    }
}
