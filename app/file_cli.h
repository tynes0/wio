#pragma once

#include <optional>

namespace wio::tooling::file
{
    std::optional<int> tryHandleFileCommand(int argc, char* argv[]);
}
