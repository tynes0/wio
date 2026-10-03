#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/value.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace wio::vm
{
    struct MachineOptions
    {
        std::uint64_t instructionLimit = 10'000'000;
        std::uint32_t callDepthLimit = 1'024;
        std::uint32_t registerLimitPerFrame = 1'000'000;
    };

    struct ExecutionError
    {
        std::string code;
        std::string message;
        std::uint32_t function = bytecode::InvalidIndex;
        std::uint32_t block = bytecode::InvalidIndex;
        std::uint32_t instruction = bytecode::InvalidIndex;
        bytecode::SourceSpan source;
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
        bool succeeded_ = false;
        Value value_;
        ExecutionError error_;
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

        // External tasks are the runtime half of the future Sprint 20 native
        // bridge. Completion is exactly-once and may arrive from any thread.
        [[nodiscard]] Value makeExternalTask();
        bool complete(const Value& task, Value result = {});
        bool fail(const Value& task, std::string code, std::string message);

        // Timers share one ordered queue and one worker per machine rather
        // than creating a detached thread for every wait.
        [[nodiscard]] Value sleepFor(std::chrono::nanoseconds duration);

    private:
        [[nodiscard]] ExecutionResult execute(std::uint32_t function, std::span<const Value> arguments,
                                              const Value* activeTask);
        [[nodiscard]] Value startTask(std::uint32_t function, std::span<const Value> arguments);
        [[nodiscard]] ExecutionResult driveTask(const Value& task);

        struct Program;
        std::unique_ptr<Program> program_;
        MachineOptions options_;
    };
} // namespace wio::vm
