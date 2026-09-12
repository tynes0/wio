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
#include <string_view>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bool hasCode(
        const wio::wir::typed::VerificationResult& result,
        const std::string_view code)
    {
        for (const auto& diagnostic : result.diagnostics())
        {
            if (diagnostic.code == code)
                return true;
        }
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
        "fn Choose(condition: bool) -> i32 { "
        "  mut value: i32; "
        "  if (condition) { value = 21; } else { value += 41; } "
        "  return value; "
        "} "
        "fn Entry() -> i32 { return Choose(true); }",
        "typed_wir_control_flow_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    ok &= expect(build.succeeded(), "Local assignment and if/else must build into Typed WIR");
    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Builder-produced local control flow must pass Typed WIR verification");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("%v1: !t2 = const 0") != std::string::npos &&
            typedText.find("cond-branch %v0, ^b1, ^b2") != std::string::npos &&
            typedText.find("branch ^b3(%v2)") != std::string::npos &&
            typedText.find("branch ^b3(%v4)") != std::string::npos &&
            typedText.find("^b3 \"if.merge\"(%v5 \"value\": !t2)") != std::string::npos &&
            typedText.find("return %v5") != std::string::npos,
        "Typed WIR must express mutable local merges as deterministic SSA block parameters");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Local control-flow Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("cond-jump %v0, ^b1, ^b2") != std::string::npos &&
            loweredText.find("jump ^b3(%v2)") != std::string::npos &&
            loweredText.find("jump ^b3(%v4)") != std::string::npos,
        "Lowered WIR must preserve branch arguments across canonical control flow");

    typed::Module malformed = build.module();
    malformed.functions.front().blocks[1].instructions.back().operands.clear();
    const typed::VerificationResult malformedResult = typed::Verifier{}.verify(malformed);
    ok &= expect(
        hasCode(malformedResult, "WIR1417"),
        "Typed WIR verifier must reject a branch missing merge arguments");

    return ok ? 0 : 1;
}
