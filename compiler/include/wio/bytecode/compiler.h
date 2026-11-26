#pragma once

#include "wio/bytecode/module.h"
#include "wio/wir/lowered_ir.h"

#include <string>
#include <utility>
#include <vector>

namespace wio::bytecode
{
    struct CompileDiagnostic
    {
        std::string code;
        std::string message;
        wio::wir::SourceSpan source;
    };

    class CompileResult final
    {
    public:
        [[nodiscard]] bool succeeded() const noexcept { return diagnostics_.empty(); }
        [[nodiscard]] const Module& module() const noexcept { return module_; }
        [[nodiscard]] Module&& takeModule() noexcept { return std::move(module_); }
        [[nodiscard]] const std::vector<CompileDiagnostic>& diagnostics() const noexcept { return diagnostics_; }

    private:
        friend class Compiler;
        Module module_;
        std::vector<CompileDiagnostic> diagnostics_;
    };

    class Compiler final
    {
    public:
        [[nodiscard]] CompileResult compile(const wio::wir::lowered::Module& module) const;
    };
} // namespace wio::bytecode
