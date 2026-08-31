#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
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
        "realm std { component ResultError { public code: i32; } } "
        "object Option<T> {} "
        "object Result<T> {} "
        "fn FromOption(value: Option<i32>) -> i32 { "
        "  return match (value) { "
        "    Some(number) if number > 10: number; "
        "    Some(number): number + 1; "
        "    None(): 0; "
        "  }; "
        "} "
        "fn FromResult(value: Result<i32>) -> i32 { "
        "  return match (value) { "
        "    Ok(number): number; "
        "    Err(error): -1; "
        "  }; "
        "} "
        "fn FromArray(values: i32[]) -> i32 { "
        "  return match (values) { "
        "    [left, right] if left > 0: left + right; "
        "    [only]: only; "
        "    assumed: 0; "
        "  }; "
        "}",
        "typed_wir_data_match_test.wio");
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
    ok &= expect(build.succeeded(), "Option, Result, and array patterns must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Data-model match projections must pass Typed WIR verification");

    bool sawSomeTest = false;
    bool sawNoneTest = false;
    bool sawOkTest = false;
    bool sawErrTest = false;
    bool sawSomePayload = false;
    bool sawErrPayload = false;
    bool sawArrayLength = false;
    bool sawArrayLeft = false;
    bool sawArrayRight = false;
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == typed::Opcode::VariantTest)
                {
                    sawSomeTest = sawSomeTest || instruction.selector == "Some";
                    sawNoneTest = sawNoneTest || instruction.selector == "None";
                    sawOkTest = sawOkTest || instruction.selector == "Ok";
                    sawErrTest = sawErrTest || instruction.selector == "Err";
                }
                else if (instruction.opcode == typed::Opcode::VariantPayload)
                {
                    sawSomePayload = sawSomePayload || instruction.selector == "Some";
                    sawErrPayload = sawErrPayload || instruction.selector == "Err";
                }
                else if (instruction.opcode == typed::Opcode::ArrayLength)
                {
                    sawArrayLength = true;
                }
                else if (instruction.opcode == typed::Opcode::ArrayElement)
                {
                    sawArrayLeft = sawArrayLeft || instruction.projectionIndex == 0;
                    sawArrayRight = sawArrayRight || instruction.projectionIndex == 1;
                }
            }
        }
    }
    ok &= expect(
        sawSomeTest && sawNoneTest && sawOkTest && sawErrTest &&
            sawSomePayload && sawErrPayload && sawArrayLength && sawArrayLeft && sawArrayRight,
        "Typed WIR must expose variant tests, payloads, array length, and indexed projections");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("variant-test") != std::string::npos &&
            typedText.find("variant-payload") != std::string::npos &&
            typedText.find("array-length") != std::string::npos &&
            typedText.find("array-element") != std::string::npos,
        "Typed WIR printer must expose data-model operations");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    ok &= expect(lowering.succeeded(), "Data-model match Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("variant-test") != std::string::npos &&
            loweredText.find("variant-payload") != std::string::npos &&
            loweredText.find("array-length") != std::string::npos &&
            loweredText.find("array-element") != std::string::npos,
        "Lowered WIR must preserve backend-neutral data-model operations");

    typed::Module malformedTyped = build.module();
    bool damagedTypedProjection = false;
    for (typed::Function& function : malformedTyped.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damagedTypedProjection && instruction.opcode == typed::Opcode::VariantTest)
                {
                    instruction.selector.clear();
                    damagedTypedProjection = true;
                }
            }
        }
    }
    ok &= expect(
        damagedTypedProjection && !typed::Verifier{}.verify(malformedTyped).succeeded(),
        "Typed WIR verifier must reject a variant test without a selector");

    lowered::Module malformedLowered = lowering.module();
    bool damagedLoweredProjection = false;
    for (lowered::Function& function : malformedLowered.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (!damagedLoweredProjection && instruction.opcode == lowered::Opcode::ArrayLength)
                {
                    instruction.resultType = malformedLowered.types.i32Type();
                    damagedLoweredProjection = true;
                }
            }
        }
    }
    ok &= expect(
        damagedLoweredProjection && !lowered::Verifier{}.verify(malformedLowered).succeeded(),
        "Lowered WIR verifier must reject an array length with a non-usize result");

    return ok ? 0 : 1;
}
