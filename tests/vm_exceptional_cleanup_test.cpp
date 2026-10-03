#include "wio/vm/machine.h"

#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t RefI32 = 2;
    constexpr std::uint32_t Guard = 3;
    constexpr std::uint32_t RefGuard = 4;
    constexpr std::uint32_t Resource = 5;
    constexpr std::uint32_t RefResource = 6;
    constexpr std::uint32_t FailingResource = 7;
    constexpr std::uint32_t RefFailingResource = 8;
    constexpr std::uint32_t TaskI32 = 9;

    class DestructorObserver final : public vm::DebugObserver
    {
    public:
        vm::DebugAction onInstruction(const vm::DebugEvent& event) override
        {
            if (event.function == 1 && event.instruction == 0)
                ++entries;
            return vm::DebugAction::Continue;
        }

        std::uint32_t entries = 0;
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

    bytecode::Parameter parameter(const std::uint32_t value, const std::uint32_t type)
    {
        return bytecode::Parameter{.value = value, .type = type};
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t returnType,
                                std::vector<bytecode::Parameter> parameters,
                                std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function output;
        output.id = id;
        output.returnType = returnType;
        output.parameters = std::move(parameters);
        output.blocks.push_back(bytecode::Block{.id = 0, .instructions = std::move(instructions)});
        return output;
    }

    bytecode::Function incrementGlobal(const std::uint32_t id, const std::uint32_t receiverType,
                                       const std::uint32_t globalId)
    {
        bytecode::Instruction global = instruction(bytecode::Opcode::GlobalPlace);
        global.result = 1;
        global.resultType = RefI32;
        global.global = globalId;

        bytecode::Instruction load = instruction(bytecode::Opcode::Load);
        load.result = 2;
        load.resultType = I32;
        load.operands = {1};

        bytecode::Instruction add = instruction(bytecode::Opcode::Binary);
        add.result = 4;
        add.resultType = I32;
        add.operands = {2, 3};
        add.binaryOperator = 0;

        bytecode::Instruction store = instruction(bytecode::Opcode::Store);
        store.operands = {1, 4};
        return function(id, Void, {parameter(0, receiverType)},
                        {std::move(global), std::move(load), constant(3, 1), std::move(add), std::move(store),
                         instruction(bytecode::Opcode::Return)});
    }

    bytecode::Function readGlobal(const std::uint32_t id, const std::uint32_t globalId)
    {
        bytecode::Instruction global = instruction(bytecode::Opcode::GlobalPlace);
        global.result = 0;
        global.resultType = RefI32;
        global.global = globalId;

        bytecode::Instruction load = instruction(bytecode::Opcode::Load);
        load.result = 1;
        load.resultType = I32;
        load.operands = {0};

        bytecode::Instruction returned = instruction(bytecode::Opcode::Return);
        returned.operands = {1};
        return function(id, I32, {}, {std::move(global), std::move(load), std::move(returned)});
    }

    bytecode::Instruction construct(const bytecode::Opcode opcode, const std::uint32_t result, const std::uint32_t type)
    {
        bytecode::Instruction output = instruction(opcode);
        output.result = result;
        output.resultType = type;
        return output;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 29, .arguments = {I32}, .flags = 0x01u},
            bytecode::Type{.kind = 28, .nominalKind = 1, .ownership = 1, .cleanup = 1, .destructor = 0},
            bytecode::Type{.kind = 29, .arguments = {Guard}, .flags = 0x01u},
            bytecode::Type{.kind = 28, .nominalKind = 2, .ownership = 2, .cleanup = 2, .destructor = 1},
            bytecode::Type{.kind = 29, .arguments = {Resource}, .flags = 0x01u},
            bytecode::Type{.kind = 28, .nominalKind = 2, .ownership = 2, .cleanup = 2, .destructor = 2},
            bytecode::Type{.kind = 29, .arguments = {FailingResource}, .flags = 0x01u},
            bytecode::Type{.kind = 34, .arguments = {I32}},
        };
        module.globals = {
            bytecode::Global{.id = 0, .type = I32, .flags = 0x01u},
            bytecode::Global{.id = 1, .type = I32, .flags = 0x01u},
            bytecode::Global{.id = 2, .type = Resource, .flags = 0x01u},
        };

        module.functions.push_back(incrementGlobal(0, RefGuard, 0));
        module.functions.push_back(incrementGlobal(1, RefResource, 1));
        module.functions.push_back(
            function(2, Void, {parameter(0, RefFailingResource)}, {instruction(bytecode::Opcode::Unreachable)}));

        bytecode::Instruction dropGuard = instruction(bytecode::Opcode::DropValue);
        dropGuard.operands = {0};
        module.functions.push_back(function(3, Void, {},
                                            {construct(bytecode::Opcode::ConstructComponent, 0, Guard),
                                             std::move(dropGuard), instruction(bytecode::Opcode::Return)}));

        module.functions.push_back(function(
            4, Void, {},
            {construct(bytecode::Opcode::ConstructObject, 0, Resource), instruction(bytecode::Opcode::Unreachable)}));

        module.functions.push_back(function(5, Void, {},
                                            {construct(bytecode::Opcode::ConstructObject, 0, Resource),
                                             construct(bytecode::Opcode::ConstructObject, 1, FailingResource),
                                             instruction(bytecode::Opcode::Unreachable)}));
        module.functions.push_back(readGlobal(6, 0));
        module.functions.push_back(readGlobal(7, 1));

        bytecode::Instruction cancellation = instruction(bytecode::Opcode::CancellationCheck);
        cancellation.projectionIndex = 0;
        cancellation.asyncOperation = 1;
        bytecode::Instruction suspend = instruction(bytecode::Opcode::CoroutineSuspend);
        suspend.operands = {0};
        suspend.targets = {bytecode::BranchTarget{.block = 1}};
        suspend.projectionIndex = 0;
        suspend.asyncOperation = 1;
        bytecode::Instruction resume = instruction(bytecode::Opcode::CoroutineResume);
        resume.result = 2;
        resume.resultType = I32;
        resume.projectionIndex = 0;
        resume.asyncOperation = 1;
        bytecode::Instruction complete = instruction(bytecode::Opcode::CoroutineComplete);
        complete.operands = {2};
        bytecode::Function suspended;
        suspended.id = 8;
        suspended.returnType = TaskI32;
        suspended.parameters = {parameter(0, TaskI32)};
        suspended.blocks = {
            bytecode::Block{.id = 0,
                            .instructions = {construct(bytecode::Opcode::ConstructObject, 1, Resource),
                                             std::move(cancellation), std::move(suspend)}},
            bytecode::Block{.id = 1, .instructions = {std::move(resume), std::move(complete)}},
        };
        suspended.flags = 0x0081u;
        suspended.hasCoroutine = true;
        suspended.coroutine.resultType = I32;
        suspended.coroutine.frameSlots = {
            bytecode::Function::CoroutineFrameSlot{.slot = 0, .value = 0, .type = TaskI32, .kind = 0},
            bytecode::Function::CoroutineFrameSlot{
                .slot = 1, .value = 1, .type = Resource, .kind = 3, .ownership = 2, .cleanup = 2},
            bytecode::Function::CoroutineFrameSlot{.slot = 2, .value = 2, .type = I32, .kind = 3},
        };
        suspended.coroutine.states = {
            bytecode::Function::CoroutineState{.index = 0,
                                               .suspendBlock = 0,
                                               .resumeBlock = 1,
                                               .awaitedTask = 0,
                                               .resumedValue = 2,
                                               .resultType = I32,
                                               .executor = 0,
                                               .cancellationPoint = true},
        };
        module.functions.push_back(std::move(suspended));

        bytecode::Instruction capturedCancellation = instruction(bytecode::Opcode::CancellationCheck);
        capturedCancellation.projectionIndex = 0;
        capturedCancellation.asyncOperation = 1;
        bytecode::Instruction capturedSuspend = instruction(bytecode::Opcode::CoroutineSuspend);
        capturedSuspend.operands = {1};
        capturedSuspend.targets = {bytecode::BranchTarget{.block = 1}};
        capturedSuspend.projectionIndex = 0;
        capturedSuspend.asyncOperation = 1;
        bytecode::Instruction capturedResume = instruction(bytecode::Opcode::CoroutineResume);
        capturedResume.result = 2;
        capturedResume.resultType = I32;
        capturedResume.projectionIndex = 0;
        capturedResume.asyncOperation = 1;
        bytecode::Instruction capturedComplete = instruction(bytecode::Opcode::CoroutineComplete);
        capturedComplete.operands = {2};
        bytecode::Function captured;
        captured.id = 9;
        captured.returnType = TaskI32;
        captured.parameters = {parameter(0, Resource), parameter(1, TaskI32)};
        captured.blocks = {
            bytecode::Block{.id = 0, .instructions = {std::move(capturedCancellation), std::move(capturedSuspend)}},
            bytecode::Block{.id = 1, .instructions = {std::move(capturedResume), std::move(capturedComplete)}},
        };
        captured.flags = 0x0081u;
        captured.hasCoroutine = true;
        captured.coroutine.resultType = I32;
        captured.coroutine.frameSlots = {
            bytecode::Function::CoroutineFrameSlot{
                .slot = 0, .value = 0, .type = Resource, .kind = 0, .ownership = 2, .cleanup = 2},
            bytecode::Function::CoroutineFrameSlot{.slot = 1, .value = 1, .type = TaskI32, .kind = 0},
            bytecode::Function::CoroutineFrameSlot{.slot = 2, .value = 2, .type = I32, .kind = 3},
        };
        captured.coroutine.states = {
            bytecode::Function::CoroutineState{.index = 0,
                                               .suspendBlock = 0,
                                               .resumeBlock = 1,
                                               .awaitedTask = 1,
                                               .resumedValue = 2,
                                               .resultType = I32,
                                               .executor = 0,
                                               .cancellationPoint = true},
        };
        module.functions.push_back(std::move(captured));

        bytecode::Instruction startCaptured = instruction(bytecode::Opcode::Call);
        startCaptured.result = 2;
        startCaptured.resultType = TaskI32;
        startCaptured.operands = {1, 0};
        startCaptured.callee = 9;
        bytecode::Instruction releaseCaptured = instruction(bytecode::Opcode::Release);
        releaseCaptured.operands = {1};
        bytecode::Instruction returnCaptured = instruction(bytecode::Opcode::Return);
        returnCaptured.operands = {2};
        module.functions.push_back(
            function(10, TaskI32, {parameter(0, TaskI32)},
                     {construct(bytecode::Opcode::ConstructObject, 1, Resource), std::move(startCaptured),
                      std::move(releaseCaptured), std::move(returnCaptured)}));

        bytecode::Instruction globalResource = instruction(bytecode::Opcode::GlobalPlace);
        globalResource.result = 1;
        globalResource.resultType = RefResource;
        globalResource.global = 2;
        bytecode::Instruction storeResource = instruction(bytecode::Opcode::Store);
        storeResource.operands = {1, 0};
        bytecode::Instruction releaseResource = instruction(bytecode::Opcode::Release);
        releaseResource.operands = {0};
        module.functions.push_back(
            function(11, Void, {},
                     {construct(bytecode::Opcode::ConstructObject, 0, Resource), std::move(globalResource),
                      std::move(storeResource), std::move(releaseResource), instruction(bytecode::Opcode::Return)}));
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    const bytecode::Module module = makeModule();
    vm::Machine machine{module};

    const vm::ExecutionResult dropped = machine.invoke(3);
    const vm::ExecutionResult componentCount = machine.invoke(6);
    ok &= expect(dropped.succeeded() && componentCount.succeeded() && componentCount.value().asSignedInteger() == 1,
                 "DropValue must execute a component destructor exactly once");

    const vm::ExecutionResult fault = machine.invoke(4);
    const vm::ExecutionResult objectCount = machine.invoke(7);
    ok &= expect(!fault.succeeded() && fault.error().code == "WVM1035" && objectCount.succeeded() &&
                     objectCount.value().asSignedInteger() == 1,
                 "Exceptional frame unwind must execute the final object release");

    const vm::ExecutionResult cleanupFault = machine.invoke(5);
    const vm::ExecutionResult finalObjectCount = machine.invoke(7);
    ok &= expect(!cleanupFault.succeeded() && cleanupFault.error().code == "WVM1035" &&
                     cleanupFault.error().cleanupFailures.size() == 1 &&
                     cleanupFault.error().cleanupFailures.front().code == "WVM1035",
                 "A destructor failure must be attached without replacing the primary execution error");
    ok &= expect(finalObjectCount.succeeded() && finalObjectCount.value().asSignedInteger() == 2,
                 "Unwind must continue cleaning older values after a destructor failure");

    const vm::Value external = machine.makeExternalTask();
    const vm::ExecutionResult suspended = machine.invoke(8, std::span{&external, 1});
    ok &= expect(suspended.succeeded() && suspended.value().taskState() == vm::AsyncTaskState::Running,
                 "Cleanup fixture coroutine must remain suspended on its external task");
    ok &= expect(suspended.succeeded() && machine.cancel(suspended.value()),
                 "A suspended coroutine must accept cancellation");
    const vm::ExecutionResult cancelledObjectCount = machine.invoke(7);
    ok &= expect(cancelledObjectCount.succeeded() && cancelledObjectCount.value().asSignedInteger() == 3,
                 "Cancelling a suspended coroutine must unwind its live object exactly once");

    const vm::Value capturedExternal = machine.makeExternalTask();
    const vm::ExecutionResult capturedTask = machine.invoke(10, std::span{&capturedExternal, 1});
    ok &= expect(capturedTask.succeeded() && machine.cancel(capturedTask.value()),
                 "A suspended coroutine with an owned argument must accept cancellation");
    const vm::ExecutionResult capturedObjectCount = machine.invoke(7);
    ok &= expect(capturedObjectCount.succeeded() && capturedObjectCount.value().asSignedInteger() == 4,
                 "Terminal task cleanup must release captured arguments after its frame unwinds");

    DestructorObserver shutdownObserver;
    {
        vm::Machine shutdownMachine{module, vm::MachineOptions{.debugObserver = &shutdownObserver}};
        ok &= expect(shutdownMachine.invoke(11).succeeded(),
                     "Shutdown cleanup fixture must store its final object in module-global storage");
        ok &= expect(shutdownObserver.entries == 0, "A live module global must retain its object until shutdown");
    }
    ok &= expect(shutdownObserver.entries == 1,
                 "Machine shutdown must run the final module-global destructor exactly once");
    return ok ? 0 : 1;
}
