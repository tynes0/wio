#include "compiler.h"

#include <cstdlib>
#include <filesystem>
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

        std::string getBackendCompiler()
        {
#ifdef WIO_BACKEND_CXX_COMPILER
            return WIO_BACKEND_CXX_COMPILER;
#else
            return "g++";
#endif
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
                            finalStatements.push_back(std::move(stmt));
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
            
            std::filesystem::path exePath = sourcePath.parent_path() / sourcePath.stem();

#ifdef _WIN32
            exePath.replace_extension(".exe");
#endif

            std::filesystem::path runtimePath = getRuntimeIncludeDir();
            if (runtimePath.empty() || !std::filesystem::exists(runtimePath))
            {
                WIO_LOG_FATAL("Runtime headers were not found. Expected directory: {}", runtimePath.string());
                return EXIT_FAILURE;
            }

            std::string backendCompiler = getBackendCompiler();
            std::stringstream cmd;
            cmd << quoteCommand(backendCompiler);
            cmd << " -std=c++20 ";
            cmd << quotePath(cppPath);
            cmd << " -I" << quotePath(runtimePath);
            cmd << " -o " << quotePath(exePath);
            
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            int exitCode = std::system(cmd.str().c_str());
            
            if (exitCode != 0)
            {
                WIO_LOG_FATAL("Backend compilation failed with code: {}", exitCode);
                return EXIT_FAILURE;
            }

            if (gAppData.flags.get_Run())
            {
                WIO_LOG_INFO("Running {} ...", exePath.filename().string());
                
                std::stringstream runCmd;
                
                std::filesystem::path finalExePath = std::filesystem::absolute(exePath).make_preferred();
                
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

    Compiler& Compiler::get()
    {
        static Compiler instance;
        return instance;
    }

    Ref<Program> Compiler::parseAndMerge(const std::string& modulePath, bool isStdLib, const std::filesystem::path& currentDir, std::vector<std::string>* exportedSymbols)
    {
        if (isStdLib)
        {
            if (exportedSymbols)
                exportedSymbols->clear();
            return makeNodePtr<Program>(std::vector<NodePtr<Statement>>{});
        }

        std::filesystem::path actualPath = currentDir / (modulePath + ".wio");
        std::string absolutePath = std::filesystem::absolute(actualPath).string();

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
