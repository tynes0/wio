#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/value.h"

#include <cstdint>
#include <string>

namespace wio::vm::detail
{
    class ValueFormatter final
    {
    public:
        // Formatting is bounded and never invokes arbitrary Wio/user hooks.
        // Object/user-hook interpolation needs shared callable lowering or a continuation;
        // unsupported leaves retain WVM1062 at the caller.
        [[nodiscard]] static bool append(const bytecode::Module& module, std::uint32_t type, const Value& value,
                                         std::string& target, unsigned depth = 0);
    };
} // namespace wio::vm::detail
