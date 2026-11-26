#pragma once

#include "wio/bytecode/module.h"

#include <string>

namespace wio::bytecode
{
    class Disassembler final
    {
    public:
        [[nodiscard]] std::string disassemble(const Module& module) const;
    };
} // namespace wio::bytecode
