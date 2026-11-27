#include "vm_runner.h"

#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wio::app
{
    bool parseVmInstructionLimit(const std::string_view value, std::uint64_t& limit)
    {
        if (value.empty())
            return false;
        for (const char character : value)
            if (character < '0' || character > '9')
                return false;
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
            return false;
        limit = parsed;
        return true;
    }

    namespace
    {
        struct RunOptions final
        {
            std::filesystem::path bytecode;
            std::vector<std::filesystem::path> nativeLibraries;
            std::vector<std::string> arguments;
            std::uint64_t instructionLimit = 0;
        };

        bool parseOptions(const int argc, char* argv[], RunOptions& options, std::string& error)
        {
            if (argc < 3 || !argv[2] || argv[2][0] == '\0')
            {
                error = "--vm-run-bytecode requires a .wiob module path";
                return false;
            }
            options.bytecode = std::filesystem::u8path(argv[2]);
            bool applicationArguments = false;
            bool sawInstructionLimit = false;
            for (int index = 3; index < argc; ++index)
            {
                const std::string_view argument = argv[index] ? argv[index] : "";
                if (applicationArguments)
                {
                    options.arguments.emplace_back(argument);
                    continue;
                }
                if (argument == "--")
                {
                    applicationArguments = true;
                    continue;
                }
                if (argument == "--native-library")
                {
                    if (++index >= argc || !argv[index] || argv[index][0] == '\0')
                    {
                        error = "--native-library requires a dynamic library path";
                        return false;
                    }
                    options.nativeLibraries.push_back(std::filesystem::u8path(argv[index]));
                    continue;
                }
                if (argument == "--instruction-limit")
                {
                    if (sawInstructionLimit || ++index >= argc || !argv[index] ||
                        !parseVmInstructionLimit(argv[index], options.instructionLimit))
                    {
                        error = "--instruction-limit requires one unsigned decimal 64-bit value (0 = unlimited)";
                        return false;
                    }
                    sawInstructionLimit = true;
                    continue;
                }
                error = "unknown VM runner option: " + std::string(argument);
                return false;
            }
            return true;
        }

        bool readModule(const std::filesystem::path& path, bytecode::Module& module, std::string& error)
        {
            const auto encoded = path.u8string();
            const std::string displayPath(encoded.begin(), encoded.end());
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream)
            {
                error = "could not open bytecode module: " + displayPath;
                return false;
            }
            const auto size = stream.tellg();
            if (size <= 0 || static_cast<std::uint64_t>(size) > bytecode::DecodeLimits{}.maximumFileBytes)
            {
                error = "bytecode module is empty or exceeds the decoder size limit: " + displayPath;
                return false;
            }
            std::vector<char> data(static_cast<std::size_t>(size));
            stream.seekg(0);
            if (!stream.read(data.data(), static_cast<std::streamsize>(data.size())))
            {
                error = "could not read complete bytecode module: " + displayPath;
                return false;
            }
            const auto bytes = std::span{reinterpret_cast<const std::byte*>(data.data()), data.size()};
            bytecode::DecodeResult decoded = bytecode::decode(bytes);
            if (!decoded.succeeded())
            {
                error = "bytecode decode failed: " + decoded.error;
                return false;
            }
            module = std::move(decoded.module);
            return true;
        }

        std::uint32_t findEntry(const bytecode::Module& module)
        {
            if (module.contract.hasApplication && module.contract.application.entry != bytecode::InvalidIndex)
                return module.contract.application.entry;
            for (const std::string_view name : {std::string_view{"main"}, std::string_view{"Entry"}})
            {
                for (const bytecode::Function& function : module.functions)
                {
                    if (module.string(function.name) == name)
                        return function.id;
                }
            }
            return bytecode::InvalidIndex;
        }

        int exitCode(const vm::Value& value)
        {
            if (value.kind() == vm::Value::Kind::SignedInteger)
                return static_cast<int>(value.asSignedInteger());
            if (value.kind() == vm::Value::Kind::UnsignedInteger)
                return static_cast<int>(value.asUnsignedInteger());
            return EXIT_SUCCESS;
        }

        void printExecutionError(const vm::ExecutionError& error)
        {
            std::cerr << "VM Error " << error.code << ": " << error.message;
            if (error.function != bytecode::InvalidIndex)
                std::cerr << " (function " << error.function << ')';
            std::cerr << '\n';
        }
    } // namespace

    int checkVmNativeSupport(const std::filesystem::path& bytecodePath, const std::filesystem::path& standardLibrary)
    {
        bytecode::Module module;
        bytecode::Module catalog;
        std::string error;
        if (!readModule(bytecodePath, module, error))
        {
            std::cerr << "VM native preflight: " << error << '\n';
            return EXIT_FAILURE;
        }
        auto catalogPath = standardLibrary;
        catalogPath.replace_extension(".wiob");
        if (!readModule(catalogPath, catalog, error))
        {
            std::cerr << "VM standard adapter catalog: " << error << '\n';
            return EXIT_FAILURE;
        }
        // A matching C++ symbol/signature must not accidentally replace a user
        // definition from another header. The verified companion catalog also
        // identifies the native declaration's header, not just its stable key.
        std::unordered_set<std::string> nativeImports;
        const auto importKey = [](const bytecode::Module& owner, const bytecode::Import& entry)
        { return std::to_string(entry.kind) + ":" + std::string(owner.string(entry.logicalName)); };
        for (const auto& entry : catalog.contract.imports)
            if (entry.kind == 2 || entry.kind == 3)
                nativeImports.insert(importKey(catalog, entry));
        for (const auto& entry : module.contract.imports)
            if ((entry.kind == 2 || entry.kind == 3) && !nativeImports.contains(importKey(module, entry)))
            {
                // A header may have C++ global initialization even without an
                // explicit native function. Do not discard its producer.
                std::cout << "native import requires a project adapter: " << module.string(entry.logicalName) << '\n';
                return 3;
            }
        std::unordered_multimap<std::string_view, std::string_view> headers;
        const auto openTypes = module.openTypes();
        const auto catalogOpenTypes = catalog.openTypes();
        for (const auto& function : catalog.functions)
            if (function.hasNativeBinding && !function.hasOpenSignature(catalogOpenTypes))
                headers.emplace(catalog.string(function.nativeBinding.stableKey),
                                catalog.string(function.nativeBinding.header));
        for (const auto& function : module.functions)
        {
            if (!function.hasNativeBinding || function.hasOpenSignature(openTypes))
                continue;
            const auto key = module.string(function.nativeBinding.stableKey);
            const auto [begin, end] = headers.equal_range(key);
            if (std::none_of(begin, end, [&](const auto& entry)
                             { return entry.second == module.string(function.nativeBinding.header); }))
            {
                std::cout << "native declaration requires a project adapter: " << key << '\n';
                return 3;
            }
        }
        std::unordered_multimap<std::string_view, std::string_view> typeHeaders;
        for (const auto& type : catalog.types)
            if (type.hasNativeBinding)
                typeHeaders.emplace(catalog.string(type.nativeBinding.cppName),
                                    catalog.string(type.nativeBinding.header));
        for (std::size_t index = 0; index < module.types.size(); ++index)
        {
            const auto& type = module.types[index];
            if (type.kind != 28 || !type.hasNativeBinding || openTypes[index] ||
                (type.nominalKind != 1 && type.nominalKind != 2))
                continue;
            const auto [begin, end] = typeHeaders.equal_range(module.string(type.nativeBinding.cppName));
            if (std::none_of(begin, end, [&](const auto& entry)
                             { return entry.second == module.string(type.nativeBinding.header); }))
            {
                std::cout << "native type declaration requires a project adapter\n";
                return 3;
            }
        }
        vm::Machine machine{module};
        if (!machine.loadNativeLibrary(standardLibrary, &error))
        {
            std::cerr << "VM standard adapter: " << error << '\n';
            return EXIT_FAILURE;
        }
        if (!machine.nativeBindingsAvailable(&error))
        {
            std::cout << error << '\n';
            return 3;
        }
        return EXIT_SUCCESS;
    }

    int runVmBytecode(const int argc, char* argv[])
    {
        RunOptions options;
        std::string error;
        if (!parseOptions(argc, argv, options, error))
        {
            std::cerr << "VM runner: " << error << '\n';
            return EXIT_FAILURE;
        }

        bytecode::Module module;
        if (!readModule(options.bytecode, module, error))
        {
            std::cerr << "VM runner: " << error << '\n';
            return EXIT_FAILURE;
        }

        return runVmModule(module, options.bytecode, options.nativeLibraries, std::move(options.arguments),
                           options.instructionLimit);
    }

    int runVmModule(const bytecode::Module& module, const std::filesystem::path& argumentPath,
                    std::span<const std::filesystem::path> libraries, std::vector<std::string> applicationArguments,
                    const std::uint64_t instructionLimit)
    {
        std::string error;
        const std::uint32_t entry = findEntry(module);
        if (entry == bytecode::InvalidIndex || entry >= module.functions.size())
        {
            std::cerr << "VM runner: module has no runnable main, Entry, or application entry\n";
            return EXIT_FAILURE;
        }

        vm::Machine machine{module, vm::MachineOptions{.instructionLimit = instructionLimit}};
        for (const std::filesystem::path& library : libraries)
        {
            if (!machine.loadNativeLibrary(library, &error))
            {
                std::cerr << "VM runner: " << error << '\n';
                return EXIT_FAILURE;
            }
        }
        machine.bindMainExecutor();

        std::vector<vm::Value> arguments;
        const bytecode::Function& function = module.functions[entry];
        if (function.parameters.size() > 1)
        {
            std::cerr << "VM runner: entry function accepts more than one parameter\n";
            return EXIT_FAILURE;
        }
        if (function.parameters.size() == 1)
        {
            const auto typeId = function.parameters.front().type;
            if (typeId >= module.types.size() || module.types[typeId].kind != 31 ||
                module.types[typeId].arguments.size() != 1 ||
                module.types[typeId].arguments.front() >= module.types.size() ||
                module.types[module.types[typeId].arguments.front()].kind != 17)
            {
                std::cerr << "VM runner: entry parameter must be string[]\n";
                return EXIT_FAILURE;
            }
            std::vector<vm::Value> values;
            values.reserve(applicationArguments.size() + 1);
            const auto modulePath = argumentPath.u8string();
            values.push_back(vm::Value::string(std::string(modulePath.begin(), modulePath.end())));
            for (std::string& argument : applicationArguments)
                values.push_back(vm::Value::string(std::move(argument)));
            arguments.push_back(vm::Value::array(std::move(values)));
        }

        vm::ExecutionResult result = machine.invoke(entry, arguments);
        if (!result.succeeded())
        {
            printExecutionError(result.error());
            return EXIT_FAILURE;
        }
        if (result.value().kind() == vm::Value::Kind::AsyncTask)
            result = machine.wait(result.value());
        if (!result.succeeded())
        {
            printExecutionError(result.error());
            return EXIT_FAILURE;
        }
        return exitCode(result.value());
    }
} // namespace wio::app
