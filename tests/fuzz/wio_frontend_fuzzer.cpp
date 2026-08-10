#include "compiler.h"
#include "wio/codegen/cpp_generator.h"
#include "wio/common/exception.h"
#include "wio/common/logger.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
    class DiagnosticProbe
    {
    public:
        DiagnosticProbe() { wio::Logger::get().beginDiagnosticProbe(); }
        DiagnosticProbe(const DiagnosticProbe&) = delete;
        DiagnosticProbe& operator=(const DiagnosticProbe&) = delete;
        ~DiagnosticProbe()
        {
            if (active_)
                (void)wio::Logger::get().endDiagnosticProbe();
        }

        int finish()
        {
            active_ = false;
            return wio::Logger::get().endDiagnosticProbe();
        }

    private:
        bool active_ = true;
    };
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size > 256 * 1024)
        return 0;

    std::string source(reinterpret_cast<const char*>(data), size);
    std::vector<wio::Token> tokens;
    {
        DiagnosticProbe probe;
        try
        {
            wio::Lexer lexer(std::move(source), "<fuzz>");
            tokens = lexer.lex();
        }
        catch (const wio::common::Exception&)
        {
            return 0;
        }
    }

    wio::NodePtr<wio::Program> program;
    {
        DiagnosticProbe probe;
        wio::Parser parser(std::move(tokens));
        program = parser.parseProgram();
        if (probe.finish() != 0)
            return 0;
    }

    // The analyzer owns the scopes and symbols referenced weakly by the AST.
    // Keep it alive through code generation, matching the compiler pipeline's
    // lifetime contract.
    wio::sema::SemanticAnalyzer analyzer;
    {
        DiagnosticProbe probe;
        try
        {
            analyzer.analyze(program);
        }
        catch (const wio::common::Exception&)
        {
            return 0;
        }
        if (probe.finish() != 0)
            return 0;
    }

    // Successful semantic inputs must also survive C++ generation. Sanitizer
    // failures, standard-library exceptions, and empty output remain visible
    // to the fuzzing engine as real bugs.
    wio::codegen::CppGenerator generator;
    const std::string generated = generator.generate(program);
    if (generated.empty())
        __builtin_trap();

    return 0;
}
