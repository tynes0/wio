#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/diagnostic.h"
#include "wio/vm/value.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace wio::vm
{
    enum class ExecutorKind : std::uint8_t
    {
        Inherit,
        Main,
        Worker,
        Blocking,
        Io
    };

    enum class DebugAction : std::uint8_t
    {
        Continue,
        Abort
    };

    struct DebugEvent
    {
        std::uint32_t function = bytecode::InvalidIndex;
        std::uint32_t block = bytecode::InvalidIndex;
        std::uint32_t instruction = bytecode::InvalidIndex;
        bytecode::Opcode opcode = bytecode::Opcode::Unreachable;
        bytecode::SourceSpan source;
        std::uint32_t callDepth = 0;
        ExecutorKind executor = ExecutorKind::Inherit;
    };

    class DebugObserver
    {
    public:
        virtual ~DebugObserver() = default;

        // Called synchronously before an instruction executes. Implementations
        // must not throw or re-enter the machine. The observer must outlive it.
        [[nodiscard]] virtual DebugAction onInstruction(const DebugEvent& event) = 0;
    };

    struct MachineOptions
    {
        std::uint64_t instructionLimit = 10'000'000;
        std::uint32_t callDepthLimit = 1'024;
        std::uint32_t registerLimitPerFrame = 1'000'000;
        DebugObserver* debugObserver = nullptr;
        std::uint32_t workerThreadCount = 0;
        std::uint32_t blockingThreadCount = 1;
        std::uint32_t ioThreadCount = 1;
    };

    class ExecutionResult final
    {
    public:
        [[nodiscard]] static ExecutionResult success(Value value = {});
        [[nodiscard]] static ExecutionResult failure(ExecutionError error);

        [[nodiscard]] bool succeeded() const noexcept
        {
            return succeeded_;
        }
        [[nodiscard]] const Value& value() const noexcept
        {
            return value_;
        }
        [[nodiscard]] const ExecutionError& error() const noexcept
        {
            return error_;
        }

    private:
        [[nodiscard]] static ExecutionResult suspended();
        [[nodiscard]] bool isSuspended() const noexcept
        {
            return suspended_;
        }

        bool succeeded_ = false;
        bool suspended_ = false;
        Value value_;
        ExecutionError error_;

        friend class Machine;
    };

    class Machine final
    {
    public:
        // The module is verified and indexed once, then borrowed immutably for
        // the machine's lifetime.
        explicit Machine(const bytecode::Module& module, MachineOptions options = {});
        ~Machine();

        Machine(Machine&&) noexcept;
        Machine& operator=(Machine&&) noexcept;
        Machine(const Machine&) = delete;
        Machine& operator=(const Machine&) = delete;

        [[nodiscard]] ExecutionResult invoke(std::uint32_t function, std::span<const Value> arguments = {});
        [[nodiscard]] ExecutionResult wait(const Value& task);
        bool cancel(const Value& task);

        // Main-executor continuations run only on the bound thread. wait()
        // pumps this queue automatically when called by that thread.
        void bindMainExecutor();
        [[nodiscard]] std::uint64_t drainMainExecutor();

        // External tasks are the runtime half of the future Sprint 20 native
        // bridge. Completion is exactly-once and may arrive from any thread.
        [[nodiscard]] Value makeExternalTask();
        bool complete(const Value& task, Value result = {});
        bool fail(const Value& task, std::string code, std::string message);

        // Timers share one ordered queue and one worker per machine rather
        // than creating a detached thread for every wait.
        [[nodiscard]] Value sleepFor(std::chrono::nanoseconds duration);

    private:
        struct ExecutionState;

        [[nodiscard]] ExecutionResult execute(std::uint32_t function, std::span<const Value> arguments,
                                              const Value* activeTask,
                                              std::shared_ptr<ExecutionState> resumedState = {});
        [[nodiscard]] bool scheduleTask(const std::shared_ptr<ExecutionState>& state, ExecutorKind executor);
        void wakeTask(std::weak_ptr<ExecutionState> state, AsyncTaskState terminalState, const Value& result,
                      const ExecutionError& error);
        void resumeTask(std::shared_ptr<ExecutionState> state);
        [[nodiscard]] Value startTask(std::uint32_t function, std::span<const Value> arguments);
        [[nodiscard]] ExecutionResult driveTask(const Value& task);

        struct Program;
        std::shared_ptr<Program> program_;
        MachineOptions options_;
    };
} // namespace wio::vm
