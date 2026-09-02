#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace typed = wio::wir::typed;
    namespace lowered = wio::wir::lowered;

    bool ok = true;
    Lexer lexer(
        "[native] fn AsyncYield() -> coroutine<void>; "
        "async fn Produce(value: i32) -> i32 { "
        "  await AsyncYield(); "
        "  return value; "
        "} "
        "async fn Complete() { await AsyncYield(); } "
        "async fn Consume() -> i32 { let value = await Produce(7); return value; } "
        "fn StartTask() -> coroutine<i32> { return Consume(); }",
        "typed_wir_async_lowering_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    typed::BuildResult build = typed::Builder{}.build(program);
    if (!build.succeeded())
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(build.succeeded(), "Async source must build into Typed WIR");

    typed::Module module = build.module();
    bool addedMainSwitch = false;
    for (typed::Function& function : module.functions)
    {
        if (function.name != "Produce") continue;
        for (typed::BasicBlock& block : function.blocks)
        {
            const auto await = std::ranges::find_if(block.instructions, [](const typed::Instruction& instruction)
                { return instruction.opcode == typed::Opcode::Await; });
            if (await != block.instructions.end())
            {
                block.instructions.insert(await + 1, typed::Instruction{
                    .opcode = typed::Opcode::ExecutorSwitch,
                    .asyncOperation = AsyncOperation::SwitchExecutor,
                    .asyncExecutor = AsyncExecutorKind::Main
                });
                addedMainSwitch = true;
                break;
            }
        }
    }
    ok &= expect(addedMainSwitch, "Fixture must contain a place for an explicit main-executor handoff");

    const typed::VerificationResult typedVerification = typed::Verifier{}.verify(module);
    if (!typedVerification.succeeded())
    {
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        std::cerr << typed::Printer{}.print(module);
    }
    ok &= expect(typedVerification.succeeded(), "Async Typed WIR must verify");

    std::size_t asyncFunctions = 0;
    std::size_t awaits = 0;
    std::size_t executorSwitches = 0;
    bool yieldCallTagged = false;
    for (const typed::Function& function : module.functions)
    {
        if (function.isAsync)
        {
            ++asyncFunctions;
            ok &= expect(function.coroutine.has_value(), "Every async function must publish a coroutine layout");
        }
        for (const typed::BasicBlock& block : function.blocks)
        {
            for (const typed::Instruction& instruction : block.instructions)
            {
                awaits += instruction.opcode == typed::Opcode::Await;
                executorSwitches += instruction.opcode == typed::Opcode::ExecutorSwitch;
                yieldCallTagged |= instruction.asyncOperation == AsyncOperation::Yield;
            }
        }
    }
    ok &= expect(asyncFunctions == 3 && awaits == 3 && executorSwitches == 1 && yieldCallTagged,
        "Typed WIR must preserve awaits, void awaits, and await-main distinctly");

    const std::string typedText = typed::Printer{}.print(module);
    ok &= expect(typedText.find("async=await-task") != std::string::npos &&
        typedText.find("executor-switch main") != std::string::npos &&
        typedText.find("coroutine[result=") != std::string::npos,
        "Typed WIR text must expose async operations and coroutine payload contracts");

    LoweringResult lowering = LoweringPipeline{}.lower(module);
    if (!lowering.succeeded())
        for (const auto& diagnostic : lowering.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(lowering.succeeded(), "Async Typed WIR must lower into canonical coroutine states");
    ok &= expect(std::ranges::find(lowering.completedPasses(), "lower-async-state-machines") !=
        lowering.completedPasses().end(), "Async lowering pass must be reported explicitly");

    std::size_t cancellationChecks = 0;
    std::size_t suspends = 0;
    std::size_t resumes = 0;
    std::size_t completes = 0;
    std::size_t plainReturnsInAsync = 0;
    bool mainExecutorState = false;
    bool cleanupAwareTaskSlot = false;
    for (const lowered::Function& function : lowering.module().functions)
    {
        if (function.coroutine)
        {
            for (const CoroutineState& state : function.coroutine->states)
                mainExecutorState |= state.executor == AsyncExecutorKind::Main;
            for (const CoroutineFrameSlot& slot : function.coroutine->frameSlots)
                cleanupAwareTaskSlot |= slot.kind == CoroutineFrameSlotKind::AwaitedTask &&
                    slot.ownership == OwnershipModel::ReferenceCounted &&
                    slot.cleanup == CleanupKind::ReleaseReference;
        }
        for (const lowered::BasicBlock& block : function.blocks)
        {
            for (const lowered::Instruction& instruction : block.instructions)
            {
                cancellationChecks += instruction.opcode == lowered::Opcode::CancellationCheck;
                suspends += instruction.opcode == lowered::Opcode::CoroutineSuspend;
                resumes += instruction.opcode == lowered::Opcode::CoroutineResume;
                completes += instruction.opcode == lowered::Opcode::CoroutineComplete;
                plainReturnsInAsync += function.isAsync && instruction.opcode == lowered::Opcode::Return;
            }
        }
    }
    ok &= expect(cancellationChecks == 4 && suspends == 4 && resumes == 1 && completes == 3 &&
        plainReturnsInAsync == 0 && mainExecutorState && cleanupAwareTaskSlot,
        "Lowering must create cancellation-safe suspend states, async completion, executor affinity, and frame cleanup");
    ok &= expect(lowered::Verifier{}.verify(lowering.module()).succeeded(),
        "Canonical async Lowered WIR must verify");

    const std::string loweredText = lowered::Printer{}.print(lowering.module());
    ok &= expect(loweredText.find("cancellation-check state=") != std::string::npos &&
        loweredText.find("coroutine-suspend state=") != std::string::npos &&
        loweredText.find("coroutine-complete") != std::string::npos &&
        loweredText.find("thread-switch=true") != std::string::npos,
        "Lowered WIR text must make the coroutine state machine observable");

    lowered::Module damaged = lowering.module();
    bool removedCheck = false;
    for (lowered::Function& function : damaged.functions)
    {
        for (lowered::BasicBlock& block : function.blocks)
        {
            const auto check = std::ranges::find_if(block.instructions, [](const lowered::Instruction& instruction)
                { return instruction.opcode == lowered::Opcode::CancellationCheck; });
            if (check != block.instructions.end())
            {
                block.instructions.erase(check);
                removedCheck = true;
                break;
            }
        }
        if (removedCheck) break;
    }
    ok &= expect(removedCheck && !lowered::Verifier{}.verify(damaged).succeeded(),
        "Lowered verifier must reject a suspension point without its cancellation check");

    return ok ? 0 : 1;
}
