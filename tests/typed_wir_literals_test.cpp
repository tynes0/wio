#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"

#include <cmath>
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
        "object Boxed {} "
        "fn TakeU64(value: u64) -> u64 { return value; } "
        "fn Types() -> u64 { "
        "  let explicit = 3u64; "
        "  let contextual: u64 = 4; "
        "  let argument = TakeU64(5); "
        "  mut assigned: u64 = 1; "
        "  assigned = 6; "
        "  let hexadecimal: u16 = 0xFF; "
        "  let defaulted = 9; "
        "  let contextualFloat: f32 = 1.25; "
        "  let explicitFloat = 2.5f32; "
        "  let defaultFloat = 3.75; "
        "  let unicode: text = u\"Selam 世界\"; "
        "  let ascii: string = \"Wio\"; "
        "  let character: char = 'A'; "
        "  return 7; "
        "} "
        "fn Maybe() -> Boxed? { return null; } "
        "fn Entry() -> i32 { return 0; }",
        "typed_wir_literals_test.wio");
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
    ok &= expect(build.succeeded(), "Contextual, suffixed, Unicode, char, and null literals must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Typed WIR literal values must pass type verification");

    bool sawExplicitU64 = false;
    bool sawContextualU64 = false;
    bool sawContextualArgument = false;
    bool sawContextualAssignment = false;
    bool sawHexU16 = false;
    bool sawDefaultI32 = false;
    bool sawF32 = false;
    bool sawF64 = false;
    bool sawText = false;
    bool sawString = false;
    bool sawChar = false;
    bool sawNull = false;

    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode != typed::Opcode::Constant)
                    continue;
                const Type& type = build.module().types.get(instruction.resultType);
                if (const auto* value = std::get_if<std::uint64_t>(&instruction.literal))
                {
                    sawExplicitU64 = sawExplicitU64 || (type.kind == TypeKind::U64 && *value == 3);
                    sawContextualU64 = sawContextualU64 || (type.kind == TypeKind::U64 && *value == 4);
                    sawContextualArgument = sawContextualArgument || (type.kind == TypeKind::U64 && *value == 5);
                    sawContextualAssignment = sawContextualAssignment || (type.kind == TypeKind::U64 && *value == 6);
                    sawHexU16 = sawHexU16 || (type.kind == TypeKind::U16 && *value == 255);
                    sawChar = sawChar || (type.kind == TypeKind::Char && *value == 65);
                }
                else if (const auto* value = std::get_if<std::int64_t>(&instruction.literal))
                    sawDefaultI32 = sawDefaultI32 || (type.kind == TypeKind::I32 && *value == 9);
                else if (const auto* value = std::get_if<double>(&instruction.literal))
                {
                    sawF32 = sawF32 || (type.kind == TypeKind::F32 && std::abs(*value - 1.25) < 0.0001);
                    sawF64 = sawF64 || (type.kind == TypeKind::F64 && std::abs(*value - 3.75) < 0.0001);
                }
                else if (const auto* value = std::get_if<std::string>(&instruction.literal))
                {
                    sawText = sawText || (type.kind == TypeKind::Text && value->find("Selam") == 0);
                    sawString = sawString || (type.kind == TypeKind::String && *value == "Wio");
                }
                else if (std::holds_alternative<typed::NullLiteral>(instruction.literal))
                    sawNull = sawNull || type.kind == TypeKind::Nullable;
            }
        }
    }

    ok &= expect(
        sawExplicitU64 && sawContextualU64 && sawContextualArgument && sawContextualAssignment && sawHexU16 &&
            sawDefaultI32 && sawF32 && sawF64 && sawText && sawString && sawChar && sawNull,
        "WIR literals must preserve explicit suffixes, contextual types, defaults, encodings, and nullability");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Typed WIR literals must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("const null") != std::string::npos &&
            loweredText.find("text") != std::string::npos,
        "Lowered WIR must retain null and Unicode text literal metadata");

    return ok ? 0 : 1;
}
