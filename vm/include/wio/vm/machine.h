#pragma once

#include "wio/bytecode/module.h"
#include "wio/vm/diagnostic.h"
#include "wio/vm/value.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

struct WioNativeAbiRegistry;
struct WioNativeTaskApi;
struct WioNativeTaskCompletionApi;
struct WioNativePodLayoutApi;
struct WioNativePodDefaultApi;
struct WioNativeSequenceApi;
struct WioNativeDictionaryApi;
struct WioNativeAnyApi;
struct WioNativeObjectApi;

namespace wio::vm
{
    namespace detail
    {
        struct NativePodLayouts;
        struct NativeValueResult;
        struct NativeObjectAccess;
    } // namespace detail
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
        Abort,
        Pause
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

        // A debug observer may pause execution at the instruction boundary it
        // is currently observing. Debug control is thread-safe: an editor or
        // debugger thread can wait for the pause, inspect its stable location,
        // and then resume or abort the serialized execution segment.
        [[nodiscard]] bool isDebugPaused() const;
        [[nodiscard]] bool waitForDebugPause(std::chrono::milliseconds timeout) const;
        [[nodiscard]] std::optional<DebugEvent> pausedDebugEvent() const;
        bool resumeDebugExecution();
        bool abortDebugExecution();

        // Registry descriptors are copied; the registry array may be released
        // after this call. Native thunk code and its containing library must
        // remain loaded while the machine can execute registered functions.
        [[nodiscard]] bool registerNativeRegistry(const WioNativeAbiRegistry& registry, std::string* error = nullptr);
        [[nodiscard]] bool registerNativeRegistry(const WioNativeAbiRegistry& registry, const WioNativeTaskApi& tasks,
                                                  const WioNativeTaskCompletionApi& completions,
                                                  std::string* error = nullptr);
        // Layout and function registration is atomic. Descriptor storage is
        // copied; adapter code must remain loaded just like native thunks.
        [[nodiscard]] bool registerNativeRegistry(const WioNativeAbiRegistry& registry,
                                                  const WioNativePodLayoutApi& pods,
                                                  const WioNativeTaskApi* tasks = nullptr,
                                                  const WioNativeTaskCompletionApi* completions = nullptr,
                                                  std::string* error = nullptr);
        // Additive overload: preserve the original C++ registration symbol.
        [[nodiscard]] bool registerNativeRegistry(const WioNativeAbiRegistry& registry,
                                                  const WioNativePodLayoutApi& pods, const WioNativeTaskApi* tasks,
                                                  const WioNativeTaskCompletionApi* completions, std::string* error,
                                                  const WioNativePodDefaultApi* defaults);
        // Independent optional sequence sidecar, with atomic registration.
        [[nodiscard]] bool registerNativeRegistry(const WioNativeAbiRegistry& registry,
                                                  const WioNativeSequenceApi& sequences, std::string* error = nullptr,
                                                  const WioNativePodLayoutApi* pods = nullptr,
                                                  const WioNativePodDefaultApi* defaults = nullptr,
                                                  const WioNativeTaskApi* tasks = nullptr,
                                                  const WioNativeTaskCompletionApi* completions = nullptr);

        [[nodiscard]] bool registerNativeRegistry(
            const WioNativeAbiRegistry& registry, const WioNativeDictionaryApi& dictionaries,
            std::string* error = nullptr, const WioNativeSequenceApi* sequences = nullptr,
            const WioNativePodLayoutApi* pods = nullptr, const WioNativePodDefaultApi* defaults = nullptr,
            const WioNativeTaskApi* tasks = nullptr, const WioNativeTaskCompletionApi* completions = nullptr);

        [[nodiscard]] bool registerNativeRegistry(
            const WioNativeAbiRegistry& registry, const WioNativeAnyApi& any, std::string* error = nullptr,
            const WioNativeDictionaryApi* dictionaries = nullptr, const WioNativeSequenceApi* sequences = nullptr,
            const WioNativePodLayoutApi* pods = nullptr, const WioNativePodDefaultApi* defaults = nullptr,
            const WioNativeTaskApi* tasks = nullptr, const WioNativeTaskCompletionApi* completions = nullptr);

        [[nodiscard]] bool registerNativeRegistry(
            const WioNativeAbiRegistry& registry, const WioNativeObjectApi& objects, std::string* error = nullptr,
            const WioNativeAnyApi* any = nullptr, const WioNativeDictionaryApi* dictionaries = nullptr,
            const WioNativeSequenceApi* sequences = nullptr, const WioNativePodLayoutApi* pods = nullptr,
            const WioNativePodDefaultApi* defaults = nullptr, const WioNativeTaskApi* tasks = nullptr,
            const WioNativeTaskCompletionApi* completions = nullptr);

