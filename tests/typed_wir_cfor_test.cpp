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
        "fn Sum() -> i32 { "
        "  mut sum: i32 = 0; "
        "  for (mut index: i32 = 0; index < 8; index += 1) { "
        "    if (index == 2) { continue; } "
        "    sum += index; "
        "    if (sum > 10) { break; } "
        "  } "
        "  return sum; "
        "} "
        "fn Entry() -> i32 { return Sum(); }",
        "typed_wir_cfor_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    ok &= expect(build.succeeded(), "C-style for must build into Typed WIR");
    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "C-style for Typed WIR must pass verification");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("\"for.header\"") != std::string::npos &&
            typedText.find("\"for.body\"") != std::string::npos &&
            typedText.find("\"for.increment\"") != std::string::npos &&
            typedText.find("\"for.condition-exit\"") != std::string::npos &&
            typedText.find("\"for.exit\"") != std::string::npos &&
            typedText.find("\"index.next\"") != std::string::npos,
        "Typed WIR must expose the canonical C-style for regions and increment parameters");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "C-style for Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("\"for.increment\"") != std::string::npos &&
            loweredText.find("cond-jump") != std::string::npos,
        "Lowered WIR must preserve C-style for increment and conditional edges");

    return ok ? 0 : 1;
}
