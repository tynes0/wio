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

    Lexer lexer(
        "interface IDamageable { "
        "  fn TakeDamage(amount: i32); "
        "  fn GetHp() -> i32; "
        "} "
        "[Default(public)] object Entity { "
        "  fn GetHp() -> i32 { return 1; } "
        "} "
        "[From(Entity), From(IDamageable), Default(public)] "
        "object Boss { "
        "  hp: i32; "
        "  OnConstruct(hp: i32) { self.hp = hp; } "
        "  fn TakeDamage(amount: i32) { self.hp -= amount; } "
        "  fn GetHp() -> i32 { return self.hp; } "
        "  fn Same(other: view Boss) -> bool { return self == other; } "
        "  fn AsValue() -> Boss { return deref self; } "
        "  fn AsRef() -> ref Boss { return self; } "
        "  fn AsView() -> view Boss { return self; } "
        "  fn BaseHp() -> i32 { return super.GetHp(); } "
        "} "
        "fn ApplyHit(target: ref IDamageable, amount: i32) { target.TakeDamage(amount); } "
        "fn Read(target: view IDamageable) -> i32 { return target.GetHp(); } "
        "fn Exercise() -> i32 { "
        "  mut boss = Boss(12); "
        "  ApplyHit(ref boss, 3); "
        "  let damageable = ref boss fit IDamageable; "
        "  if ref boss is IDamageable { damageable.TakeDamage(2); } "
        "  if boss.Same(boss) { return Read(damageable); } "
        "  return 0; "
        "} "
        "fn Entry() -> i32 { return Exercise(); }",
        "typed_wir_object_interface_test.wio");
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

    bool ok = expect(build.succeeded(), "Object/interface source must build into Typed WIR");
    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(build.module());
    if (!typedVerification.succeeded())
    {
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(typedVerification.succeeded(), "Object/interface Typed WIR must verify");

    bool interfaceLayout = false;
    bool objectLayout = false;
    std::size_t receiverMethods = 0;
    std::size_t virtualCalls = 0;
    std::size_t interfaceCalls = 0;
    std::size_t directMethodCalls = 0;
    std::size_t upcasts = 0;
    std::size_t checkedCasts = 0;
    std::size_t typeTests = 0;
    std::size_t identityComparisons = 0;
    std::size_t fieldPlaces = 0;
    std::size_t borrows = 0;

    for (const Type& type : build.module().types.types())
    {
        if (type.name == "IDamageable")
            interfaceLayout = type.nominalKind == NominalKind::Interface && type.methods.size() == 2 &&
                type.methods[0].slot != type.methods[1].slot &&
                type.methods[0].isAbstract && type.methods[1].isAbstract;
        if (type.name == "Boss")
        {
            objectLayout = type.nominalKind == NominalKind::Object && type.methods.size() == 7 &&
                type.methods[0].slot != type.methods[1].slot;
        }
    }

    for (const typed::Function& function : build.module().functions)
    {
        if (function.isMethod)
        {
            ++receiverMethods;
            ok &= expect(!function.parameters.empty() && function.parameters.front().name == "self",
                "Every WIR method must expose self as its leading receiver parameter");
        }
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                virtualCalls += instruction.opcode == typed::Opcode::VirtualCall;
                interfaceCalls += instruction.opcode == typed::Opcode::InterfaceCall;
                directMethodCalls += instruction.opcode == typed::Opcode::MethodCall;
                upcasts += instruction.opcode == typed::Opcode::Upcast;
                checkedCasts += instruction.opcode == typed::Opcode::CheckedCast;
                typeTests += instruction.opcode == typed::Opcode::TypeTest;
                identityComparisons += instruction.opcode == typed::Opcode::IdentityEqual;
                fieldPlaces += instruction.opcode == typed::Opcode::FieldPlace;
                borrows += instruction.opcode == typed::Opcode::Borrow;
            }
        }
    }

    ok &= expect(interfaceLayout && objectLayout,
        "Object/interface WIR types must expose deterministic method slots and abstract interface entries");
    ok &= expect(receiverMethods == 11 && fieldPlaces >= 3 && borrows >= 1,
        "Interface, lifecycle, and object method bodies must retain receiver and self-field semantics");
    ok &= expect(virtualCalls >= 1 && interfaceCalls >= 3 && directMethodCalls >= 1,
        "Typed WIR must distinguish object virtual dispatch from interface dispatch");
    ok &= expect(upcasts >= 1 && checkedCasts == 1 && typeTests == 1 && identityComparisons == 1,
        "Typed WIR must preserve implicit upcast, checked fit, runtime type test, and identity equality");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(typedText.find("methods={") != std::string::npos &&
        typedText.find("virtual-call") != std::string::npos &&
        typedText.find("interface-call") != std::string::npos &&
        typedText.find("checked-cast") != std::string::npos,
        "Typed WIR printer must expose the object/interface model");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
    {
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    ok &= expect(lowering.succeeded(), "Object/interface Typed WIR must lower successfully");
    const lowered::VerificationResult loweredVerification = lowered::Verifier{}.verify(lowering.module());
    ok &= expect(loweredVerification.succeeded(), "Object/interface Lowered WIR must verify");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(loweredText.find("interface-call") != std::string::npos &&
        loweredText.find("type-test") != std::string::npos,
        "Lowered WIR must preserve backend-neutral dispatch and runtime type operations");

    typed::Module malformed = build.module();
    bool damaged = false;
    for (typed::Function& function : malformed.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damaged && instruction.opcode == typed::Opcode::InterfaceCall)
                {
                    ++instruction.projectionIndex;
                    damaged = true;
                }
            }
        }
    }
    ok &= expect(damaged && !typed::Verifier{}.verify(malformed).succeeded(),
        "Typed WIR verifier must reject interface dispatch through an invalid method slot");

    lowered::Module malformedLowered = lowering.module();
    bool damagedLowered = false;
    for (lowered::Function& function : malformedLowered.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (!damagedLowered && instruction.opcode == lowered::Opcode::CheckedCast)
                {
                    instruction.targetType = malformedLowered.types.i32Type();
                    damagedLowered = true;
                }
            }
        }
    }
    ok &= expect(damagedLowered && !lowered::Verifier{}.verify(malformedLowered).succeeded(),
        "Lowered WIR verifier must reject a checked cast with a non-object target");

    return ok ? 0 : 1;
}
