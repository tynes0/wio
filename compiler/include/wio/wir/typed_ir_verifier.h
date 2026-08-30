#pragma once

#include "wio/wir/typed_ir.h"

#include <string>
#include <vector>

namespace wio::wir::typed
{
    struct VerificationDiagnostic
    {
        std::string code;
        std::string message;
        SourceSpan source;
        FunctionId function;
        BlockId block;
    };

    class VerificationResult final
    {
    public:
        [[nodiscard]] bool succeeded() const { return diagnostics_.empty(); }
        [[nodiscard]] const std::vector<VerificationDiagnostic>& diagnostics() const { return diagnostics_; }

    private:
        friend class Verifier;
        std::vector<VerificationDiagnostic> diagnostics_;
    };

    class Verifier final
    {
    public:
        [[nodiscard]] VerificationResult verify(const Module& module) const;
    };
}
