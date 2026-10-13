#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace wio::bytecode
{
    struct Module;
}

namespace wio::app
{
    [[nodiscard]] int runVmBytecode(int argc, char* argv[]);
    [[nodiscard]] int runVmModule(const bytecode::Module& module, const std::filesystem::path& argumentPath,
                                  std::span<const std::filesystem::path> libraries, std::vector<std::string> arguments);
} // namespace wio::app
