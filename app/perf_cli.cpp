#include "perf_cli.h"

#include <argonaut.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace wio::tooling::perf
{
    namespace
    {
        struct PerfStats
        {
            std::vector<double> measurements;

            void add_measurement(const double value)
            {
                measurements.push_back(value);
            }

            [[nodiscard]] double calculate_average() const
            {
                if (measurements.empty())
                    return 0.0;

                double sum = 0.0;
                for (const double value : measurements)
                    sum += value;

                return sum / static_cast<double>(measurements.size());
            }

            [[nodiscard]] double calculate_median() const
            {
                if (measurements.empty())
                    return 0.0;

                std::vector<double> sorted = measurements;
                std::sort(sorted.begin(), sorted.end());

                const size_t middle = sorted.size() / 2;
                if ((sorted.size() % 2) == 0)
                    return (sorted[middle - 1] + sorted[middle]) / 2.0;

                return sorted[middle];
            }

            [[nodiscard]] double get_min_value() const
            {
                if (measurements.empty())
                    return 0.0;

                return *std::min_element(measurements.begin(), measurements.end());
            }

            [[nodiscard]] double get_max_value() const
            {
                if (measurements.empty())
                    return 0.0;

                return *std::max_element(measurements.begin(), measurements.end());
            }
        };

        struct ScenarioStats
        {
            std::string name;
            PerfStats stats;
        };

        bool isHelpToken(const std::string_view value)
        {
            return value == "--help" || value == "-h" || value == "help";
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
                std::cerr << "Perf CLI setup failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const Argonaut::ParseException& e)
            {
                std::cerr << "Perf argument parsing failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Unhandled perf CLI error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        bool getFlagValue(Argonaut::Parser& parser, const std::string& id)
        {
            auto values = parser.GetValuesOf<bool>(id);
            return !values.empty() && values.front();
        }

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
            std::string mergedPath;
            std::optional<size_t> pathEntryIndex;

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

                if (normalizedKey == "path")
                {
                    if (!pathEntryIndex.has_value())
                        pathEntryIndex = sanitizedEntries.size();
                    if (!mergedPath.empty() && mergedPath.back() != ';' &&
                        equalsIndex + 1 < entry.size() && entry[equalsIndex + 1] != ';')
                        mergedPath.push_back(';');
                    mergedPath.append(entry.substr(equalsIndex + 1));
                    continue;
                }

                if (!seenKeys.insert(normalizedKey).second)
                    continue;

                sanitizedEntries.emplace_back(entry);
            }

            if (!mergedPath.empty() && pathEntryIndex.has_value())
                sanitizedEntries.insert(sanitizedEntries.begin() + static_cast<std::ptrdiff_t>(*pathEntryIndex), "Path=" + mergedPath);

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

        std::filesystem::path getExecutablePath()
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

        std::optional<std::filesystem::path> tryFindRepoRoot()
        {
            std::error_code ec;
            std::filesystem::path current = std::filesystem::current_path(ec);
            if (ec)
                current.clear();

            std::vector<std::filesystem::path> seeds;
            if (!current.empty())
                seeds.push_back(current);

            if (const std::filesystem::path executablePath = getExecutablePath(); !executablePath.empty())
                seeds.push_back(executablePath.parent_path());

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

        std::filesystem::path resolveDefaultScratchDir()
        {
            if (const auto repoRoot = tryFindRepoRoot(); repoRoot.has_value())
                return std::filesystem::absolute(*repoRoot / ".wio-build" / "perf-smoke").make_preferred();

            return std::filesystem::absolute(getUserCacheRoot() / "Wio" / "cache" / "perf-smoke").make_preferred();
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

        Argonaut::Parser makePerfSmokeParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("ITERATIONS")
                        .AddAlias("--iterations")
                        .SetDefaultValue("1")
                        .SetDescription("How many times each scenario should be measured.")
                )
                .Add(
                    Argonaut::Argument("SCRATCH-DIR")
                        .AddAlias("--scratch-dir")
                        .SetDefaultValue("")
                        .SetDescription("Optional scratch directory to use for generated perf inputs and sample projects.")
                )
                .Add(
                    Argonaut::Argument("KEEP-SCRATCH")
                        .AddAlias("--keep-scratch")
                        .Flag()
                        .SetDescription("Keep the perf scratch directory on disk after a successful run.")
                )
                .AutoHelp()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        int recordScenario(ScenarioStats& scenario,
                           const int iterations,
                           const std::function<int(int)>& action)
        {
            for (int iteration = 0; iteration < iterations; ++iteration)
            {
                const auto startTime = std::chrono::steady_clock::now();
                const int result = action(iteration);
                const auto endTime = std::chrono::steady_clock::now();

                if (result != EXIT_SUCCESS)
                    return result;

                const auto elapsed = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                scenario.stats.add_measurement(elapsed);
            }

            return EXIT_SUCCESS;
        }

        void printScenarioSummary(const ScenarioStats& scenario)
        {
            std::cout << "Scenario: " << scenario.name << '\n';
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "  average_ms: " << scenario.stats.calculate_average() << '\n';
            std::cout << "  median_ms: " << scenario.stats.calculate_median() << '\n';
            std::cout << "  min_ms: " << scenario.stats.get_min_value() << '\n';
            std::cout << "  max_ms: " << scenario.stats.get_max_value() << '\n';
            std::cout.unsetf(std::ios::floatfield);
        }

        int handlePerfSmokeCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makePerfSmokeParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            const int iterations = parser.GetValuesOf<int>("ITERATIONS").front();
            if (iterations <= 0)
            {
                std::cerr << "Perf iterations must be at least 1.\n";
                return EXIT_FAILURE;
            }

            const std::string scratchValue = parser.GetValuesOf<std::string>("SCRATCH-DIR").front();
            const bool keepScratch = getFlagValue(parser, "KEEP-SCRATCH");

            const std::filesystem::path scratchDir =
                scratchValue.empty() ? resolveDefaultScratchDir() : std::filesystem::absolute(std::filesystem::path(scratchValue)).make_preferred();

            std::error_code ec;
            std::filesystem::remove_all(scratchDir, ec);
            ec.clear();
            std::filesystem::create_directories(scratchDir, ec);
            if (ec)
            {
                std::cerr << "Could not create perf scratch directory: " << scratchDir << '\n';
                return EXIT_FAILURE;
            }

            const std::filesystem::path executablePath = getExecutablePath();
            if (executablePath.empty())
            {
                std::cerr << "Could not resolve the current wio executable path for perf smoke.\n";
                return EXIT_FAILURE;
            }

            const std::filesystem::path sourcesDir = scratchDir / "sources";
            const std::filesystem::path projectScratchDir = scratchDir / "projects";
            std::filesystem::create_directories(sourcesDir, ec);
            std::filesystem::create_directories(projectScratchDir, ec);

            const std::filesystem::path fileCheckSource = sourcesDir / "perf_check.wio";
            const std::filesystem::path fileRunSource = sourcesDir / "perf_run.wio";

            writeUtf8File(
                fileCheckSource,
                "fn Entry() -> i32 {\n"
                "    return 0;\n"
                "}\n"
            );

            writeUtf8File(
                fileRunSource,
                "fn Entry() -> i32 {\n"
                "    return 0;\n"
                "}\n"
            );

            ScenarioStats fileCheckStats{ "file-check" };
            ScenarioStats fileRunStats{ "file-run" };
            ScenarioStats projectBuildColdStats{ "project-build-cold" };
            ScenarioStats projectBuildWarmStats{ "project-build-warm" };
            ScenarioStats projectRunWarmStats{ "project-run-warm" };

            const auto runWio = [&](const std::vector<std::string>& subcommand, const std::optional<std::filesystem::path>& workingDirectory = std::nullopt) -> int
            {
                std::vector<std::string> parts;
                parts.reserve(subcommand.size() + 1);
                parts.push_back(executablePath.string());
                parts.insert(parts.end(), subcommand.begin(), subcommand.end());
                return runShellCommand(parts, workingDirectory);
            };

            if (const int result = recordScenario(fileCheckStats, iterations, [&](int) -> int
            {
                return runWio({ "file", "check", fileCheckSource.string() }, scratchDir);
            }); result != EXIT_SUCCESS)
            {
                std::cerr << "Perf smoke failed during file-check.\n";
                std::cerr << "Scratch directory: " << scratchDir << '\n';
                return result;
            }

            if (const int result = recordScenario(fileRunStats, iterations, [&](int) -> int
            {
                return runWio({ "file", "run", fileRunSource.string() }, scratchDir);
            }); result != EXIT_SUCCESS)
            {
                std::cerr << "Perf smoke failed during file-run.\n";
                std::cerr << "Scratch directory: " << scratchDir << '\n';
                return result;
            }

            for (int iteration = 0; iteration < iterations; ++iteration)
            {
                const std::string iterationName = "PerfSmokeApp" + std::to_string(iteration + 1);
                const std::filesystem::path iterationRoot = projectScratchDir / iterationName;
                const std::filesystem::path projectRoot = iterationRoot / iterationName;

                std::filesystem::remove_all(iterationRoot, ec);
                ec.clear();
                std::filesystem::create_directories(iterationRoot, ec);

                const int newResult = runWio(
                    { "project", "new", iterationName, "--output-dir", iterationRoot.string(), "--template", "wio-app" },
                    scratchDir
                );

                if (newResult != EXIT_SUCCESS)
                {
                    std::cerr << "Perf smoke failed while creating project scenario " << iterationName << ".\n";
                    std::cerr << "Scratch directory: " << scratchDir << '\n';
                    return newResult;
                }

                const auto coldBuildStart = std::chrono::steady_clock::now();
                const int coldBuildResult = runWio({ "project", "build", "--project", projectRoot.string() }, scratchDir);
                const auto coldBuildEnd = std::chrono::steady_clock::now();
                if (coldBuildResult != EXIT_SUCCESS)
                {
                    std::cerr << "Perf smoke failed during cold project build.\n";
                    std::cerr << "Scratch directory: " << scratchDir << '\n';
                    return coldBuildResult;
                }
                projectBuildColdStats.stats.add_measurement(
                    std::chrono::duration<double, std::milli>(coldBuildEnd - coldBuildStart).count());

                const auto warmBuildStart = std::chrono::steady_clock::now();
                const int warmBuildResult = runWio({ "project", "build", "--project", projectRoot.string() }, scratchDir);
                const auto warmBuildEnd = std::chrono::steady_clock::now();
                if (warmBuildResult != EXIT_SUCCESS)
                {
                    std::cerr << "Perf smoke failed during warm project build.\n";
                    std::cerr << "Scratch directory: " << scratchDir << '\n';
                    return warmBuildResult;
                }
                projectBuildWarmStats.stats.add_measurement(
                    std::chrono::duration<double, std::milli>(warmBuildEnd - warmBuildStart).count());

                const auto runStart = std::chrono::steady_clock::now();
                const int runResult = runWio({ "project", "run", "--project", projectRoot.string() }, scratchDir);
                const auto runEnd = std::chrono::steady_clock::now();
                if (runResult != EXIT_SUCCESS)
                {
                    std::cerr << "Perf smoke failed during project run.\n";
                    std::cerr << "Scratch directory: " << scratchDir << '\n';
                    return runResult;
                }
                projectRunWarmStats.stats.add_measurement(
                    std::chrono::duration<double, std::milli>(runEnd - runStart).count());
            }

            std::cout << "Wio performance smoke\n";
            std::cout << "Iterations: " << iterations << '\n';
            std::cout << "Scratch directory: " << scratchDir.string() << '\n';
            printScenarioSummary(fileCheckStats);
            printScenarioSummary(fileRunStats);
            printScenarioSummary(projectBuildColdStats);
            printScenarioSummary(projectBuildWarmStats);
            printScenarioSummary(projectRunWarmStats);

            if (!keepScratch)
            {
                std::filesystem::remove_all(scratchDir, ec);
            }

            return EXIT_SUCCESS;
        }
    }

    std::optional<int> tryHandlePerfCommand(int argc, char* argv[])
    {
        if (argc < 2 || argv == nullptr || argv[1] == nullptr)
            return std::nullopt;

        if (std::string_view(argv[1]) != "perf")
            return std::nullopt;

        if (argc < 3 || argv[2] == nullptr)
        {
            std::cerr << "Expected a perf subcommand. Currently supported: smoke\n";
            return EXIT_FAILURE;
        }

        const std::string_view subcommand = argv[2];
        if (isHelpToken(subcommand))
        {
            std::cout
                << "Wio perf commands\n"
                << "\n"
                << "Usage:\n"
                << "  wio perf smoke [--iterations N] [--scratch-dir DIR] [--keep-scratch]\n";
            return EXIT_SUCCESS;
        }

        if (subcommand == "smoke")
            return handlePerfSmokeCommand(collectCommandArgs("wio perf smoke", argc, argv, 3));

        std::cerr << "Unknown perf subcommand: " << subcommand << '\n';
        return EXIT_FAILURE;
    }
}
