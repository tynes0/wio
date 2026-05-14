#include "file_cli.h"

#include "compiler.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace wio::tooling::file
{
    namespace
    {
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

        std::filesystem::path resolveDefaultSourcePath()
        {
            if (const auto repoRoot = tryFindRepoRoot(); repoRoot.has_value())
                return std::filesystem::absolute(*repoRoot / "playground" / "main.wio").make_preferred();

            return std::filesystem::absolute(std::filesystem::path("playground") / "main.wio").make_preferred();
        }

        std::vector<std::string> buildCompilerArgs(const std::filesystem::path& executablePath,
                                                   const std::string& mode,
                                                   const std::filesystem::path& sourcePath,
                                                   const std::vector<std::string>& compilerArgs,
                                                   const std::vector<std::string>& runArgs)
        {
            std::vector<std::string> args;
            args.reserve(4 + compilerArgs.size() + (runArgs.size() * 2));

            args.push_back(executablePath.string());
            args.push_back(sourcePath.string());

            if (mode == "run")
            {
                args.push_back("--run");
            }
            else if (mode == "check")
            {
                args.push_back("--dry-run");
            }
            else if (mode == "tokens")
            {
                args.push_back("--show-tokens");
                args.push_back("--dry-run");
            }
            else if (mode == "ast")
            {
                args.push_back("--show-ast");
                args.push_back("--dry-run");
            }

            args.insert(args.end(), compilerArgs.begin(), compilerArgs.end());

            if (mode == "run")
            {
                for (const auto& runArg : runArgs)
                {
                    args.push_back("--run-arg");
                    args.push_back(runArg);
                }
            }

            return args;
        }

        std::optional<std::pair<std::filesystem::path, size_t>> parseOptionalSourcePath(int argc, char* argv[], int startIndex)
        {
            if (argc <= startIndex || argv[startIndex] == nullptr)
                return std::nullopt;

            const std::string_view candidate = argv[startIndex];
            if (!candidate.empty() && candidate.front() != '-')
            {
                return std::make_pair(
                    std::filesystem::absolute(std::filesystem::path(std::string(candidate))).make_preferred(),
                    static_cast<size_t>(startIndex + 1));
            }

            return std::nullopt;
        }

        int handleFileMode(const std::string& mode, int argc, char* argv[])
        {
            std::filesystem::path sourcePath = resolveDefaultSourcePath();
            size_t nextIndex = 3;

            if (const auto parsedSource = parseOptionalSourcePath(argc, argv, 3); parsedSource.has_value())
            {
                sourcePath = parsedSource->first;
                nextIndex = parsedSource->second;
            }

            std::error_code ec;
            if (!std::filesystem::exists(sourcePath, ec) || ec)
            {
                std::cerr << "Wio source file not found: " << sourcePath.string() << '\n';
                return EXIT_FAILURE;
            }

            std::vector<std::string> compilerArgs;
            std::vector<std::string> runArgs;
            bool collectingRunArgs = false;
            for (size_t i = nextIndex; i < static_cast<size_t>(argc); ++i)
            {
                if (argv[i] != nullptr)
                {
                    const std::string currentArg = argv[i];
                    if (mode == "run" && currentArg == "--")
                    {
                        collectingRunArgs = true;
                        continue;
                    }

                    if (collectingRunArgs)
                        runArgs.push_back(currentArg);
                    else
                        compilerArgs.push_back(currentArg);
                }
            }

            const std::filesystem::path executablePath =
                (argc > 0 && argv != nullptr && argv[0] != nullptr && argv[0][0] != '\0')
                    ? std::filesystem::absolute(std::filesystem::path(argv[0])).make_preferred()
                    : std::filesystem::path("wio");

            std::vector<std::string> finalCompilerArgs =
                buildCompilerArgs(executablePath, mode, sourcePath, compilerArgs, runArgs);

            std::vector<char*> argvView;
            argvView.reserve(finalCompilerArgs.size());
            for (std::string& arg : finalCompilerArgs)
                argvView.push_back(arg.data());

            wio::Compiler::get().loadArgs(static_cast<int>(argvView.size()), argvView.data());
            return wio::Compiler::get().compile();
        }
    }

    std::optional<int> tryHandleFileCommand(int argc, char* argv[])
    {
        if (argc < 2 || argv == nullptr || argv[1] == nullptr)
            return std::nullopt;

        const std::string_view command = argv[1];
        if (command != "file")
            return std::nullopt;

        if (argc < 3 || argv[2] == nullptr)
        {
            std::cerr
                << "Expected a file subcommand. Currently supported: run, check, tokens, ast\n"
                << "Usage:\n"
                << "  wio file run    [FILE] [compiler args...] [-- program args...]\n"
                << "  wio file check  [FILE] [extra compiler args...]\n"
                << "  wio file tokens [FILE] [extra compiler args...]\n"
                << "  wio file ast    [FILE] [extra compiler args...]\n";
            return EXIT_FAILURE;
        }

        const std::string_view mode = argv[2];
        if (mode == "run" || mode == "check" || mode == "tokens" || mode == "ast")
            return handleFileMode(std::string(mode), argc, argv);

        std::cerr << "Unknown file subcommand: " << mode << '\n';
        return EXIT_FAILURE;
    }
}
