#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wio::tooling::process
{
    std::string FormatCommand(const std::vector<std::string>& parts);

    int Run(const std::vector<std::string>& parts,
            const std::optional<std::filesystem::path>& workingDirectory = std::nullopt,
            const std::vector<std::filesystem::path>& extraPathEntries = {});
}
