#include "entry_args.h"
#include "std_process.h"

#include <filesystem>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

#ifndef WIO_PLAYGROUND_COMPILER_PATH
#error "WIO_PLAYGROUND_COMPILER_PATH must identify the configured Wio executable."
#endif

namespace
{
    struct Options
    {
        std::string backend = "native";
        std::string config = std::string(WIO_PLAYGROUND_BUILD_CONFIG) == "Debug" ? "Debug" : "Release";
        std::string project = WIO_PLAYGROUND_MANIFEST_PATH;
        bool help = false;
        bool dryRun = false;
        bool rebuild = false;
        std::vector<std::string> arguments;
    };

    std::string utf8(const std::filesystem::path& path)
    {
        const auto bytes = path.u8string();
        return {bytes.begin(), bytes.end()};
    }

    Options parse(const std::vector<std::string>& args)
    {
        Options options;
        bool backendSet = false;
        bool configSet = false;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            const auto& arg = args[index];
            const auto value = [&]() -> const std::string&
            {
                if (++index >= args.size() || args[index].empty())
                    throw std::runtime_error(arg + " requires a value");
                return args[index];
            };
            if (arg == "--")
            {
                options.arguments.assign(args.begin() + static_cast<std::ptrdiff_t>(index + 1), args.end());
                break;
            }
            if (arg == "--help" || arg == "-h")
                options.help = true;
            else if (arg == "--dry-run")
                options.dryRun = true;
            else if (arg == "--rebuild")
                options.rebuild = true;
            else if (arg == "--project")
                options.project = value();
            else if (arg == "--native" || arg == "--cpp" || arg == "--vm" || arg == "--backend")
            {
                std::string backend = arg == "--backend" ? value() : (arg == "--vm" ? "vm" : "native");
                if (backend == "cpp")
                    backend = "native";
                if (backend != "native" && backend != "vm")
                    throw std::runtime_error("backend must be native/cpp or vm");
                if (backendSet && options.backend != backend)
                    throw std::runtime_error("choose only one backend");
                options.backend = std::move(backend);
                backendSet = true;
            }
            else if (arg == "--debug" || arg == "--release" || arg == "--config")
            {
                std::string config = arg == "--config" ? value() : (arg == "--debug" ? "Debug" : "Release");
                if (config == "debug")
                    config = "Debug";
                if (config == "release")
                    config = "Release";
                if (config != "Debug" && config != "Release")
                    throw std::runtime_error("profile must be Debug or Release");
                if (configSet && options.config != config)
                    throw std::runtime_error("choose only one profile");
                options.config = std::move(config);
                configSet = true;
            }
            else
                throw std::runtime_error("unknown launcher option: " + arg + "; use -- before application arguments");
        }
        return options;
    }

    void command(const std::string& executable, const std::vector<std::string>& args)
    {
        std::cout << std::quoted(executable);
        for (const auto& arg : args)
            std::cout << ' ' << std::quoted(arg);
        std::cout << '\n';
    }

    int run(const std::string& executable, const std::vector<std::string>& args)
    {
        int code = 1;
        int nativeError = 0;
        std::string message;
        auto error = wio::runtime::std_process::ProcessError::none;
        if (!wio::runtime::std_process::TryRunResult(executable, args, {}, code, error, nativeError, message))
            throw std::runtime_error("cannot launch " + executable + ": " + message);
        return code;
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const auto options = parse(wio::runtime::CollectEntryArguments(argc, argv));
        if (options.help)
        {
            std::cout << "Wio playground launcher\n"
                         "  --native | --cpp | --vm       Backend (default: native C++)\n"
                         "  --debug | --release           Application/VM runtime profile\n"
                         "  --backend native|vm --config Debug|Release are also accepted.\n"
                         "  --project PATH                Optional project/manifest override\n"
                         "  --rebuild                     Force Wio recompilation\n"
                         "  --dry-run                     Show commands without building/running\n"
                         "  -- [args...]                  Pass arguments to the Wio application\n"
                         "Default profile follows the launcher's CMake configuration.\n";
            return 0;
        }

        const bool release = options.config == "Release";
        std::vector<std::string> args{
            "project",     "run",
            "--project",   options.project,
            "--backend",   options.backend,
            "--config",    options.config,
            "--build-dir", ".wio-build/playground/" + options.backend + (release ? "-release" : "-debug")};
        for (const auto& flag :
             release ? std::vector<std::string>{"-O2", "-DNDEBUG"} : std::vector<std::string>{"-O0", "-g", "-UNDEBUG"})
        {
            args.push_back("--backend-arg");
            args.push_back(flag);
        }
        if (options.rebuild)
            args.push_back("--rebuild");

        const std::vector<std::string> runtimeBuild{"--build",  WIO_PLAYGROUND_BUILD_ROOT, "--config",   options.config,
                                                    "--target", "wio_vm_runner",           "--parallel", "1"};
        if (options.backend == "vm")
        {
            if (!WIO_PLAYGROUND_MULTI_CONFIG && options.config != WIO_PLAYGROUND_BUILD_CONFIG)
                throw std::runtime_error("VM profile differs from this single-config CMake build; configure a "
                                         "matching CMAKE_BUILD_TYPE first");
            args.push_back("--vm-runner");
            args.push_back(utf8(std::filesystem::u8path(WIO_PLAYGROUND_BUILD_ROOT) / "app" / options.config /
                                ("wio-vm" + wio::runtime::std_process::ExecutableSuffix())));
        }
        if (!options.arguments.empty())
        {
            args.push_back("--");
            args.insert(args.end(), options.arguments.begin(), options.arguments.end());
        }

        std::cout << "Playground: backend=" << options.backend << ", profile=" << options.config
                  << (release ? " (-O2 -DNDEBUG)" : " (-O0 -g -UNDEBUG)") << '\n'
                  << "Compiler host: " << WIO_PLAYGROUND_BUILD_CONFIG << " (independent of application profile)\n";
        if (options.dryRun)
        {
            if (options.backend == "vm")
                command(WIO_PLAYGROUND_CMAKE_PATH, runtimeBuild);
            command(WIO_PLAYGROUND_COMPILER_PATH, args);
            return 0;
        }
        if (!wio::runtime::std_environment::Set("WIO_ROOT", WIO_PLAYGROUND_SOURCE_ROOT))
            throw std::runtime_error("unable to configure WIO_ROOT");
        std::cout << std::flush;
        if (options.backend == "vm")
        {
            const int built = run(WIO_PLAYGROUND_CMAKE_PATH, runtimeBuild);
            if (built != 0)
                return built;
        }
        return run(WIO_PLAYGROUND_COMPILER_PATH, args);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Playground: " << error.what() << '\n';
        return 2;
    }
}
