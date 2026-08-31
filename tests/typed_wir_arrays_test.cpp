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
        "fn Read(values: i32[], index: usize) -> i32 { return values[index]; } "
        "fn Build() -> i64 { "
        "  let values: i64[] = [1, 2, 3]; "
        "  return values[1]; "
        "} "
        "fn Inferred() -> i32 { "
        "  let values = [4, 5]; "
        "  return values[0]; "
        "}",
        "typed_wir_arrays_test.wio");
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
    ok &= expect(build.succeeded(), "Contextual and inferred arrays must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Array creation and indexing must pass Typed WIR verification");

    std::size_t createCount = 0;
    std::size_t getCount = 0;
    bool sawContextualI64Elements = false;
    bool sawInferredExtent = false;
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == typed::Opcode::ArrayCreate)
                {
                    ++createCount;
                    const Type& arrayType = build.module().types.get(instruction.resultType);
                    const Type& elementType = build.module().types.get(arrayType.arguments.front());
                    sawContextualI64Elements = sawContextualI64Elements ||
                        (function.name == "Build" && elementType.kind == TypeKind::I64);
                    sawInferredExtent = sawInferredExtent ||
                        (function.name == "Inferred" && arrayType.staticExtent == 2);
                }
                else if (instruction.opcode == typed::Opcode::ArrayGet)
                {
                    ++getCount;
                }
            }
        }
    }
    ok &= expect(
        createCount == 2 && getCount == 3 && sawContextualI64Elements && sawInferredExtent,
        "Typed WIR must preserve contextual element types, inferred literal extent, and index reads");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("array-create") != std::string::npos &&
            typedText.find("array-get") != std::string::npos,
        "Typed WIR printer must expose array creation and reads");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Array Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("array-create") != std::string::npos &&
            loweredText.find("array-get") != std::string::npos,
        "Lowered WIR must preserve backend-neutral array operations");

    typed::Module malformed = build.module();
    bool damaged = false;
    for (typed::Function& function : malformed.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damaged && instruction.opcode == typed::Opcode::ArrayGet)
                {
                    instruction.operands.pop_back();
                    damaged = true;
                }
            }
        }
    }
    ok &= expect(
        damaged && !typed::Verifier{}.verify(malformed).succeeded(),
        "Typed WIR verifier must reject an array read without an index");

    return ok ? 0 : 1;
}
