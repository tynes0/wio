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
        "fn Guard() -> bool { return true; } "
        "fn Zero() -> i32 { return 10; } "
        "fn Small() -> i32 { return 20; } "
        "fn Mid() -> i32 { return 30; } "
        "fn Large() -> i32 { return 40; } "
        "fn Classify(value: i32) -> i64 { "
        "  return match (value) { "
        "    0: Zero(); "
        "    1, 2 or 3 if Guard(): Small(); "
        "    4...6: Mid(); "
        "    assumed: Large(); "
        "  }; "
        "} "
        "fn Accumulate(value: i32) -> i32 { "
        "  mut total: i32 = 1; "
        "  match (value) { "
        "    0: { total += 10; } "
        "    1, 2: { total += 20; } "
        "    assumed: { total += 30; } "
        "  }; "
        "  return total; "
        "} "
        "fn MaybeAccumulate(value: i32) -> i32 { "
        "  mut total: i32 = 5; "
        "  match (value) { "
        "    0: { total += 2; } "
        "  }; "
        "  return total; "
        "} "
        "fn Entry() -> i32 { return Classify(5) fit i32; }",
        "typed_wir_match_test.wio");
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
    ok &= expect(build.succeeded(), "Literal, range, guarded, and assumed match cases must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Value-producing match CFG must pass Typed WIR verification");

    const typed::Function* classify = nullptr;
    const typed::Function* accumulate = nullptr;
    const typed::Function* maybeAccumulate = nullptr;
    for (const typed::Function& function : build.module().functions)
    {
        if (function.name == "Classify")
            classify = &function;
        else if (function.name == "Accumulate")
            accumulate = &function;
        else if (function.name == "MaybeAccumulate")
            maybeAccumulate = &function;
    }
    ok &= expect(classify != nullptr, "Classify function must be present in Typed WIR");
    ok &= expect(
        accumulate != nullptr && maybeAccumulate != nullptr,
        "Statement-form match functions must be present in Typed WIR");

    bool sawGuard = false;
    bool sawRangeUpper = false;
    bool sawAssumedBody = false;
    bool sawI64Merge = false;
    bool sawRangeLowerComparison = false;
    bool sawRangeUpperComparison = false;
    if (classify)
    {
        for (const typed::BasicBlock& block : classify->blocks)
        {
            sawGuard = sawGuard || block.name == "match.case.1.guard";
            sawRangeUpper = sawRangeUpper || block.name == "match.range.upper";
            sawAssumedBody = sawAssumedBody || block.name == "match.case.3.body";
            for (const typed::Instruction& instruction : block.instructions)
            {
                sawRangeLowerComparison = sawRangeLowerComparison ||
                    (instruction.opcode == typed::Opcode::Binary &&
                     instruction.binaryOperator == typed::BinaryOperator::GreaterEqual);
                sawRangeUpperComparison = sawRangeUpperComparison ||
                    (instruction.opcode == typed::Opcode::Binary &&
                     instruction.binaryOperator == typed::BinaryOperator::LessEqual);
            }
            if (block.name == "match.merge" && !block.parameters.empty())
                sawI64Merge = build.module().types.get(block.parameters.front().type).kind == TypeKind::I64;
        }
    }
    ok &= expect(
        sawGuard && sawRangeUpper && sawAssumedBody && sawI64Merge &&
            sawRangeLowerComparison && sawRangeUpperComparison,
        "Match WIR must preserve guards, inclusive ranges, assumed fallback, and contextual result type");

    bool sawStatementMerge = false;
    bool sawUnmatchedFallthrough = false;
    if (accumulate)
    {
        for (const typed::BasicBlock& block : accumulate->blocks)
        {
            if (block.name == "match.merge")
            {
                sawStatementMerge = !block.parameters.empty() &&
                    block.parameters.front().name == "total.match";
            }
        }
    }
    if (maybeAccumulate)
    {
        for (const typed::BasicBlock& block : maybeAccumulate->blocks)
        {
            if (block.name == "match.unmatched" && !block.instructions.empty())
            {
                sawUnmatchedFallthrough =
                    block.instructions.back().opcode == typed::Opcode::Branch;
            }
        }
    }
    ok &= expect(
        sawStatementMerge && sawUnmatchedFallthrough,
        "Statement-form match must merge mutated locals and preserve unmatched fallthrough without assumed");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("\"match.pattern.next\"") != std::string::npos &&
            typedText.find("\"match.unmatched\"") != std::string::npos &&
            typedText.find("\"match.result\"") != std::string::npos,
        "Typed WIR match output must expose deterministic pattern control flow");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Value and statement-form match Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("cond-jump") != std::string::npos &&
            loweredText.find("\"match.merge\"") != std::string::npos &&
            loweredText.find("numeric-widen") != std::string::npos,
        "Lowered WIR must preserve match branches and contextual arm widening");

    return ok ? 0 : 1;
}
