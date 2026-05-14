#pragma once

#include <optional>

namespace wio::tooling
{
    std::optional<int> tryHandleToolingCommand(int argc, char* argv[]);
}
