#pragma once

#include "wio/wir/lowered_ir.h"
#include "wio/wir/typed_ir.h"

#include <string>
#include <utility>
#include <vector>

namespace wio::wir
{
    struct LoweringDiagnostic
    {
        std::string code;
        std::string pass;
        std::string message;
        SourceSpan source;
    };

    class LoweringResult final
    {
    public:
        [[nodiscard]] bool succeeded() const { return diagnostics_.empty(); }
        [[nodiscard]] const lowered::Module& module() const { return module_; }
        [[nodiscard]] lowered::Module&& takeModule() { return std::move(module_); }
        [[nodiscard]] const std::vector<LoweringDiagnostic>& diagnostics() const { return diagnostics_; }
        [[nodiscard]] const std::vector<std::string>& completedPasses() const { return completedPasses_; }

    private:
        friend class LoweringPipeline;
        friend class CanonicalControlFlowLowerer;
        lowered::Module module_;
        std::vector<LoweringDiagnostic> diagnostics_;
        std::vector<std::string> completedPasses_;
    };

    class LoweringPipeline final
    {
    public:
        [[nodiscard]] LoweringResult lower(const typed::Module& module) const;
    };
}
