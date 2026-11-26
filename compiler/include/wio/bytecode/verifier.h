#pragma once

#include "wio/bytecode/module.h"

#include <string>
#include <vector>

namespace wio::bytecode
{
    struct VerificationDiagnostic
    {
        std::string code;
        std::string message;
        std::uint32_t function = InvalidIndex;
        std::uint32_t block = InvalidIndex;
        std::uint32_t instruction = InvalidIndex;
    };

    class VerificationResult final
    {
    public:
        [[nodiscard]] bool succeeded() const noexcept { return diagnostics_.empty(); }
        [[nodiscard]] const std::vector<VerificationDiagnostic>& diagnostics() const noexcept
        {
            return diagnostics_;
        }

    private:
        friend class Verifier;
        std::vector<VerificationDiagnostic> diagnostics_;
    };

    class Verifier final
    {
    public:
        [[nodiscard]] VerificationResult verify(const Module& module) const;
    };
} // namespace wio::bytecode
