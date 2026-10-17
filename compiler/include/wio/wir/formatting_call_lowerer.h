#pragma once

#include "wio/wir/typed_ir.h"

#include <cstddef>

namespace wio::wir
{
    // Materialize synchronous scalar object formatting as ordinary, null-safe
    // WIR method dispatch. This pass runs after generic specialization and
    // before coroutine/control-flow lowering; unsupported surfaces are retained.
    class FormattingCallLowerer final
    {
    public:
        [[nodiscard]] static std::size_t lower(typed::Module& module);
    };
} // namespace wio::wir
