#pragma once

#include "scalar_codegen_probe.h"

#include <iostream>
#include <string>

namespace string_fast_probe
{
    inline std::uint64_t End()
    {
        const auto count = scalar_codegen_probe::End();
        std::cout << "string_fast_allocations=" << count << '\n';
        return count;
    }

    inline bool IsLargest(const std::string& value)
    {
        return value == "18446744073709551615";
    }
} // namespace string_fast_probe
