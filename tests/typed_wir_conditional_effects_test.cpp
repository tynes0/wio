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
        "fn First() -> i32 { return 11; } "
        "fn Second() -> i32 { return 22; } "
        "fn Choose(condition: bool) -> i64 { "
        "  return condition ? First() : Second(); "
        "} "
        "fn Nested(outer: bool, inner: bool) -> i32 { "
        "  return outer ? (inner ? First() : Second()) : Second(); "
        "} "
        "fn Entry() -> i32 { return Choose(true) fit i32; }",
        "typed_wir_conditional_effects_test.wio");
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
    ok &= expect(build.succeeded(), "Side-effecting conditional expressions must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Conditional expression CFG must pass Typed WIR verification");

    const typed::Function* choose = nullptr;
    for (const typed::Function& function : build.module().functions)
    {
        if (function.name == "Choose")
            choose = &function;
    }
    ok &= expect(choose != nullptr, "Choose function must be present in Typed WIR");

    bool sawTrueCall = false;
    bool sawFalseCall = false;
    bool sawI64Merge = false;
    bool sawSelect = false;
    if (choose)
    {
        for (const typed::BasicBlock& block : choose->blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                sawSelect = sawSelect || instruction.opcode == typed::Opcode::Select;
                if (instruction.opcode == typed::Opcode::Call)
                {
                    sawTrueCall = sawTrueCall || block.name == "conditional.true";
                    sawFalseCall = sawFalseCall || block.name == "conditional.false";
                }
            }
            if (block.name == "conditional.merge" && !block.parameters.empty())
            {
                sawI64Merge = build.module().types.get(block.parameters.front().type).kind == TypeKind::I64;
            }
        }
    }
    ok &= expect(
        sawTrueCall && sawFalseCall && sawI64Merge && !sawSelect,
        "Conditional calls must stay in separate branches and merge through the contextual result type");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("\"conditional.true\"") != std::string::npos &&
            typedText.find("\"conditional.false\"") != std::string::npos &&
            typedText.find("\"conditional.merge\"") != std::string::npos &&
            typedText.find("\"conditional.result\"") != std::string::npos,
        "Typed WIR printer must expose conditional effect regions deterministically");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Side-effecting conditional Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("cond-jump") != std::string::npos &&
            loweredText.find("\"conditional.merge\"") != std::string::npos,
        "Lowered WIR must preserve lazy conditional control flow");

    return ok ? 0 : 1;
}
