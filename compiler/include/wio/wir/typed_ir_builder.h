#pragma once

#include "wio/ast/ast.h"
#include "wio/wir/typed_ir.h"

#include <string>
#include <utility>
#include <vector>

namespace wio::wir::typed
{
    struct BuildDiagnostic
    {
        std::string code;
        std::string message;
        SourceSpan source;
    };

    class BuildResult final
    {
    public:
        [[nodiscard]] bool succeeded() const { return diagnostics_.empty(); }
        [[nodiscard]] const Module& module() const { return module_; }
        [[nodiscard]] Module&& takeModule() { return std::move(module_); }
        [[nodiscard]] const std::vector<BuildDiagnostic>& diagnostics() const { return diagnostics_; }

    private:
        friend class Builder;
        friend class BuildContext;
        Module module_;
        std::vector<BuildDiagnostic> diagnostics_;
    };

    class Builder final
    {
    public:
        [[nodiscard]] BuildResult build(const Ref<Program>& program) const;
    };
}
