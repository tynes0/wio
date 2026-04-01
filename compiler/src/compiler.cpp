#include "compiler.h"

#include <filesystem>
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
    
    namespace{ AppData gAppData; }
    
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
    void Compiler::compile() const
    {
        try
        {
            std::vector<std::string> filePaths = gAppData.argParser.GetValuesOf<std::string>("FILE");
            std::string filePathStr = filePaths.at(0);
            std::filesystem::path sourcePath(filePathStr);
            
            std::string source = filesystem::readFile(sourcePath);
            
            if (source.empty())
            {
                WIO_LOG_ERROR("File is empty or not found: {}", filePathStr);
                return;
            }

            filesystem::stripBOM(source);

            // 1. Lexer
            Lexer lexer(source);
            auto tokens = lexer.lex();

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
                        finalStatements.push_back(std::move(stmt));
                    }
                    else
                    {
                        auto moduleProg = parseAndMerge(useStmt->modulePath, useStmt->isStdLib, sourcePath.parent_path());
                        for (auto& modStmt : moduleProg->statements)
                        {
                            finalStatements.push_back(std::move(modStmt));
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

            // 4. Code Generation
            codegen::CppGenerator generator;
            std::string cppCode = generator.generate(program);

            // main.wio -> main.wio.cpp
            auto cppPath = sourcePath;
            cppPath += ".cpp";
            
            filesystem::writeFilepath(cppCode, cppPath);
            
            std::filesystem::path exePath = sourcePath.parent_path() / sourcePath.stem();
            
#ifdef _WIN32
            exePath.replace_extension(".exe");
            
            std::filesystem::path compilerPath = std::filesystem::current_path(); 
            std::filesystem::path rootPath = compilerPath.parent_path().parent_path();
    
            std::filesystem::path runtimePath = rootPath / "compiler" / "include"/ "runtime";
            
            std::stringstream cmd;
            cmd << "g++ -std=c++20 \"";
            cmd << cppPath.string();
            cmd << "\"";
            cmd << " -I\"";
            cmd << runtimePath.string();
            cmd << "\"";
            cmd << " -o \"";
            cmd << exePath.string();
            cmd << "\"";

            std::string s = cmd.str();
            
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            int exitCode = std::system(cmd.str().c_str());
            
            if (exitCode != 0)
            {
                WIO_LOG_FATAL("Backend compilation failed with code: {}", exitCode);
                return;
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
                }
            }
#endif
        }
        catch (const std::exception& e)
        {
            WIO_LOG_FATAL("Compiling failed: {}", e.what());
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

    Ref<Program> Compiler::parseAndMerge(const std::string& modulePath, bool isStdLib, const std::filesystem::path& currentDir)
    {
        if (isStdLib)
        {
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

        std::vector<NodePtr<Statement>> mergedStatements;

        for (auto& stmt : subProgram->statements)
        {
            if (stmt->is<UseStatement>())
            {
                auto useStmt = stmt->as<UseStatement>();
                if (useStmt->isStdLib)
                {
                    mergedStatements.push_back(std::move(stmt));
                }
                else
                {
                    auto childProgram = parseAndMerge(useStmt->modulePath, useStmt->isStdLib, actualPath.parent_path());
                    for (auto& childStmt : childProgram->statements)
                    {
                        mergedStatements.push_back(std::move(childStmt));
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
