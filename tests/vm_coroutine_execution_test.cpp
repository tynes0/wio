#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"
#include "wio/vm/machine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t TaskI32 = 2;

    class ExecutorObserver final : public vm::DebugObserver
    {
    public:
        explicit ExecutorObserver(const std::thread::id callingThread) : callingThread_(callingThread)
        {
        }

        vm::DebugAction onInstruction(const vm::DebugEvent& event) override
        {
            if (event.function == 3 && event.block == 0 && event.executor == vm::ExecutorKind::Inherit &&
                std::this_thread::get_id() == callingThread_)
                initialOnCallingThread.store(true, std::memory_order_relaxed);
            if ((event.function == 3 && event.block == 1) || (event.function == 5 && event.block == 1))
            {
                if (event.executor == vm::ExecutorKind::Worker && std::this_thread::get_id() != callingThread_)
                    resumedOnWorker.store(true, std::memory_order_relaxed);
            }
            if (event.function == 5 && event.block == 2 && event.executor == vm::ExecutorKind::Blocking &&
                std::this_thread::get_id() != callingThread_)
                resumedOnBlocking.store(true, std::memory_order_relaxed);
            if (event.function == 5 && event.block == 3 && event.executor == vm::ExecutorKind::Io &&
                std::this_thread::get_id() != callingThread_)
                resumedOnIo.store(true, std::memory_order_relaxed);
            if (event.function == 5 && event.block == 4 && event.executor == vm::ExecutorKind::Main &&
                std::this_thread::get_id() == callingThread_)
                resumedOnMain.store(true, std::memory_order_relaxed);
            if (event.function == 8 && event.block == 1 && event.opcode == bytecode::Opcode::CoroutineSuspend &&
                event.executor == vm::ExecutorKind::Worker)
                waitingOnWorker.store(true, std::memory_order_release);
            return vm::DebugAction::Continue;
        }

        std::atomic_bool initialOnCallingThread = false;
        std::atomic_bool resumedOnWorker = false;
        std::atomic_bool resumedOnBlocking = false;
        std::atomic_bool resumedOnIo = false;
        std::atomic_bool resumedOnMain = false;
        std::atomic_bool waitingOnWorker = false;

    private:
        std::thread::id callingThread_;
    };

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        return result;
    }

    bytecode::Instruction constant(const std::uint32_t result, const bytecode::ConstantId value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant);
        output.result = result;
        output.resultType = I32;
        output.constant = value;
        return output;
    }

    bytecode::Instruction call(const std::uint32_t result, const std::uint32_t callee,
                               std::vector<std::uint32_t> operands = {})
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Call);
        output.result = result;
        output.resultType = TaskI32;
        output.operands = std::move(operands);
        output.callee = callee;
        return output;
    }

    bytecode::Instruction complete(const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::CoroutineComplete);
        output.operands = {value};
        return output;
    }

    bytecode::Instruction returnValue(const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Return);
        output.operands = {value};
        return output;
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t returnType,
                                std::vector<bytecode::Parameter> parameters, std::vector<bytecode::Block> blocks)
    {
        bytecode::Function output;
        output.id = id;
        output.returnType = returnType;
        output.parameters = std::move(parameters);
        output.blocks = std::move(blocks);
        return output;
    }

    bytecode::Block block(const std::uint32_t id, std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Block output;
        output.id = id;
        output.instructions = std::move(instructions);
        return output;
    }

    void addFrameSlot(bytecode::Function& function, const std::uint32_t value, const std::uint32_t type,
                      const std::uint8_t kind = 3)
    {
        function.coroutine.frameSlots.push_back(bytecode::Function::CoroutineFrameSlot{
            .slot = static_cast<std::uint32_t>(function.coroutine.frameSlots.size()),
            .value = value,
            .type = type,
            .kind = kind,
        });
    }

    void makeAsync(bytecode::Function& function)
    {
        function.flags = 0x0081u;
        function.hasCoroutine = true;
        function.coroutine.resultType = I32;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 3},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 9},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 0},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 34, .arguments = {I32}},
        };

        bytecode::Instruction addOne = instruction(bytecode::Opcode::Binary);
        addOne.result = 2;
        addOne.resultType = I32;
        addOne.operands = {0, 1};
        addOne.binaryOperator = 0;
        bytecode::Function immediate = function(0, TaskI32, {bytecode::Parameter{.value = 0, .type = I32}},
                                                {block(0, {constant(1, 1), addOne, complete(2)})});
        makeAsync(immediate);
        addFrameSlot(immediate, 0, I32, 0);
        addFrameSlot(immediate, 1, I32);
        addFrameSlot(immediate, 2, I32);
        module.functions.push_back(std::move(immediate));

        bytecode::Instruction cancellation = instruction(bytecode::Opcode::CancellationCheck);
        cancellation.projectionIndex = 0;
        cancellation.asyncOperation = 1;
        bytecode::Instruction suspend = instruction(bytecode::Opcode::CoroutineSuspend);
        suspend.operands = {1};
        suspend.targets = {bytecode::BranchTarget{.block = 1}};
        suspend.projectionIndex = 0;
        suspend.asyncOperation = 1;
        bytecode::Instruction resume = instruction(bytecode::Opcode::CoroutineResume);
        resume.result = 2;
        resume.resultType = I32;
        resume.projectionIndex = 0;
        resume.asyncOperation = 1;
        bytecode::Instruction addTwo = instruction(bytecode::Opcode::Binary);
        addTwo.result = 4;
        addTwo.resultType = I32;
        addTwo.operands = {2, 3};
        addTwo.binaryOperator = 0;
        bytecode::Instruction releaseChild = instruction(bytecode::Opcode::Release);
        releaseChild.operands = {1};
        bytecode::Function compose = function(1, TaskI32, {},
                                              {block(0, {constant(0, 3), call(1, 0, {0}), cancellation, suspend}),
                                               block(1, {resume, releaseChild, constant(3, 2), addTwo, complete(4)})});
        makeAsync(compose);
        addFrameSlot(compose, 0, I32);
        addFrameSlot(compose, 1, TaskI32, 2);
        addFrameSlot(compose, 2, I32);
        addFrameSlot(compose, 3, I32);
        addFrameSlot(compose, 4, I32);
        compose.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 0,
            .suspendBlock = 0,
            .resumeBlock = 1,
            .awaitedTask = 1,
            .resumedValue = 2,
            .resultType = I32,
            .executor = 0,
            .cancellationPoint = true,
        });
        module.functions.push_back(std::move(compose));

        module.functions.push_back(function(2, TaskI32, {}, {block(0, {call(0, 1), returnValue(0)})}));

        bytecode::Instruction switchCheck = instruction(bytecode::Opcode::CancellationCheck);
        switchCheck.projectionIndex = 0;
        switchCheck.asyncOperation = 2;
        switchCheck.asyncExecutor = 2;
        bytecode::Instruction switchExecutor = instruction(bytecode::Opcode::CoroutineSuspend);
        switchExecutor.targets = {bytecode::BranchTarget{.block = 1}};
        switchExecutor.projectionIndex = 0;
        switchExecutor.asyncOperation = 2;
        switchExecutor.asyncExecutor = 2;
        bytecode::Function switched = function(
            3, TaskI32, {}, {block(0, {switchCheck, switchExecutor}), block(1, {constant(0, 4), complete(0)})});
        makeAsync(switched);
        addFrameSlot(switched, 0, I32);
        switched.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 0,
            .suspendBlock = 0,
            .resumeBlock = 1,
            .resultType = Void,
            .executor = 2,
            .cancellationPoint = true,
        });
        switched.coroutine.maySwitchThreads = true;
        module.functions.push_back(std::move(switched));

        bytecode::Instruction divide = instruction(bytecode::Opcode::Binary);
        divide.result = 2;
        divide.resultType = I32;
        divide.operands = {0, 1};
        divide.binaryOperator = 3;
        bytecode::Function faulted =
            function(4, TaskI32, {}, {block(0, {constant(0, 3), constant(1, 5), divide, complete(2)})});
        makeAsync(faulted);
        addFrameSlot(faulted, 0, I32);
        addFrameSlot(faulted, 1, I32);
        addFrameSlot(faulted, 2, I32);
        module.functions.push_back(std::move(faulted));

        bytecode::Instruction switchWorkerCheck = instruction(bytecode::Opcode::CancellationCheck);
        switchWorkerCheck.projectionIndex = 0;
        switchWorkerCheck.asyncOperation = 2;
        switchWorkerCheck.asyncExecutor = 2;
        bytecode::Instruction switchWorker = instruction(bytecode::Opcode::CoroutineSuspend);
        switchWorker.targets = {bytecode::BranchTarget{.block = 1}};
        switchWorker.projectionIndex = 0;
        switchWorker.asyncOperation = 2;
        switchWorker.asyncExecutor = 2;
        bytecode::Instruction switchBlockingCheck = instruction(bytecode::Opcode::CancellationCheck);
        switchBlockingCheck.projectionIndex = 1;
        switchBlockingCheck.asyncOperation = 2;
        switchBlockingCheck.asyncExecutor = 3;
        bytecode::Instruction switchBlocking = instruction(bytecode::Opcode::CoroutineSuspend);
        switchBlocking.targets = {bytecode::BranchTarget{.block = 2}};
        switchBlocking.projectionIndex = 1;
        switchBlocking.asyncOperation = 2;
        switchBlocking.asyncExecutor = 3;
        bytecode::Instruction switchIoCheck = instruction(bytecode::Opcode::CancellationCheck);
        switchIoCheck.projectionIndex = 2;
        switchIoCheck.asyncOperation = 2;
        switchIoCheck.asyncExecutor = 4;
        bytecode::Instruction switchIo = instruction(bytecode::Opcode::CoroutineSuspend);
        switchIo.targets = {bytecode::BranchTarget{.block = 3}};
        switchIo.projectionIndex = 2;
        switchIo.asyncOperation = 2;
        switchIo.asyncExecutor = 4;
        bytecode::Instruction switchMainCheck = instruction(bytecode::Opcode::CancellationCheck);
        switchMainCheck.projectionIndex = 3;
        switchMainCheck.asyncOperation = 2;
        switchMainCheck.asyncExecutor = 1;
        bytecode::Instruction switchMain = instruction(bytecode::Opcode::CoroutineSuspend);
        switchMain.targets = {bytecode::BranchTarget{.block = 4}};
        switchMain.projectionIndex = 3;
        switchMain.asyncOperation = 2;
        switchMain.asyncExecutor = 1;
        bytecode::Function roundTrip =
            function(5, TaskI32, {},
                     {block(0, {switchWorkerCheck, switchWorker}), block(1, {switchBlockingCheck, switchBlocking}),
                      block(2, {switchIoCheck, switchIo}), block(3, {switchMainCheck, switchMain}),
                      block(4, {constant(0, 4), complete(0)})});
        makeAsync(roundTrip);
        addFrameSlot(roundTrip, 0, I32);
        roundTrip.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 0,
            .suspendBlock = 0,
            .resumeBlock = 1,
            .resultType = Void,
            .executor = 2,
            .cancellationPoint = true,
        });
        roundTrip.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 1,
            .suspendBlock = 1,
            .resumeBlock = 2,
            .resultType = Void,
            .executor = 3,
            .cancellationPoint = true,
        });
        roundTrip.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 2,
            .suspendBlock = 2,
            .resumeBlock = 3,
            .resultType = Void,
            .executor = 4,
            .cancellationPoint = true,
        });
        roundTrip.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 3,
            .suspendBlock = 3,
            .resumeBlock = 4,
            .resultType = Void,
            .executor = 1,
            .cancellationPoint = true,
        });
        roundTrip.coroutine.maySwitchThreads = true;
        module.functions.push_back(std::move(roundTrip));

        bytecode::Instruction awaitSwitchedCheck = instruction(bytecode::Opcode::CancellationCheck);
        awaitSwitchedCheck.projectionIndex = 0;
        awaitSwitchedCheck.asyncOperation = 1;
        bytecode::Instruction awaitSwitched = instruction(bytecode::Opcode::CoroutineSuspend);
        awaitSwitched.operands = {0};
        awaitSwitched.targets = {bytecode::BranchTarget{.block = 1}};
        awaitSwitched.projectionIndex = 0;
        awaitSwitched.asyncOperation = 1;
        bytecode::Instruction resumeSwitched = instruction(bytecode::Opcode::CoroutineResume);
        resumeSwitched.result = 1;
        resumeSwitched.resultType = I32;
        resumeSwitched.projectionIndex = 0;
        resumeSwitched.asyncOperation = 1;
        bytecode::Instruction releaseSwitched = instruction(bytecode::Opcode::Release);
        releaseSwitched.operands = {0};
        bytecode::Function awaitWorker = function(6, TaskI32, {},
                                                  {block(0, {call(0, 3), awaitSwitchedCheck, awaitSwitched}),
                                                   block(1, {resumeSwitched, releaseSwitched, complete(1)})});
        makeAsync(awaitWorker);
        addFrameSlot(awaitWorker, 0, TaskI32, 2);
        addFrameSlot(awaitWorker, 1, I32);
        awaitWorker.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 0,
            .suspendBlock = 0,
            .resumeBlock = 1,
            .awaitedTask = 0,
            .resumedValue = 1,
            .resultType = I32,
            .executor = 0,
            .cancellationPoint = true,
        });
        module.functions.push_back(std::move(awaitWorker));

        bytecode::Instruction faultSwitchCheck = instruction(bytecode::Opcode::CancellationCheck);
        faultSwitchCheck.projectionIndex = 0;
        faultSwitchCheck.asyncOperation = 2;
        faultSwitchCheck.asyncExecutor = 2;
        bytecode::Instruction faultSwitch = instruction(bytecode::Opcode::CoroutineSuspend);
        faultSwitch.targets = {bytecode::BranchTarget{.block = 1}};
        faultSwitch.projectionIndex = 0;
        faultSwitch.asyncOperation = 2;
        faultSwitch.asyncExecutor = 2;
        bytecode::Instruction divideAfterSwitch = instruction(bytecode::Opcode::Binary);
        divideAfterSwitch.result = 2;
        divideAfterSwitch.resultType = I32;
        divideAfterSwitch.operands = {0, 1};
        divideAfterSwitch.binaryOperator = 3;
        bytecode::Function switchedFault =
            function(7, TaskI32, {},
                     {block(0, {faultSwitchCheck, faultSwitch}),
                      block(1, {constant(0, 3), constant(1, 5), divideAfterSwitch, complete(2)})});
        makeAsync(switchedFault);
        addFrameSlot(switchedFault, 0, I32);
        addFrameSlot(switchedFault, 1, I32);
        addFrameSlot(switchedFault, 2, I32);
        switchedFault.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 0,
            .suspendBlock = 0,
            .resumeBlock = 1,
            .resultType = Void,
            .executor = 2,
            .cancellationPoint = true,
        });
        switchedFault.coroutine.maySwitchThreads = true;
        module.functions.push_back(std::move(switchedFault));

        bytecode::Instruction externalSwitchCheck = instruction(bytecode::Opcode::CancellationCheck);
        externalSwitchCheck.projectionIndex = 0;
        externalSwitchCheck.asyncOperation = 2;
        externalSwitchCheck.asyncExecutor = 2;
        bytecode::Instruction externalSwitch = instruction(bytecode::Opcode::CoroutineSuspend);
        externalSwitch.targets = {bytecode::BranchTarget{.block = 1}};
        externalSwitch.projectionIndex = 0;
        externalSwitch.asyncOperation = 2;
        externalSwitch.asyncExecutor = 2;
        bytecode::Instruction externalAwaitCheck = instruction(bytecode::Opcode::CancellationCheck);
        externalAwaitCheck.projectionIndex = 1;
        externalAwaitCheck.asyncOperation = 1;
        bytecode::Instruction externalAwait = instruction(bytecode::Opcode::CoroutineSuspend);
        externalAwait.operands = {0};
        externalAwait.targets = {bytecode::BranchTarget{.block = 2}};
        externalAwait.projectionIndex = 1;
        externalAwait.asyncOperation = 1;
        bytecode::Instruction externalResume = instruction(bytecode::Opcode::CoroutineResume);
        externalResume.result = 1;
        externalResume.resultType = I32;
        externalResume.projectionIndex = 1;
        externalResume.asyncOperation = 1;
        bytecode::Function awaitExternal =
            function(8, TaskI32, {bytecode::Parameter{.value = 0, .type = TaskI32}},
                     {block(0, {externalSwitchCheck, externalSwitch}), block(1, {externalAwaitCheck, externalAwait}),
                      block(2, {externalResume, complete(1)})});
        makeAsync(awaitExternal);
        addFrameSlot(awaitExternal, 0, TaskI32, 2);
        addFrameSlot(awaitExternal, 1, I32);
        awaitExternal.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 0,
            .suspendBlock = 0,
            .resumeBlock = 1,
            .resultType = Void,
            .executor = 2,
            .cancellationPoint = true,
        });
        awaitExternal.coroutine.states.push_back(bytecode::Function::CoroutineState{
            .index = 1,
            .suspendBlock = 1,
            .resumeBlock = 2,
            .awaitedTask = 0,
            .resumedValue = 1,
            .resultType = I32,
            .executor = 0,
            .cancellationPoint = true,
        });
        awaitExternal.coroutine.maySwitchThreads = true;
        module.functions.push_back(std::move(awaitExternal));
        return module;
    }
} // namespace