        // Loads a generated native thunk module, discovers its registry and
        // optional async sidecars, and pins the dynamic library until this
        // machine has cancelled tasks and released all foreign values.
        [[nodiscard]] bool loadNativeLibrary(const std::filesystem::path& path, std::string* error = nullptr);
        // Host safe point after external producer calls have returned. Releases
        // delayed code leases from proxies/callback results disposed after
        // their VM was closed. Call after producer ABI frames have returned.
        static void collectNativeObjectRetirements();

        // Main-executor continuations run only on the bound thread. wait()
        // pumps this queue automatically when called by that thread.
        void bindMainExecutor();
        [[nodiscard]] std::uint64_t drainMainExecutor();

        // External tasks are shared by embedders and the native async bridge.
        // Completion is exactly-once and may arrive from any thread.
        [[nodiscard]] Value makeExternalTask();
        bool complete(const Value& task, Value result = {});
        bool fail(const Value& task, std::string code, std::string message);

        // Timers share one ordered queue and one worker per machine rather
        // than creating a detached thread for every wait.
        [[nodiscard]] Value sleepFor(std::chrono::nanoseconds duration);

    private:
        struct ExecutionState;

        [[nodiscard]] ExecutionResult invokeWithCaptures(std::uint32_t function, std::span<const Value> arguments,
                                                         const Value* callable);
        [[nodiscard]] ExecutionResult execute(std::uint32_t function, std::span<const Value> arguments,
                                              const Value* activeTask,
                                              std::shared_ptr<ExecutionState> resumedState = {},
                                              const Value* callable = nullptr);
        void cleanupValue(Value& value, ExecutionError& primaryError);
        void cleanupTaskArguments(const Value& task, ExecutionError& primaryError);
        [[nodiscard]] ExecutionError unwind(ExecutionState& state, ExecutionError error);
        void cancelAndUnwindContinuations();
        void cleanupGlobals();
        void cleanupNativeObjects(bool shutdown = false, ExecutionError* primaryError = nullptr) noexcept;
        [[nodiscard]] WioNativeAbiStatus dispatchNativeObjectMethod(const Value&, std::uint64_t,
                                                                    const WioNativeAbiValue*, std::uint64_t,
                                                                    WioNativeAbiValue*, WioNativeAbiFailure*);
        [[nodiscard]] bool encodeNativeObject(const Value&, std::uint32_t, WioNativeAbiValue&, std::string&,
                                              const detail::NativePodLayouts&);
        [[nodiscard]] bool encodeNativeTask(const Value&, std::uint32_t, WioNativeAbiValue&, std::string&,
                                            const detail::NativePodLayouts&);
        [[nodiscard]] detail::NativeValueResult decodeNativeTask(const WioNativeAbiValue&, std::uint32_t);
        [[nodiscard]] detail::NativeValueResult decodeNativeObject(const WioNativeAbiValue&, std::uint32_t,
                                                                   const detail::NativePodLayouts&);
        [[nodiscard]] detail::NativeObjectAccess nativeObjectAccess(bool hostedOnly = false);
        [[nodiscard]] ExecutionResult invokeNativeCallable(const Value&, std::span<const Value>);
        [[nodiscard]] bool encodeNativeCallable(const Value&, std::uint32_t, bool, WioNativeAbiValue&, std::string&,
                                                const detail::NativePodLayouts&);
        [[nodiscard]] bool scheduleTask(const std::shared_ptr<ExecutionState>& state, ExecutorKind executor);
        void wakeTask(std::weak_ptr<ExecutionState> state, AsyncTaskState terminalState, const Value& result,
                      const ExecutionError& error);
        void resumeTask(std::shared_ptr<ExecutionState> state);
        [[nodiscard]] Value startTask(std::uint32_t function, std::span<const Value> arguments);
        [[nodiscard]] ExecutionResult driveTask(const Value& task);
        [[nodiscard]] bool registerNativeRegistryImpl(
            const WioNativeAbiRegistry& registry, const WioNativeTaskApi* tasks,
            const WioNativeTaskCompletionApi* completions, std::string* error,
            const WioNativePodLayoutApi* pods = nullptr, const WioNativePodDefaultApi* defaults = nullptr,
            const WioNativeSequenceApi* sequences = nullptr, const WioNativeDictionaryApi* dictionaries = nullptr,
            const WioNativeAnyApi* any = nullptr, const WioNativeObjectApi* objects = nullptr);

        struct Program;
        std::shared_ptr<Program> program_;
        MachineOptions options_;
    };
} // namespace wio::vm
