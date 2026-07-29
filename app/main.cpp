#include "compiler.h"
#include "process_cli.h"
#include "tooling_cli.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

namespace
{
    int runNativeCli(int argc, char* argv[])
    {
        if (const auto toolingResult = wio::tooling::tryHandleToolingCommand(argc, argv);
            toolingResult.has_value())
        {
            return *toolingResult;
        }

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
        return false;
    }

    std::filesystem::path selfHostedCliPath(char* executableArgument)
    {
        if (executableArgument == nullptr || executableArgument[0] == '\0')
            return {};

        std::error_code ec;
        const std::filesystem::path executable =
            std::filesystem::absolute(std::filesystem::path(executableArgument), ec);
        if (ec)
            return {};

#if defined(_WIN32)
        return executable.parent_path() / "wio-selfhost.exe";
#else
        return executable.parent_path() / "wio-selfhost";
#endif
    }
}

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    if (argc >= 2 && argv[1] != nullptr && std::string_view(argv[1]) == "--native-cli")
    {
        std::vector<char*> nativeArgv;
        nativeArgv.reserve(static_cast<size_t>(argc - 1));
        nativeArgv.push_back(argv[0]);
        for (int index = 2; index < argc; ++index)
            nativeArgv.push_back(argv[index]);
        return runNativeCli(static_cast<int>(nativeArgv.size()), nativeArgv.data());
    }

    if (shouldUseSelfHostedCli(argc, argv) && std::getenv("WIO_FORCE_NATIVE_CLI") == nullptr)
    {
        const std::filesystem::path companion = selfHostedCliPath(argc > 0 ? argv[0] : nullptr);
        std::error_code ec;
        if (!companion.empty() && std::filesystem::is_regular_file(companion, ec))
        {
            std::vector<std::string> command{ companion.string() };
            for (int index = 1; index < argc; ++index)
            {
                if (argv[index] != nullptr)
                    command.emplace_back(argv[index]);
            }
            return wio::tooling::process::Run(command);
        }
    }

    return runNativeCli(argc, argv);
}
