#include "file_cli.h"

#include "cli_common.h"
#include "compiler.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
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
    void printFileCommandUsage(std::ostream& stream)
    {
        stream
            << "Wio file commands\n"
            << "\n"
            << "Usage:\n"
            << "  wio file run    [FILE] [compiler args...] [-- program args...]\n"
            << "  wio file check  [FILE] [extra compiler args...]\n"
            << "  wio file tokens [FILE] [extra compiler args...]\n"
            << "  wio file ast    [FILE] [extra compiler args...]\n"
            << "\n"
            << "When FILE is omitted, Wio looks for a conventional project entry file and\n"
            << "then falls back to the repository playground entry when available.\n";
    }

        bool hasProjectManifest(const std::filesystem::path& candidate)
        {
            std::error_code ec;
            return std::filesystem::exists(candidate / "wio.makewio", ec) ||
                   std::filesystem::exists(candidate / "makewio", ec) ||
                   std::filesystem::exists(candidate / "wio.project.json", ec);
        }

        std::optional<std::filesystem::path> searchAncestorDirectories(
            std::filesystem::path seed,
            const std::function<bool(const std::filesystem::path&)>& predicate)
        {
            if (seed.empty())
                return std::nullopt;

            std::error_code ec;
            seed = std::filesystem::absolute(seed, ec).make_preferred();
            if (ec)
                return std::nullopt;

            while (!seed.empty())
            {
                if (predicate(seed))
                    return seed;

                const auto parent = seed.parent_path();
                if (parent == seed)
                    break;
                seed = parent;
            }

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

        std::optional<std::filesystem::path> tryFindProjectRoot(const std::filesystem::path& sourcePath)
        {
            if (auto fromSource = searchAncestorDirectories(sourcePath.parent_path(), hasProjectManifest); fromSource.has_value())
                return fromSource;

            std::error_code ec;
            std::filesystem::path current = std::filesystem::current_path(ec);
            if (!ec && !current.empty())
            {
                if (auto fromCurrent = searchAncestorDirectories(current, hasProjectManifest); fromCurrent.has_value())
                    return fromCurrent;
            }

            return std::nullopt;
        }

        std::filesystem::path getUserCacheRoot()
        {
#if defined(_WIN32)
            if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData != nullptr && *localAppData != '\0')
                return std::filesystem::path(localAppData).make_preferred();

            if (const char* userProfile = std::getenv("USERPROFILE"); userProfile != nullptr && *userProfile != '\0')
                return (std::filesystem::path(userProfile) / "AppData" / "Local").make_preferred();
#else
            if (const char* xdgCacheHome = std::getenv("XDG_CACHE_HOME"); xdgCacheHome != nullptr && *xdgCacheHome != '\0')
                return std::filesystem::path(xdgCacheHome).make_preferred();

            if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
                return (std::filesystem::path(home) / ".cache").make_preferred();
#endif

            std::error_code ec;
            std::filesystem::path tempRoot = std::filesystem::temp_directory_path(ec);
            if (!ec && !tempRoot.empty())
                return tempRoot.make_preferred();

            return std::filesystem::absolute(std::filesystem::path(".")).make_preferred();
        }

        std::filesystem::path resolveDefaultSourcePath()
        {
            auto findConventionalEntry = [](const std::filesystem::path& root)
                -> std::optional<std::filesystem::path>
            {
                const std::filesystem::path candidates[]{
                    root / "wio" / "main.wio",
                    root / "wio" / "module.wio",
                    root / "src" / "main.wio",
                    root / "src" / "module.wio",
                    root / "main.wio"
                };

                for (const auto& candidate : candidates)
                {
                    std::error_code ec;
                    if (std::filesystem::exists(candidate, ec) &&
                        std::filesystem::is_regular_file(candidate, ec))
                    {
                        return std::filesystem::absolute(candidate).make_preferred();
                    }
                }

                return std::nullopt;
            };

            std::error_code ec;
            const std::filesystem::path current = std::filesystem::current_path(ec);
            if (!ec && !current.empty())
            {
                if (const auto projectRoot = searchAncestorDirectories(current, hasProjectManifest); projectRoot.has_value())
                {
                    if (const auto projectEntry = findConventionalEntry(*projectRoot); projectEntry.has_value())
                        return *projectEntry;
                }

                if (const auto currentEntry = findConventionalEntry(current); currentEntry.has_value())
                    return *currentEntry;
            }

            if (const auto repoRoot = tryFindRepoRoot(); repoRoot.has_value())
            {
                const std::filesystem::path playgroundEntry = *repoRoot / "playground" / "main.wio";
                if (std::filesystem::exists(playgroundEntry, ec) &&
                    std::filesystem::is_regular_file(playgroundEntry, ec))
                {
                    return std::filesystem::absolute(playgroundEntry).make_preferred();
                }
            }

            return std::filesystem::absolute(std::filesystem::path("main.wio")).make_preferred();
        }

        std::string sanitizePathStem(std::string stem)
        {
            for (char& ch : stem)
            {
                const unsigned char value = static_cast<unsigned char>(ch);
                if (!std::isalnum(value) && ch != '_' && ch != '-')
                    ch = '_';
            }

            if (stem.empty())
                return "file";

            return stem;
        }

        std::filesystem::path resolveFileRunOutputRoot(const std::filesystem::path& sourcePath)
        {
            if (const auto repoRoot = tryFindRepoRoot(); repoRoot.has_value())
                return std::filesystem::absolute(*repoRoot / ".wio-build" / "file-run").make_preferred();

            if (const auto projectRoot = tryFindProjectRoot(sourcePath); projectRoot.has_value())
                return std::filesystem::absolute(*projectRoot / ".wio-build" / "file-run").make_preferred();

            return std::filesystem::absolute(getUserCacheRoot() / "Wio" / "cache" / "file-run").make_preferred();
        }

        std::filesystem::path buildDefaultFileRunOutputPath(const std::filesystem::path& sourcePath)
        {
            const std::filesystem::path root = resolveFileRunOutputRoot(sourcePath);
            const std::string stem = sanitizePathStem(sourcePath.stem().string());
            const size_t sourceHash = std::hash<std::string>{}(sourcePath.string());

            std::ostringstream nameBuilder;
            nameBuilder << stem << "-" << std::hex << sourceHash;

            std::filesystem::path outputPath = root / nameBuilder.str();
#if defined(_WIN32)
            outputPath += ".exe";
#endif
            return outputPath.make_preferred();
        }

        bool hasExplicitOutputOverride(const std::vector<std::string>& compilerArgs)
        {
            for (size_t i = 0; i < compilerArgs.size(); ++i)
            {
                const std::string_view arg = compilerArgs[i];
                if (arg == "-o" || arg == "--output")
                    return true;

                if (arg.starts_with("--output="))
                    return true;
            }

            return false;
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

            if (mode == "run" && !hasExplicitOutputOverride(compilerArgs))
            {
                const std::filesystem::path outputRoot = resolveFileRunOutputRoot(sourcePath);
                args.push_back("--output");
                args.push_back(buildDefaultFileRunOutputPath(sourcePath).string());
                args.push_back("--intermediate-dir");
                args.push_back(outputRoot.string());
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
                    if (mode == "run" && !collectingRunArgs && currentArg == "--")
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
            printFileCommandUsage(std::cout);
            return EXIT_SUCCESS;
        }

        const std::string_view mode = argv[2];
        if (cli::IsHelpToken(mode))
        {
            printFileCommandUsage(std::cout);
            return EXIT_SUCCESS;
        }

        if (mode == "run" || mode == "check" || mode == "tokens" || mode == "ast")
        {
            if (argc >= 4 && argv[3] != nullptr)
            {
                const std::string_view firstModeArgument = argv[3];
                if (cli::IsHelpToken(firstModeArgument))
                {
                    printFileCommandUsage(std::cout);
                    return EXIT_SUCCESS;
                }
            }
            return handleFileMode(std::string(mode), argc, argv);
        }

        std::cerr << "Unknown file subcommand: " << mode << '\n';
        if (const auto suggestion = cli::SuggestCommand(mode, { "run", "check", "tokens", "ast" });
            suggestion.has_value())
        {
            std::cerr << "Did you mean 'wio file " << *suggestion << "'?\n";
        }
        std::cerr << "Run 'wio file --help' to list available file commands.\n";
        return EXIT_FAILURE;
    }
}
