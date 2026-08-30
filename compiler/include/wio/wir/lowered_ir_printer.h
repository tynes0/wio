#pragma once

#include "wio/wir/lowered_ir.h"

#include <string>

namespace wio::wir::lowered
{
    class Printer final
    {
    public:
        [[nodiscard]] std::string print(const Module& module) const;
    };
}
