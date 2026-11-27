#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wio::bytecode
{
    struct Module;
}

namespace wio::app
{
    // Decimal uint64 only. Zero explicitly disables the instruction budget.
    [[nodiscard]] bool parseVmInstructionLimit(std::string_view value, std::uint64_t& limit);
    [[nodiscard]] int runVmBytecode(int argc, char* argv[]);
    [[nodiscard]] int runVmModule(const bytecode::Module& module, const std::filesystem::path& argumentPath,
                                  std::span<const std::filesystem::path> libraries, std::vector<std::string> arguments,
                                  std::uint64_t instructionLimit = 0);
} // namespace wio::app
