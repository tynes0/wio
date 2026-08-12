#include "compiler.h"
#include "entry_args.h"
#include "std_process.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

namespace
{
    int runCompiler(int argc, char* argv[])
    {
        wio::Compiler::get().loadArgs(argc, argv);
        return wio::Compiler::get().compile();
    }

    bool shouldUseSelfHostedCli(int argc, char* argv[])
    {
        if (argc <= 1)
            return true;
        if (argv[1] == nullptr)
            return false;

        const std::string_view command = argv[1];
        if (command == "--self-hosted-info" || command == "--help" ||
            command == "-h" || command == "--version" || command == "-v")
        {
            return true;
        }

        constexpr std::string_view toolingCommands[]{
            "help", "run", "build", "test", "file", "project", "bind",
            "env", "package", "perf", "dev"
        };
        for (const std::string_view candidate : toolingCommands)
        {
            if (command == candidate)
                return true;
        }

        if (command.empty() || command.front() == '-')
            return false;

        const std::filesystem::path possibleSource{ std::string(command) };
        if (possibleSource.extension() == ".wio")
            return false;

        std::error_code ec;
        if (std::filesystem::is_regular_file(possibleSource, ec) && !ec)
            return false;

        return true;
    }

    std::filesystem::path selfHostedCliPath(const char* executableArgument)
    {
        std::filesystem::path executable =
            wio::runtime::std_process::CurrentExecutablePath();

        if (executable.empty() && executableArgument != nullptr && executableArgument[0] != '\0')
        {
            const std::string resolved =
                wio::runtime::std_process::WhichExecutable(executableArgument);
            if (!resolved.empty())
                executable = resolved;
        }

        if (executable.empty() && executableArgument != nullptr && executableArgument[0] != '\0')
        {
            std::error_code ec;
            executable = std::filesystem::absolute(executableArgument, ec);
            if (ec)
                executable.clear();
        }

        if (executable.empty())
            return {};

#if defined(_WIN32)
        return executable.parent_path() / "wio-selfhost.exe";
#else
        return executable.parent_path() / "wio-selfhost";
#endif
    }

    int runSelfHostedCli(const std::filesystem::path& companion, int argc, char* argv[])
    {
        std::vector<std::string> args;
        args.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0u);
        for (int index = 1; index < argc; ++index)
        {
            if (argv[index] != nullptr)
                args.emplace_back(argv[index]);
        }

        int exitCode = EXIT_FAILURE;
        int nativeError = 0;
        std::string message;
        wio::runtime::std_process::ProcessError error =
            wio::runtime::std_process::ProcessError::none;
        if (wio::runtime::std_process::TryRunResult(
                companion.string(), args, {}, exitCode, error, nativeError, message))
        {
            return exitCode;
        }

        std::cerr << "Failed to launch the self-hosted CLI: " << message;
        if (nativeError != 0)
            std::cerr << " (native error " << nativeError << ')';
        std::cerr << '\n';
        return EXIT_FAILURE;
    }
}

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    std::vector<std::string> utf8Arguments =
        wio::runtime::CollectEntryArguments(argc, argv);
    std::vector<char*> utf8ArgumentPointers;
    utf8ArgumentPointers.reserve(utf8Arguments.size());
    for (std::string& argument : utf8Arguments)
        utf8ArgumentPointers.push_back(argument.data());
    argc = static_cast<int>(utf8ArgumentPointers.size());
    argv = utf8ArgumentPointers.data();

    if (argc >= 2 && argv[1] != nullptr && std::string_view(argv[1]) == "--compiler-version")
    {
        std::cout << WIO_VERSION << '\n';
        return 0;
    }

    if (shouldUseSelfHostedCli(argc, argv))
    {
        const std::filesystem::path companion = selfHostedCliPath(argc > 0 ? argv[0] : nullptr);
        std::error_code ec;
        if (!companion.empty() && std::filesystem::is_regular_file(companion, ec))
            return runSelfHostedCli(companion, argc, argv);

        std::cerr << "Wio CLI companion was not found";
        if (!companion.empty())
            std::cerr << " at '" << companion.string() << '\'';
        std::cerr << ". Reinstall Wio or keep wio and wio-selfhost in the same bin directory.\n";
        return EXIT_FAILURE;
    }

    return runCompiler(argc, argv);
}
