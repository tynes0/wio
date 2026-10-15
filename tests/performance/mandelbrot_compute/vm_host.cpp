#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

// A diagnostic host, not a change to the production runner's safety defaults.
// This checksum-qualified workload executes more than the default 10M budget.
int main(int argc, char** argv)
{
    try
    {
        if (argc != 3)
            throw std::runtime_error("usage: vm-benchmark MODULE NATIVE_BRIDGE");
        std::ifstream input(argv[1], std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open benchmark bytecode");
        const std::vector<char> bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes}));
        if (!decoded.succeeded())
            throw std::runtime_error("cannot decode benchmark bytecode");
        auto& module = decoded.module;
        std::uint32_t entry = wio::bytecode::InvalidIndex;
        for (std::uint32_t index = 0; index < module.functions.size(); ++index)
        {
            if (module.string(module.functions[index].name) == "Entry")
            {
                if (entry != wio::bytecode::InvalidIndex)
                    throw std::runtime_error("ambiguous benchmark entry");
                entry = index;
            }
        }

        if (entry == wio::bytecode::InvalidIndex)
            throw std::runtime_error("benchmark has no Entry");
        wio::vm::MachineOptions options;
        options.instructionLimit = 10'000'000'000ULL;
        wio::vm::Machine machine(module, options);
        std::string error;
        if (!machine.loadNativeLibrary(argv[2], &error))
            throw std::runtime_error(error);
        machine.bindMainExecutor();
        const auto result = machine.invoke(entry);
        if (!result.succeeded())
            throw std::runtime_error(result.error().code + ": " + result.error().message);
        return static_cast<int>(result.value().asSignedInteger());
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
