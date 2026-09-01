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
#include <unordered_set>

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
        "fn Transform(value: i32) -> i32 { return value + 1; } "
        "fn Transform(value: f32) -> f32 { return value + 1.0f32; } "
        "fn Identity<T>(value: T) -> T { return value; } "
        "fn Increment(value: i32) -> i32 { return value + 1; } "
        "fn Apply(value: i32, callback: fn(i32) -> i32) -> i32 { return callback(value); } "
        "component Counter { "
        "  value: i32; "
        "  OnConstruct(value: i32) { self.value = value; } "
        "} "
        "extension CounterOps for Counter { "
        "  public ref fn Add(amount: i32) -> i32 { self.value += amount; return self.value; } "
        "  public view fn Echo<T>(value: T) -> T { return value; } "
        "} "
        "object Holder { "
        "  value: i32; "
        "  public fn OnConstruct(value: i32) { self.value = value; } "
        "  public fn Reader() -> fn() -> i32 { return () => self.value; } "
        "} "
        "fn Entry() -> i32 { "
        "  let direct: fn(i32) -> i32 = Increment; "
        "  let first = Apply(Transform(2), direct); "
        "  let transformedFloat = Transform(1.0f32); "
        "  let captured = 3; "
        "  let closure: fn(i32) -> i32 = (value) => value + captured; "
        "  let second = Apply(first, closure); "
        "  let genericInteger = Identity(second); "
        "  let genericString = Identity(\"wio\"); "
        "  mut counter = Counter(genericInteger); "
        "  let updated = counter.Add(1); "
        "  let echoed = counter.Echo<string>(genericString); "
        "  let holder = Holder(updated); "
        "  let reader = holder.Reader(); "
        "  return reader(); "
        "}",
        "typed_wir_callable_model_test.wio");
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
    bool ok = expect(build.succeeded(), "Callable source must build into Typed WIR");

    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(build.module());
    if (!typedVerification.succeeded())
    {
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(typedVerification.succeeded(), "Callable Typed WIR must verify");

    std::size_t functionReferences = 0;
    std::size_t closures = 0;
    std::size_t indirectCalls = 0;
    std::size_t extensionCalls = 0;
    std::size_t retainedSelfCaptures = 0;
    std::size_t valueCaptures = 0;
    std::unordered_set<std::string> genericSpecializations;
    std::unordered_set<FunctionId::ValueType> transformTargets;
    bool genericDeclaration = false;
    bool extensionDeclaration = false;
    bool closureDeclaration = false;

    for (const typed::Function& function : build.module().functions)
    {
        genericDeclaration |= !function.genericParameters.empty();
        extensionDeclaration |= function.isExtension;
        closureDeclaration |= function.isClosureBody;
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                functionReferences += instruction.opcode == typed::Opcode::FunctionReference;
                closures += instruction.opcode == typed::Opcode::ClosureCreate;
                indirectCalls += instruction.opcode == typed::Opcode::IndirectCall;
                extensionCalls += instruction.opcode == typed::Opcode::ExtensionCall;
                if (instruction.opcode == typed::Opcode::ClosureCreate)
                {
                    for (const CaptureKind kind : instruction.captureKinds)
                    {
                        valueCaptures += kind == CaptureKind::Value;
                        retainedSelfCaptures += kind == CaptureKind::RetainedSelf;
                    }
                }
                if (instruction.opcode == typed::Opcode::Call && !instruction.genericArguments.empty())
                    genericSpecializations.insert(instruction.specializationKey);
                if (instruction.opcode == typed::Opcode::Call &&
                    instruction.signatureTypes.size() == 1 &&
                    (instruction.resultType == build.module().types.i32Type() ||
                     build.module().types.get(instruction.resultType).kind == TypeKind::F32))
                {
                    const auto target = std::ranges::find_if(build.module().functions, [&](const typed::Function& candidate)
                        { return candidate.id == instruction.callee && candidate.name.find("Transform") != std::string::npos; });
                    if (target != build.module().functions.end())
                        transformTargets.insert(instruction.callee.value());
                }
            }
        }
    }

    ok &= expect(functionReferences >= 1 && closures >= 2 && indirectCalls >= 2,
        "Callable WIR must represent named function values, closures, and indirect calls explicitly");
    ok &= expect(extensionCalls == 2 && extensionDeclaration,
        "Extension calls must pin their external implementation callable");
    ok &= expect(valueCaptures >= 1 && retainedSelfCaptures == 1 && closureDeclaration,
        "Closure layouts must distinguish value captures from retained self captures");
    ok &= expect(genericDeclaration && genericSpecializations.size() == 2,
        "Generic calls must preserve distinct concrete specialization identities");
    ok &= expect(transformTargets.size() == 2,
        "Overload resolution must be frozen to two distinct callable declaration ids");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(typedText.find("function-ref") != std::string::npos &&
        typedText.find("closure-create") != std::string::npos &&
        typedText.find("indirect-call") != std::string::npos &&
        typedText.find("extension-call") != std::string::npos &&
        typedText.find("specialization=") != std::string::npos,
        "Typed WIR printer must expose the complete callable model");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    ok &= expect(lowering.succeeded(), "Callable Typed WIR must lower successfully");
    const lowered::VerificationResult loweredVerification = lowered::Verifier{}.verify(lowering.module());
    if (!loweredVerification.succeeded())
    {
        for (const auto& diagnostic : loweredVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << lowered::Printer{}.print(lowering.module());
    }
    ok &= expect(loweredVerification.succeeded(), "Callable Lowered WIR must verify");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(loweredText.find("closure-create") != std::string::npos &&
        loweredText.find("indirect-call") != std::string::npos &&
        loweredText.find("extension-call") != std::string::npos,
        "Lowered WIR must preserve backend-neutral callable operations");

    typed::Module malformed = build.module();
    bool damaged = false;
    for (typed::Function& function : malformed.functions)
    {
        for (typed::BasicBlock& block : function.blocks)
        {
            for (typed::Instruction& instruction : block.instructions)
            {
                if (!damaged && instruction.opcode == typed::Opcode::ClosureCreate && !instruction.captureKinds.empty())
                {
                    instruction.captureKinds.front() = instruction.captureKinds.front() == CaptureKind::RetainedSelf
                        ? CaptureKind::Value
                        : CaptureKind::RetainedSelf;
                    damaged = true;
                }
            }
        }
    }
    ok &= expect(damaged && !typed::Verifier{}.verify(malformed).succeeded(),
        "Typed WIR verifier must reject a closure with corrupted capture semantics");

    lowered::Module malformedLowered = lowering.module();
    bool damagedLowered = false;
    for (lowered::Function& function : malformedLowered.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (!damagedLowered && instruction.opcode == lowered::Opcode::IndirectCall &&
                    instruction.signatureTypes.size() > 1)
                {
                    instruction.signatureTypes.back() = malformedLowered.types.stringType();
                    damagedLowered = true;
                }
            }
        }
    }
    ok &= expect(damagedLowered && !lowered::Verifier{}.verify(malformedLowered).succeeded(),
        "Lowered WIR verifier must reject an indirect call with a corrupted concrete signature");

    return ok ? 0 : 1;
}
