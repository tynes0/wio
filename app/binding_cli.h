#pragma once

#include <optional>

namespace wio::tooling::binding
{
    std::optional<int> tryHandleBindCommand(int argc, char* argv[]);
}
