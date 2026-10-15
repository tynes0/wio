#pragma once

#include "scalar_codegen_probe.h"
#include <string>

namespace managed_codegen_probe
{
    inline void Mutate(std::string& value)
    {
        value.append("!");
    }
} // namespace managed_codegen_probe
