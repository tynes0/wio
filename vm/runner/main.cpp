#include "artifact.h"
#include "entry_args.h"
#include "std_process.h"
#include "vm_runner.h"
#include "wio/bytecode/codec.h"

#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        auto args = wio::runtime::CollectEntryArguments(argc, argv);
        const auto self = std::filesystem::u8path(wio::runtime::std_process::CurrentExecutablePath());
        wio::vm::artifact::Payload payload;
        if (wio::vm::artifact::read(self, payload))
        {
            auto decoded = wio::bytecode::decode(payload.bytecode);
            std::vector<std::filesystem::path> libraries;
            for (const auto& name : payload.libraries)
                libraries.push_back(self.parent_path() / std::filesystem::u8path(name));
            return wio::app::runVmModule(decoded.module, self, libraries, {args.begin() + 1, args.end()});
        }
        if (args.size() > 1 && args[1] == "--package")
        {
            if (args.size() < 5)
                throw std::runtime_error(
                    "usage: wio-vm --package MODULE --output EXECUTABLE [--native-library LIBRARY ...]");
            std::filesystem::path output;
            std::vector<std::filesystem::path> libraries;
            for (std::size_t index = 3; index < args.size(); ++index)
            {
                const auto option = args[index];
                if (++index >= args.size())
                    throw std::runtime_error("VM package option requires a value");
                if (option == "--output" && output.empty())
                    output = std::filesystem::u8path(args[index]);
                else if (option == "--native-library")
                    libraries.push_back(std::filesystem::u8path(args[index]));
                else
                    throw std::runtime_error("unknown or repeated VM package option: " + option);
            }
            if (output.empty())
                throw std::runtime_error("VM package requires --output");
            wio::vm::artifact::package(self, std::filesystem::u8path(args[2]), libraries, output);
            const auto encoded = output.u8string();
            std::cout << "Generated VM executable: " << std::string(encoded.begin(), encoded.end()) << '\n';
            return 0;
        }
        if (args.size() < 2 || args[1] == "--help" || args[1] == "-h")
        {
            std::cout << "Usage: wio-vm MODULE.wiob [--native-library LIBRARY ...] [-- application args...]\n"
                         "       wio-vm --package MODULE.wiob --output EXECUTABLE [--native-library LIBRARY ...]\n";
            return 0;
        }
        args.insert(args.begin() + 1, "--vm-run-bytecode");
        std::vector<char*> pointers;
        for (auto& argument : args)
            pointers.push_back(argument.data());
        return wio::app::runVmBytecode(static_cast<int>(pointers.size()), pointers.data());
    }
    catch (const std::exception& error)
    {
        std::cerr << "VM artifact: " << error.what() << '\n';
        return 1;
    }
}
