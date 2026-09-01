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
        "component Point { "
        "  public x: i32; private y: i32; "
        "  OnConstruct(x: i32, y: i32) { self.x = x; self.y = y; } "
        "  OnDestruct() {} "
        "} "
        "object Owner { "
        "  public id: i32; "
        "  OnConstruct(id: i32) { self.id = id; } "
        "  OnDestruct() {} "
        "} "
        "fn MakePoint() -> Point { "
        "  let point = Point(1, 2); "
        "  return point; "
        "} "
        "fn Exercise(flag: bool) -> i32 { "
        "  let owner = Owner(7); "
        "  let shared = owner; "
        "  { "
        "    let value = Point(3, 4); "
        "    if flag { return value.x; } "
        "  } "
        "  return shared.id; "
        "} "
        "fn Loop(flag: bool) -> i32 { "
        "  mut count: i32 = 0; "
        "  while count < 2 { "
        "    let scoped = Point(count, count); "
        "    count += 1; "
        "    if flag { break; } "
        "    continue; "
        "  } "
        "  return count; "
        "} "
        "fn Entry() -> i32 { return Exercise(false) + Loop(false); }",
        "typed_wir_lifecycle_test.wio");
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
    ok &= expect(build.succeeded(), "Component/object construction and lifetime source must build into Typed WIR");

    const typed::VerificationResult verification = typed::Verifier{}.verify(build.module());
    if (!verification.succeeded())
    {
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(verification.succeeded(), "Construction and lifetime Typed WIR must verify");

    bool pointLayout = false;
    bool ownerLayout = false;
    for (const Type& type : build.module().types.types())
    {
        if (type.kind != TypeKind::Named)
            continue;
        if (type.name == "Point")
        {
            pointLayout = type.nominalKind == NominalKind::Component && type.fields.size() == 2 &&
                type.fields[0].name == "x" && type.fields[0].type == build.module().types.i32Type() &&
                type.fields[0].visibility == FieldVisibility::Public &&
                type.fields[1].name == "y" && type.fields[1].visibility == FieldVisibility::Private &&
                type.hasConstructor && type.hasDestructor;
        }
        if (type.name == "Owner")
        {
            ownerLayout = type.nominalKind == NominalKind::Object && type.fields.size() == 1 &&
                type.fields.front().name == "id" && type.hasConstructor && type.hasDestructor;
        }
    }
    ok &= expect(pointLayout && ownerLayout,
        "Nominal WIR types must retain ordered field layout, visibility, and lifecycle capabilities");

    std::size_t componentConstructions = 0;
    std::size_t objectConstructions = 0;
    std::size_t drops = 0;
    std::size_t fieldPlaces = 0;
    for (const typed::Function& function : build.module().functions)
    {
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                componentConstructions += instruction.opcode == typed::Opcode::ConstructComponent;
                objectConstructions += instruction.opcode == typed::Opcode::ConstructObject;
                drops += instruction.opcode == typed::Opcode::Drop;
                fieldPlaces += instruction.opcode == typed::Opcode::FieldPlace;
            }
        }
    }
    ok &= expect(componentConstructions == 3 && objectConstructions == 1,
        "Typed WIR must distinguish stack component construction from owning object allocation");
    ok &= expect(drops >= 9 && fieldPlaces >= 2,
        "Typed WIR must expose reverse lexical cleanup on normal, early-return, break, and continue paths");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(
        typedText.find("fields={") != std::string::npos &&
            typedText.find("has-destructor") != std::string::npos &&
            typedText.find("construct-component") != std::string::npos &&
            typedText.find("construct-object") != std::string::npos &&
            typedText.find("drop") != std::string::npos,
        "Typed WIR printer must expose nominal layout, construction, and cleanup");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    ok &= expect(lowering.succeeded(), "Construction and lifecycle Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(
        loweredText.find("construct-component") != std::string::npos &&
            loweredText.find("construct-object") != std::string::npos &&
            loweredText.find("drop") != std::string::npos,
        "Lowered WIR must preserve backend-neutral construction and cleanup operations");

    typed::Module malformed = build.module();
    bool damaged = false;
    for (typed::Function& function : malformed.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damaged && instruction.opcode == typed::Opcode::FieldPlace)
                {
                    instruction.selector = "missing_field";
                    damaged = true;
                }
            }
        }
    }
    ok &= expect(damaged && !typed::Verifier{}.verify(malformed).succeeded(),
        "Typed WIR verifier must reject field projections absent from the nominal layout");

    typed::Module malformedConstruction = build.module();
    bool damagedConstruction = false;
    for (typed::Function& function : malformedConstruction.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damagedConstruction &&
                    (instruction.opcode == typed::Opcode::ConstructComponent ||
                     instruction.opcode == typed::Opcode::ConstructObject) &&
                    !instruction.signatureTypes.empty())
                {
                    instruction.signatureTypes.pop_back();
                    damagedConstruction = true;
                }
            }
        }
    }
    ok &= expect(damagedConstruction && !typed::Verifier{}.verify(malformedConstruction).succeeded(),
        "Typed WIR verifier must reject constructor signatures that do not match their operands");

    return ok ? 0 : 1;
}
