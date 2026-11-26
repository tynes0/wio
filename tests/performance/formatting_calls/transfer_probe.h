#pragma once

#include <stdexcept>
#include <string>

namespace transfer_probe
{
    inline std::string Echo(const std::string& value)
    {
        return value;
    }

    inline std::string Fail(const std::string&)
    {
        throw std::runtime_error("transfer-probe failure");
    }
} // namespace transfer_probe
