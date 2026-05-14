#pragma once

#include <optional>

namespace wio::tooling::env
{
    std::optional<int> tryHandleEnvCommand(int argc, char* argv[]);
}
