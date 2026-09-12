#pragma once

#include "wio/wir/typed_ir.h"

#include <string>

namespace wio::wir::typed
{
    class Printer final
    {
    public:
        [[nodiscard]] std::string print(const Module& module) const;
    };
}
