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
        "fn Clamp() -> i8 { return 500 fit i8; } "
        "fn ToFloat(value: i32) -> f64 { return value fit f64; } "
        "fn AcceptWide(value: i64) -> i64 { return value; } "
        "fn Widen(value: i8) -> i64 { "
        "  let local: i64 = value; "
        "  return AcceptWide(value); "
        "} "
        "fn Entry() -> i32 { return 3.9 fit i32; }",
        "typed_wir_conversion_test.wio");
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
    ok &= expect(build.succeeded(), "Numeric fit expressions must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Numeric fit Typed WIR must pass verification");

    std::size_t conversionCount = 0;
    std::size_t wideningCount = 0;
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == typed::Opcode::Convert)
                {
                    ++conversionCount;
                    if (instruction.conversionKind == typed::ConversionKind::NumericWiden)
                        ++wideningCount;
                }
            }
        }
    }
    ok &= expect(conversionCount == 5, "Explicit fit and implicit widening sites must produce conversion instructions");
    ok &= expect(wideningCount == 2, "Safe initializer and argument widening must remain explicit in WIR");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("numeric-fit") != std::string::npos &&
            typedText.find("numeric-widen") != std::string::npos,
        "Typed WIR printer must distinguish clamping fits from safe implicit widening");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Numeric fit Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("numeric-fit") != std::string::npos,
        "Lowered WIR must preserve numeric fit conversion semantics");

    typed::Module malformed = build.module();
    bool corrupted = false;
    for (typed::BasicBlock& block : malformed.functions.front().blocks)
    {
        for (typed::Instruction& instruction : block.instructions)
        {
            if (instruction.opcode == typed::Opcode::Convert)
            {
                instruction.resultType = malformed.types.stringType();
                corrupted = true;
                break;
            }
        }
        if (corrupted)
            break;
    }
    const typed::VerificationResult malformedResult = typed::Verifier{}.verify(malformed);
    ok &= expect(
        corrupted && hasCode(malformedResult, "WIR1422"),
        "Typed WIR verifier must reject non-numeric conversion destinations");

    return ok ? 0 : 1;
}
