#pragma once

#include "wio/wir/typed_ir.h"

#include <cstddef>

namespace wio::wir
{
    // Materialize synchronous formatting hooks, including container traversal,
    // as ordinary WIR calls/CFG. Native POD display uses a registered native
    // boundary, not a VM callback. Runs after specialization and before coroutine
    // lowering, so synchronous hooks also work inside async functions.
    class FormattingCallLowerer final
    {
    public:
        [[nodiscard]] static std::size_t lower(typed::Module& module);
    };
} // namespace wio::wir
