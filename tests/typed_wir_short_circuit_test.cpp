#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"

#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace lowered = wio::wir::lowered;
    namespace typed = wio::wir::typed;

    bool ok = true;
    Lexer lexer(
        "fn Mark(value: bool) -> bool { return value; } "
        "fn Evaluate(left: bool) -> bool { "
        "  return (left and Mark(true)) or Mark(false); "
        "} "
        "fn Entry() -> bool { return Evaluate(false); }",
        "typed_wir_short_circuit_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    if (!build.succeeded())
    {
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    ok &= expect(build.succeeded(), "Logical expressions with side effects must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Short-circuit Typed WIR must pass verification");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("\"logical.and.rhs\"") != std::string::npos &&
            typedText.find("\"logical.and.short\"") != std::string::npos &&
            typedText.find("\"logical.and.merge\"") != std::string::npos &&
            typedText.find("\"logical.or.rhs\"") != std::string::npos &&
            typedText.find("\"logical.or.short\"") != std::string::npos &&
            typedText.find("\"logical.or.merge\"") != std::string::npos &&
            typedText.find("\"logical.result\"") != std::string::npos &&
            typedText.find("logical-and") == std::string::npos &&
            typedText.find("logical-or") == std::string::npos,
        "Logical expressions must use explicit CFG so calls remain conditionally evaluated");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Short-circuit Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("cond-jump") != std::string::npos &&
            loweredText.find("\"logical.and.rhs\"") != std::string::npos &&
            loweredText.find("\"logical.or.merge\"") != std::string::npos,
        "Lowered WIR must preserve short-circuit control-flow regions");

    return ok ? 0 : 1;
}
