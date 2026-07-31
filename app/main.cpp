#include "binding_cli.h"
#include "compiler.h"
#include "process_cli.h"

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

    std::vector<char*> argvView(std::vector<std::string>& values)
    {
        std::vector<char*> result;
        result.reserve(values.size());
        for (std::string& value : values)
            result.push_back(value.data());
        return result;
    }

    int runPrivateService(int argc, char* argv[])
    {
        if (argc < 3 || argv[2] == nullptr)
        {
            std::cerr << "Missing Wio compiler service name.\n";
            return 1;
        }

        const std::string_view service = argv[2];
        std::vector<std::string> rewritten;
        rewritten.emplace_back(argv[0] != nullptr ? argv[0] : "wio");
        rewritten.emplace_back(service);
        for (int index = 3; index < argc; ++index)
        {
            if (argv[index] != nullptr)
                rewritten.emplace_back(argv[index]);
        }
        std::vector<char*> view = argvView(rewritten);

        if (service == "bind")
        {
            const auto result = wio::tooling::binding::tryHandleBindCommand(
                static_cast<int>(view.size()), view.data());
            return result.value_or(1);
        }
        std::cerr << "Unknown Wio compiler service: " << service << '\n';
        return 1;
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

    if (argc >= 2 && argv[1] != nullptr && std::string_view(argv[1]) == "--compiler-version")
    {
        std::cout << WIO_VERSION << '\n';
        return 0;
    }

    if (argc >= 2 && argv[1] != nullptr && std::string_view(argv[1]) == "--wio-service")
        return runPrivateService(argc, argv);

    if (shouldUseSelfHostedCli(argc, argv))
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

    return runCompiler(argc, argv);
}
