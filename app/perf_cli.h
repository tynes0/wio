#pragma once

#include <optional>

namespace wio::tooling::perf
{
    std::optional<int> tryHandlePerfCommand(int argc, char* argv[]);
}
