#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t TaskI32 = 2;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::SourceSpan source(const std::uint64_t line)
    {
        return {{.file = 0, .line = line, .column = 1}, {.file = 0, .line = line, .column = 10}};
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode, const std::uint64_t line)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        result.source = source(line);
        return result;
    }

    bytecode::Instruction constant(const std::uint32_t result, const bytecode::ConstantId value,
                                   const std::uint64_t line)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant, line);
        output.result = result;
        output.resultType = I32;
        output.constant = value;
        return output;
    }

    bytecode::Instruction call(const std::uint32_t result, const std::uint32_t callee, const std::uint64_t line)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Call, line);
        output.result = result;
        output.resultType = I32;
        output.callee = callee;
        return output;
    }

    bytecode::Instruction finish(const bytecode::Opcode opcode, const std::uint32_t value, const std::uint64_t line)
    {
        bytecode::Instruction output = instruction(opcode, line);
        output.operands = {value};
        return output;
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t name, const std::uint32_t returnType,
                                std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function output;
        output.id = id;
        output.name = name;
        output.returnType = returnType;
        output.source = source(1 + id * 10);
        bytecode::Block block;
        block.id = 0;
        block.source = output.source;
        block.instructions = std::move(instructions);
        output.blocks.push_back(std::move(block));
        return output;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"debug_trace.wio", "Crash", "Entry", "AsyncEntry"};
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 10},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 0},
        };
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 34, .arguments = {I32}},
        };

        bytecode::Instruction divide = instruction(bytecode::Opcode::Binary, 4);
        divide.result = 2;
        divide.resultType = I32;
        divide.operands = {0, 1};
        divide.binaryOperator = 3;
        module.functions.push_back(function(
            0, 1, I32, {constant(0, 1, 2), constant(1, 2, 3), divide, finish(bytecode::Opcode::Return, 2, 5)}));
        module.functions.push_back(function(1, 2, I32, {call(0, 0, 12), finish(bytecode::Opcode::Return, 0, 13)}));

        bytecode::Function asyncEntry =
            function(2, 3, TaskI32, {call(0, 0, 22), finish(bytecode::Opcode::CoroutineComplete, 0, 23)});
        asyncEntry.flags = 0x0081u;
        asyncEntry.hasCoroutine = true;
        asyncEntry.coroutine.resultType = I32;
        asyncEntry.coroutine.frameSlots.push_back({.slot = 0, .value = 0, .type = I32, .kind = 3});
        module.functions.push_back(std::move(asyncEntry));
        return module;
    }

    class Recorder final : public vm::DebugObserver
    {
    public:
        explicit Recorder(const std::size_t abortAt = 0) : abortAt_(abortAt)
        {
        }

        vm::DebugAction onInstruction(const vm::DebugEvent& event) override
        {
            events.push_back(event);
            return abortAt_ != 0 && events.size() == abortAt_ ? vm::DebugAction::Abort : vm::DebugAction::Continue;
        }

        std::vector<vm::DebugEvent> events;

    private:
        std::size_t abortAt_ = 0;
    };

    class ThrowingObserver final : public vm::DebugObserver
    {
    public:
        vm::DebugAction onInstruction(const vm::DebugEvent&) override
        {
            throw 42;
        }
    };
} // namespace

int main()
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Debug diagnostic fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    Recorder recorder;
    vm::Machine machine{decoded.module, vm::MachineOptions{.debugObserver = &recorder}};
    const vm::ExecutionResult failed = machine.invoke(1);
    ok &= expect(!failed.succeeded() && failed.error().code == "WVM1014",
                 "Nested VM execution must preserve the original runtime diagnostic");
    ok &= expect(failed.error().stack.size() == 2 && failed.error().stack[0].function == 0 &&
                     failed.error().stack[0].source.begin.line == 4 && failed.error().stack[1].function == 1 &&
                     failed.error().stack[1].source.begin.line == 12,
                 "Stack traces must run from the failing leaf to its source call site");
    ok &= expect(!recorder.events.empty() && recorder.events.front().function == 1 &&
                     recorder.events.front().opcode == bytecode::Opcode::Call && recorder.events.front().callDepth == 1,
                 "The debug observer must receive typed instruction and call-depth metadata");

    const vm::ExecutionResult asyncTask = machine.invoke(2);
    const vm::ExecutionResult asyncFailure = asyncTask.succeeded() ? machine.wait(asyncTask.value()) : asyncTask;
    ok &= expect(!asyncFailure.succeeded() && asyncFailure.error().code == "WVM1014" &&
                     asyncFailure.error().stack.size() == 2 && asyncFailure.error().stack[1].function == 2,
                 "Faulted async tasks must retain their complete VM stack trace across wait");

    Recorder aborting{1};
    vm::Machine abortedMachine{decoded.module, vm::MachineOptions{.debugObserver = &aborting}};
    const vm::ExecutionResult aborted = abortedMachine.invoke(1);
    ok &= expect(!aborted.succeeded() && aborted.error().code == "WVM1205" && aborted.error().stack.size() == 1,
                 "A debug observer must be able to stop execution at a stable instruction boundary");

    ThrowingObserver throwing;
    vm::Machine guardedMachine{decoded.module, vm::MachineOptions{.debugObserver = &throwing}};
    const vm::ExecutionResult guarded = guardedMachine.invoke(1);
    ok &= expect(!guarded.succeeded() && guarded.error().code == "WVM1206",
                 "Debugger exceptions must never escape across the VM runtime boundary");
    return ok ? 0 : 1;
}
