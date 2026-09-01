#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/native_abi_plan.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"
#include "wio_native_abi.h"

#include <algorithm>
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

    int retains = 0;
    int releases = 0;

    void retainHandle(void*) noexcept { ++retains; }
    void releaseHandle(void*) noexcept { ++releases; }
    std::uint64_t handleType(const void*) noexcept { return 0xC0FFEEu; }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace typed = wio::wir::typed;
    namespace lowered = wio::wir::lowered;

    bool ok = true;
    Lexer lexer(
        "[Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::Vec2)] "
        "component Vec2 { x: f32; y: f32; } "
        "extension Vec2Native for Vec2 { "
        "  [Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::Scale)] "
        "  public ref fn Scale(amount: f32); "
        "} "
        "realm ffi { "
        "  [Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::Compute)] "
        "  fn Compute(value: i32, callback: fn(i32) -> i32, context: opaque) -> i32; "
        "  [Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::Read)] "
        "  fn Read(value: view string) -> i32; "
        "  [Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::Write)] "
        "  fn Write(value: ref i32); "
        "  [Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::MakeContext)] "
        "  fn MakeContext() -> opaque; "
        "  [Native, CppHeader(\"native_abi_fixture.h\"), CppName(native_abi::Echo), Instantiate(i32)] "
        "  fn Echo<T>(value: T) -> T; "
        "} "
        "fn Entry() -> i32 { "
        "  mut vector = Vec2(1.0f32, 2.0f32); "
        "  vector.Scale(2.0f32); "
        "  let label: string = \"native\"; "
        "  let read = ffi::Read(ref label); "
        "  mut output: i32 = read; "
        "  ffi::Write(ref output); "
        "  let echoed = ffi::Echo(output); "
        "  let context = ffi::MakeContext(); "
        "  return ffi::Compute(echoed, (value) => value + 1, context); "
        "}",
        "typed_wir_native_abi_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    if (!build.succeeded())
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << " at " << diagnostic.source.begin.toDiagnosticString()
                      << ": " << diagnostic.message << '\n';
    ok &= expect(build.succeeded(), "Native ABI source must build into Typed WIR");

    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(build.module());
    if (!typedVerification.succeeded())
    {
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(build.module());
    }
    ok &= expect(typedVerification.succeeded(), "Native ABI Typed WIR must verify");

    std::size_t nativeFunctions = 0;
    std::size_t nativeCalls = 0;
    bool callbackContract = false;
    bool immutableBorrow = false;
    bool mutableBorrow = false;
    bool opaqueContract = false;
    bool extensionReceiver = false;
    bool genericThunk = false;
    bool podContract = false;
    bool callbackClaimReleased = false;
    for (const Type& type : build.module().types.types())
    {
        if (type.name == "Vec2")
            podContract = type.nominalRepresentation == NominalRepresentation::NativePod &&
                type.nativeBinding && type.nativeBinding->cppName == "native_abi::Vec2" &&
                type.nativeBinding->header == "native_abi_fixture.h";
    }
    for (const typed::Function& function : build.module().functions)
    {
        if (function.nativeBinding)
        {
            ++nativeFunctions;
            const NativeBinding& binding = *function.nativeBinding;
            ok &= expect(function.isExternal && !binding.stableKey.empty() && !binding.thunkSymbol.empty(),
                "Every native declaration must expose stable thunk identity");
            ok &= expect(binding.exceptionBoundary == NativeExceptionBoundary::TranslateToWioFailure,
                "C++ exceptions must be contained at every native boundary");
            extensionReceiver |= binding.receiver == NativeReceiverKind::MutableReference;
            genericThunk |= binding.thunkKind == NativeThunkKind::TemplateSpecialization &&
                binding.requiresAdapter;
            for (const NativeAbiValue& parameter : binding.parameters)
            {
                callbackContract |= parameter.marshalling == NativeMarshallingKind::Callback &&
                    parameter.callbackLifetime == NativeCallbackLifetime::Call &&
                    parameter.callbackThread == NativeCallbackThread::Caller;
                immutableBorrow |= parameter.passing == NativePassingMode::Borrow;
                mutableBorrow |= parameter.passing == NativePassingMode::BorrowMut;
                opaqueContract |= parameter.marshalling == NativeMarshallingKind::OpaqueHandle;
            }
        }
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (std::size_t index = 0; index < block.instructions.size(); ++index)
            {
                const typed::Instruction& instruction = block.instructions[index];
                nativeCalls += instruction.opcode == typed::Opcode::NativeCall;
                if (instruction.opcode != typed::Opcode::NativeCall || instruction.operands.size() < 2)
                    continue;
                const auto callee = std::ranges::find_if(build.module().functions,
                    [&](const typed::Function& candidate) { return candidate.id == instruction.callee; });
                if (callee == build.module().functions.end() || !callee->nativeBinding ||
                    callee->nativeBinding->parameters.size() < 2 ||
                    callee->nativeBinding->parameters[1].marshalling != NativeMarshallingKind::Callback)
                    continue;
                for (std::size_t cleanup = index + 1; cleanup < block.instructions.size(); ++cleanup)
                {
                    const typed::Instruction& candidate = block.instructions[cleanup];
                    if (candidate.opcode == typed::Opcode::Release &&
                        candidate.operands.size() == 1 && candidate.operands.front() == instruction.operands[1])
                    {
                        callbackClaimReleased = true;
                        break;
                    }
                    if (candidate.opcode == typed::Opcode::NativeCall)
                        break;
                }
            }
        }
    }
    ok &= expect(nativeFunctions == 6 && nativeCalls == 6,
        "Native declarations and calls must be explicit instead of generic external calls");
    ok &= expect(podContract && callbackContract && immutableBorrow && mutableBorrow &&
        opaqueContract && extensionReceiver && genericThunk && callbackClaimReleased,
        "POD, callback, ref/view, opaque and extension receiver ABI contracts must be frozen in WIR");

    const std::string typedText = typed::Printer{}.print(build.module());
    ok &= expect(typedText.find("native-call") != std::string::npos &&
        typedText.find("native-type[cpp=\"native_abi::Vec2\"") != std::string::npos &&
        typedText.find("callback/call/caller") != std::string::npos &&
        typedText.find("exception=translate-to-wio-failure") != std::string::npos,
        "Typed WIR printer must expose the complete native ABI decision");

    LoweringResult lowering = LoweringPipeline{}.lower(build.module());
    if (!lowering.succeeded())
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(lowering.succeeded(), "Native ABI Typed WIR must lower successfully");
    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(loweredText.find("native-invoke") != std::string::npos &&
        loweredText.find("thunk=\"_wio_native_") != std::string::npos,
        "Lowered WIR must preserve native invocation and thunk metadata");

    const NativeAbiPlanResult abiPlan = NativeAbiPlanner{}.plan(lowering.module());
    if (!abiPlan.succeeded())
        for (const NativeAbiPlanDiagnostic& diagnostic : abiPlan.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    bool concreteGenericThunk = false;
    for (const NativeThunkPlan& thunk : abiPlan.thunks())
        concreteGenericThunk |= !thunk.specializationKey.empty() &&
            thunk.kind == NativeThunkKind::TemplateSpecialization;
    ok &= expect(abiPlan.succeeded() && abiPlan.thunks().size() == 6 && concreteGenericThunk,
        "Native ABI planner must emit one deterministic thunk per concrete generic specialization");

    typed::Module invalidBinding = build.module();
    for (typed::Function& function : invalidBinding.functions)
    {
        if (function.nativeBinding)
        {
            function.nativeBinding->thunkSymbol.clear();
            break;
        }
    }
    ok &= expect(!typed::Verifier{}.verify(invalidBinding).succeeded(),
        "Typed verifier must reject native declarations without stable thunk identity");

    lowered::Module invalidInvoke = lowering.module();
    bool damagedInvoke = false;
    for (lowered::Function& function : invalidInvoke.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode == lowered::Opcode::NativeInvoke)
                {
                    instruction.opcode = lowered::Opcode::Call;
                    damagedInvoke = true;
                    break;
                }
            }
            if (damagedInvoke) break;
        }
        if (damagedInvoke) break;
    }
    ok &= expect(damagedInvoke && !lowered::Verifier{}.verify(invalidInvoke).succeeded(),
        "Lowered verifier must reject an ordinary call to a native-bound declaration");

    lowered::Module missingSpecialization = lowering.module();
    bool clearedSpecialization = false;
    for (lowered::Function& function : missingSpecialization.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            for (lowered::Instruction& instruction : block.instructions)
            {
                if (instruction.opcode != lowered::Opcode::NativeInvoke)
                    continue;
                const auto callee = std::ranges::find_if(missingSpecialization.functions,
                    [&](const lowered::Function& candidate) { return candidate.id == instruction.callee; });
                if (callee != missingSpecialization.functions.end() && !callee->genericParameters.empty())
                {
                    instruction.specializationKey.clear();
                    clearedSpecialization = true;
                    break;
                }
            }
            if (clearedSpecialization) break;
        }
        if (clearedSpecialization) break;
    }
    ok &= expect(clearedSpecialization && !NativeAbiPlanner{}.plan(missingSpecialization).succeeded(),
        "Native ABI planner must reject a generic native invocation without concrete specialization identity");

    const WioNativeAbiHandleOps ops{
        .retain = retainHandle,
        .release = releaseHandle,
        .typeId = handleType
    };
    int state = 7;
    WioNativeAbiHandle handle{.state = &state, .ops = &ops, .generation = 2};
    WioNativeAbiHandle alias = handle;
    WioNativeAbiRetain(alias);
    WioNativeAbiRelease(&handle);
    WioNativeAbiRelease(&handle);
    WioNativeAbiRelease(&alias);
    WioNativeAbiRelease(&alias);
    ok &= expect(retains == 1 && releases == 2 && handle.state == nullptr && alias.state == nullptr,
        "Canonical SDK handle contract must retain aliases explicitly and release every ownership claim exactly once");

    return ok ? 0 : 1;
}
