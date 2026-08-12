#pragma once

#include <string>
#include <vector>

namespace wio::runtime
{
    [[nodiscard]] std::vector<std::string> CollectEntryArguments(
        int argc, char* const argv[]);
}
