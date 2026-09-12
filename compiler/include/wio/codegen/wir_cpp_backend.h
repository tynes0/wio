#pragma once

#include "wio/wir/lowered_ir.h"
#include "wio/wir/source_span.h"

#include <string>
#include <string_view>
#include <vector>

namespace wio::codegen
{
    struct WirCppResultSink;

    enum class WirCppDiagnosticSeverity : unsigned char
    {
        Warning,
        Error
    };

    struct WirCppDiagnostic
    {
        std::string code;
        std::string message;
        wir::SourceSpan source;
        wir::FunctionId function;
        wir::BlockId block;
        WirCppDiagnosticSeverity severity = WirCppDiagnosticSeverity::Error;
    };

    struct WirCppBackendOptions
    {
        bool emitMain = true;
        bool emitLineDirectives = true;
        bool emitComments = true;
    };

    class WirCppGenerationResult final
    {
    public:
        [[nodiscard]] bool succeeded() const;
        [[nodiscard]] const std::string& code() const { return code_; }
        [[nodiscard]] const std::vector<WirCppDiagnostic>& diagnostics() const { return diagnostics_; }

    private:
        friend class WirCppBackend;
        friend struct WirCppResultSink;
        std::string code_;
        std::vector<WirCppDiagnostic> diagnostics_;
    };

    // The canonical native backend. Unlike CppGenerator, this class has no AST
    // dependency: every semantic, overload, ownership and layout decision must
    // already be frozen in Lowered WIR before generation starts.
    class WirCppBackend final
    {
    public:
        [[nodiscard]] WirCppGenerationResult generate(
            const wir::lowered::Module& module,
            const WirCppBackendOptions& options = {}) const;

        [[nodiscard]] static std::string_view backendName() { return "wir-cpp"; }
    };
}
