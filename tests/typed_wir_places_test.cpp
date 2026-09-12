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
        "component Pair { public x: i32; public y: i32; } "
        "fn First(values: ref i32[]) -> ref i32 { return ref values[0usize]; } "
        "fn Update(values: ref i32[], target: ref Pair) -> i32 { "
        "  target.x += 2; "
        "  values[1usize] = target.x; "
        "  mut local: i32 = 3; "
        "  local += 4; "
        "  if target.y > 0 { local += target.y; } else { local -= 1; } "
        "  let observed: view i32 = ref local; "
        "  return First(values) + deref observed + target.x; "
        "}",
        "typed_wir_places_test.wio");
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
    ok &= expect(build.succeeded(), "Place-oriented source must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Place-oriented Typed WIR must verify");

    std::size_t localPlaces = 0;
    std::size_t initializations = 0;
    std::size_t loads = 0;
    std::size_t stores = 0;
    std::size_t fieldPlaces = 0;
    std::size_t arrayPlaces = 0;
    std::size_t borrows = 0;
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                localPlaces += instruction.opcode == typed::Opcode::LocalPlace;
                initializations += instruction.opcode == typed::Opcode::PlaceInit;
                loads += instruction.opcode == typed::Opcode::Load;
                stores += instruction.opcode == typed::Opcode::Store;
                fieldPlaces += instruction.opcode == typed::Opcode::FieldPlace;
                arrayPlaces += instruction.opcode == typed::Opcode::ArrayPlace;
                borrows += instruction.opcode == typed::Opcode::Borrow;
            }
        }
    }
    ok &= expect(localPlaces == 2 && initializations == 2,
        "Local values, including stored views, must have explicit initialized places");
    ok &= expect(loads >= 8 && stores >= 5 && fieldPlaces >= 5 && arrayPlaces >= 2 && borrows >= 1,
        "Typed WIR must expose loads, stores, projections, and read-only borrows");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("local-place") != std::string::npos &&
            typedText.find("place-init") != std::string::npos &&
            typedText.find("field-place") != std::string::npos &&
            typedText.find("array-place") != std::string::npos &&
            typedText.find("borrow") != std::string::npos,
        "Typed WIR printer must make memory semantics inspectable");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    ok &= expect(lowering.succeeded(), "Place-oriented Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("local-place") != std::string::npos &&
            loweredText.find("store") != std::string::npos &&
            loweredText.find("field-place") != std::string::npos,
        "Lowered WIR must preserve backend-neutral memory operations");

    typed::Module malformed = build.module();
    bool damaged = false;
    for (typed::Function& function : malformed.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damaged && instruction.opcode == typed::Opcode::Store)
                {
                    const Type& mutablePlaceType = malformed.types.get(
                        [&]() -> TypeId
                        {
                            for (const typed::Instruction& candidate : block.instructions)
                            {
                                if (candidate.result == instruction.operands.front())
                                    return candidate.resultType;
                            }
                            return {};
                        }());
                    Type readOnlyPlaceType = mutablePlaceType;
                    readOnlyPlaceType.isMutable = false;
                    const TypeId readOnlyId = malformed.types.intern(std::move(readOnlyPlaceType));
                    for (typed::Instruction& candidate : block.instructions)
                    {
                        if (candidate.result == instruction.operands.front())
                        {
                            candidate.resultType = readOnlyId;
                            damaged = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    ok &= expect(damaged && !typed::Verifier{}.verify(malformed).succeeded(),
        "Typed WIR verifier must reject stores through read-only places");

    return ok ? 0 : 1;
}
