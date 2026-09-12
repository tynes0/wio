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
        "fn Count(limit: i32) -> i32 { "
        "  mut index: i32; "
        "  mut total: i32; "
        "  while (index < limit) { "
        "    index += 1; "
        "    if (index == 2) { continue; } "
        "    total += index; "
        "    if (total > 10) { break; } "
        "  } "
        "  return total + index; "
        "} "
        "fn Entry() -> i32 { return Count(8); }",
        "typed_wir_loop_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    ok &= expect(build.succeeded(), "While/break/continue must build into Typed WIR");
    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Loop-carried Typed WIR must pass verification");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("\"while.header\"") != std::string::npos &&
            typedText.find("\"while.body\"") != std::string::npos &&
            typedText.find("\"while.condition-exit\"") != std::string::npos &&
            typedText.find("\"while.exit\"") != std::string::npos &&
            typedText.find("\"index.loop\"") != std::string::npos &&
            typedText.find("\"total.after\"") != std::string::npos,
        "Typed WIR printer must expose deterministic loop header and exit parameters");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Loop-carried Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("cond-jump") != std::string::npos &&
            loweredText.find("\"while.header\"") != std::string::npos &&
            loweredText.find("\"while.exit\"") != std::string::npos,
        "Lowered WIR must retain canonical loop jumps and block arguments");

    typed::Module malformed = build.module();
    typed::Function& count = malformed.functions.front();
    count.blocks.front().instructions.back().operands.pop_back();
    const typed::VerificationResult malformedResult = typed::Verifier{}.verify(malformed);
    ok &= expect(
        hasCode(malformedResult, "WIR1417"),
        "Typed WIR verifier must reject missing loop-carried branch arguments");

    typed::Module nonDominating = build.module();
    typed::Function& nonDominatingCount = nonDominating.functions.front();
    typed::BasicBlock* bodyBlock = nullptr;
    typed::BasicBlock* conditionExitBlock = nullptr;
    for (typed::BasicBlock& block : nonDominatingCount.blocks)
    {
        if (block.name == "while.body")
            bodyBlock = &block;
        else if (block.name == "while.condition-exit")
            conditionExitBlock = &block;
    }
    if (bodyBlock && conditionExitBlock &&
        !bodyBlock->instructions.empty() && bodyBlock->instructions.front().result &&
        !conditionExitBlock->instructions.empty() &&
        !conditionExitBlock->instructions.back().operands.empty())
    {
        conditionExitBlock->instructions.back().operands.front() =
            bodyBlock->instructions.front().result;
    }
    const typed::VerificationResult dominanceResult = typed::Verifier{}.verify(nonDominating);
    ok &= expect(
        hasCode(dominanceResult, "WIR1421"),
        "Typed WIR verifier must reject a value whose definition does not dominate its use");

    return ok ? 0 : 1;
}
