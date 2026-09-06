#pragma once

#include "wio/wir/typed_ir.h"
#include <string>
#include <vector>

namespace wio::wir
{
    struct SpecializationDiagnostic
    {
        std::string code;
        std::string message;
        SourceSpan source;
    };

    // Materializes concrete bodies before ownership-sensitive canonical lowering.
    // Only pinned WIR types and call identities are consumed; no AST re-analysis.
    class GenericSpecializer final
    {
    public:
        explicit GenericSpecializer(std::size_t maximumBodies = 1024) : maximumBodies_(maximumBodies) {}
        [[nodiscard]] std::vector<SpecializationDiagnostic> specialize(typed::Module& module) const;
    private:
        std::size_t maximumBodies_;
    };
}
