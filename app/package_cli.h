#pragma once

#include <optional>

namespace wio::tooling::package
{
    std::optional<int> tryHandlePackageCommand(int argc, char* argv[]);
}