int main(const int argc, const char* const* argv)
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Coroutine VM fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    ExecutorObserver observer{std::this_thread::get_id()};
    vm::Machine machine{decoded.module, vm::MachineOptions{.debugObserver = &observer, .workerThreadCount = 1}};
    const vm::ExecutionResult started = machine.invoke(2);
    ok &= expect(started.succeeded() && started.value().kind() == vm::Value::Kind::AsyncTask,
                 "A synchronous function must receive an async task handle without replacing its return type");
    ok &= expect(started.value().taskState() == vm::AsyncTaskState::Ready,
                 "Wio async tasks must start eagerly like the native runtime");
    const vm::ExecutionResult composed = machine.wait(started.value());
    ok &= expect(composed.succeeded() && composed.value().asSignedInteger() == 6,
                 "Await suspend/resume must transfer the child task payload into its coroutine frame");

    const vm::ExecutionResult switchedTask = machine.invoke(3);
    const vm::ExecutionResult switched = switchedTask.succeeded() ? machine.wait(switchedTask.value()) : switchedTask;
    ok &= expect(switched.succeeded() && switched.value().asSignedInteger() == 9,
                 "Executor-switch suspension must resume on its canonical continuation block");
    ok &= expect(observer.initialOnCallingThread.load(std::memory_order_relaxed),
                 "An eager task must execute up to its first suspension on the calling thread");
    ok &= expect(observer.resumedOnWorker.load(std::memory_order_relaxed),
                 "A worker switch must resume the preserved VM frame on a worker thread");

    const vm::ExecutionResult roundTripTask = machine.invoke(5);
    const vm::ExecutionResult roundTrip =
        roundTripTask.succeeded() ? machine.wait(roundTripTask.value()) : roundTripTask;
    ok &= expect(roundTrip.succeeded() && roundTrip.value().asSignedInteger() == 9,
                 "A worker-to-main executor round trip must preserve its result");
    ok &= expect(observer.resumedOnMain.load(std::memory_order_relaxed),
                 "Machine::wait must pump main-executor continuations on the bound calling thread");
    ok &= expect(observer.resumedOnBlocking.load(std::memory_order_relaxed),
                 "A blocking switch must resume on the dedicated blocking executor");
    ok &= expect(observer.resumedOnIo.load(std::memory_order_relaxed),
                 "An I/O switch must resume on the dedicated I/O executor");

    const vm::ExecutionResult awaitWorkerTask = machine.invoke(6);
    const vm::ExecutionResult awaitWorker =
        awaitWorkerTask.succeeded() ? machine.wait(awaitWorkerTask.value()) : awaitWorkerTask;
    ok &= expect(awaitWorker.succeeded() && awaitWorker.value().asSignedInteger() == 9,
                 "Awaiting a task that switches executors must not deadlock the serialized VM");

    const vm::ExecutionResult switchedFaultTask = machine.invoke(7);
    const vm::ExecutionResult switchedFault =
        switchedFaultTask.succeeded() ? machine.wait(switchedFaultTask.value()) : switchedFaultTask;
    ok &= expect(!switchedFault.succeeded() && switchedFault.error().code == "WVM1014" &&
                     !switchedFault.error().stack.empty() && switchedFault.error().stack.front().function == 7 &&
                     switchedFault.error().stack.front().block == 1,
                 "A fault after an executor switch must preserve its resumed VM stack trace");

    const vm::Value external = machine.makeExternalTask();
    const std::array<vm::Value, 1> externalArguments = {external};
    const vm::ExecutionResult externalParent = machine.invoke(8, externalArguments);
    const auto workerWaitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (!observer.waitingOnWorker.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < workerWaitDeadline)
        std::this_thread::yield();
    ok &= expect(observer.waitingOnWorker.load(std::memory_order_acquire),
                 "The external await fixture must reach its worker suspension point");
    const vm::ExecutionResult workerProbeTask = machine.invoke(3);
    const vm::ExecutionResult workerProbe =
        workerProbeTask.succeeded() ? machine.wait(workerProbeTask.value()) : workerProbeTask;
    ok &= expect(workerProbe.succeeded() && workerProbe.value().asSignedInteger() == 9,
                 "A pending await must release the only worker instead of blocking its executor thread");
    ok &= expect(machine.complete(external, vm::Value::signedInteger(7)),
                 "External completion must publish the awaited payload exactly once");
    const vm::ExecutionResult externalResult =
        externalParent.succeeded() ? machine.wait(externalParent.value()) : externalParent;
    ok &= expect(externalResult.succeeded() && externalResult.value().asSignedInteger() == 7,
                 "External task completion must wake and resume its suspended VM continuation");

    observer.waitingOnWorker.store(false, std::memory_order_release);
    const vm::Value failedExternal = machine.makeExternalTask();
    const std::array<vm::Value, 1> failedArguments = {failedExternal};
    const vm::ExecutionResult failedParent = machine.invoke(8, failedArguments);
    const auto failedWaitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (!observer.waitingOnWorker.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < failedWaitDeadline)
        std::this_thread::yield();
    ok &= expect(observer.waitingOnWorker.load(std::memory_order_acquire),
                 "The failed external task must reach its worker await point");
    ok &= expect(machine.fail(failedExternal, "EXT001", "external failure"),
                 "External failure must win the awaited task terminal race");
    const vm::ExecutionResult propagatedFailure =
        failedParent.succeeded() ? machine.wait(failedParent.value()) : failedParent;
    ok &= expect(!propagatedFailure.succeeded() && propagatedFailure.error().code == "EXT001" &&
                     !propagatedFailure.error().stack.empty() && propagatedFailure.error().stack.back().function == 8,
                 "Await failure must append the parent coroutine frame to the child diagnostic");

    observer.waitingOnWorker.store(false, std::memory_order_release);
    const vm::Value cancelledExternal = machine.makeExternalTask();
    const std::array<vm::Value, 1> cancelledArguments = {cancelledExternal};
    const vm::ExecutionResult cancelledParent = machine.invoke(8, cancelledArguments);
    const auto cancelledWaitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (!observer.waitingOnWorker.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < cancelledWaitDeadline)
        std::this_thread::yield();
    ok &= expect(observer.waitingOnWorker.load(std::memory_order_acquire),
                 "The cancelled external task must reach its worker await point");
    ok &= expect(cancelledParent.succeeded() && machine.cancel(cancelledParent.value()),
                 "Cancelling a suspended parent must release its continuation ownership");
    const vm::ExecutionResult cancelledAwait = machine.wait(cancelledParent.value());
    ok &= expect(!cancelledAwait.succeeded() && cancelledAwait.error().code == "WVM1165",
                 "A cancelled suspended parent must remain observably cancelled");
    ok &= expect(machine.complete(cancelledExternal, vm::Value::signedInteger(11)),
                 "Completing an orphaned awaited task must remain safe after parent cancellation");

    ExecutorObserver shutdownObserver{std::this_thread::get_id()};
    vm::Value shutdownParent;
    {
        vm::Machine shutdownMachine{decoded.module,
                                    vm::MachineOptions{.debugObserver = &shutdownObserver, .workerThreadCount = 1}};
        const vm::Value neverCompleted = shutdownMachine.makeExternalTask();
        const std::array<vm::Value, 1> shutdownArguments = {neverCompleted};
        const vm::ExecutionResult startedShutdown = shutdownMachine.invoke(8, shutdownArguments);
        if (startedShutdown.succeeded())
            shutdownParent = startedShutdown.value();
        const auto shutdownDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
        while (!shutdownObserver.waitingOnWorker.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < shutdownDeadline)
            std::this_thread::yield();
    }
    ok &= expect(shutdownParent.kind() == vm::Value::Kind::AsyncTask &&
                     shutdownParent.taskState() == vm::AsyncTaskState::Cancelled,
                 "Machine shutdown must cancel suspended continuations instead of leaking running tasks");

    const vm::ExecutionResult faultedTask = machine.invoke(4);
    const vm::ExecutionResult faulted = faultedTask.succeeded() ? machine.wait(faultedTask.value()) : faultedTask;
    ok &= expect(!faulted.succeeded() && faulted.error().code == "WVM1014",
                 "Coroutine execution failures must remain observable through the task handle");

    const vm::Value cancelledTask = vm::Value::asyncTask(3, {});
    ok &= expect(machine.cancel(cancelledTask), "A pending VM task must accept cooperative cancellation");
    const vm::ExecutionResult cancelled = machine.wait(cancelledTask);
    ok &= expect(!cancelled.succeeded() && cancelled.error().code == "WVM1165",
                 "Waiting a cancelled task must produce a stable cancellation diagnostic");

    bytecode::Module malformed = decoded.module;
    malformed.functions[1].coroutine.states[0].resumeBlock = 99;
    const bytecode::VerificationResult malformedVerification = bytecode::Verifier{}.verify(malformed);
    ok &= expect(std::ranges::any_of(malformedVerification.diagnostics(),
                                     [](const auto& diagnostic) { return diagnostic.code == "WBC1033"; }),
                 "The bytecode verifier must reject a coroutine state with an invalid resume block");

    if (argc == 2)
    {
        std::ifstream stream{argv[1], std::ios::binary};
        const std::vector<char> raw{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        std::vector<std::byte> bytes;
        bytes.reserve(raw.size());
        for (const char value : raw)
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        const bytecode::DecodeResult realSource = bytecode::decode(bytes);
        ok &= expect(realSource.succeeded(), "Real-source coroutine bytecode must decode");
        if (realSource.succeeded())
        {
            const auto start = std::ranges::find_if(realSource.module.functions,
                                                    [&](const bytecode::Function& function)
                                                    {
                                                        return function.name < realSource.module.strings.size() &&
                                                               realSource.module.strings[function.name] == "StartTask";
                                                    });
            ok &= expect(start != realSource.module.functions.end(), "Real-source bytecode must contain StartTask");
            if (start != realSource.module.functions.end())
            {
                vm::Machine realMachine{realSource.module};
                const vm::ExecutionResult task = realMachine.invoke(start->id);
                const vm::ExecutionResult result = task.succeeded() ? realMachine.wait(task.value()) : task;
                if (!result.succeeded())
                    std::cerr << result.error().code << ": " << result.error().message << '\n';
                ok &= expect(result.succeeded() && result.value().asSignedInteger() == 6,
                             "Compiler-produced async/await state machines must execute in the VM");
            }
        }
    }
    return ok ? 0 : 1;
}
