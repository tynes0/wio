#include "wio/bytecode/verifier.h"
#include "wio/codegen/wir_cpp_backend.h"
#include "wio/vm/machine.h"

#include <wio_native_abi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t I32 = 1;
    constexpr std::uint32_t TaskI32 = 2;
    constexpr std::uint64_t NativeTaskTypeId = 0x54'41'53'4bull;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bool compileGeneratedCode(const std::string& code)
    {
        namespace fs = std::filesystem;
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path directory = fs::temp_directory_path() / ("wio-vm-native-async-" + std::to_string(nonce));
        std::error_code error;
        fs::create_directories(directory, error);
        if (error)
            return false;
        const fs::path source = directory / "generated.cpp";
        const fs::path object = directory / "generated.o";
        {
            std::ofstream stream{source, std::ios::binary};
            stream << code;
        }
        std::string command = "\"" WIO_TEST_CXX_COMPILER "\" -std=c++20 -c \"" + source.string() +
                              "\" -I\"" WIO_TEST_RUNTIME_INCLUDE "\" -I\"" WIO_TEST_SDK_INCLUDE "\" -o \"" +
                              object.string() + "\"";
#if defined(_WIN32)
        command = "\"" + command + "\"";
#endif
        const int status = std::system(command.c_str());
        fs::remove_all(directory, error);
        return status == 0;
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        return result;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"native:async-i32"};
        module.constants = {bytecode::Constant{}};
        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 5},
            bytecode::Type{.kind = 34, .arguments = {I32}},
        };

        bytecode::Function native;
        native.id = 0;
        native.returnType = TaskI32;
        native.flags = 0x0042u;
        native.hasNativeBinding = true;
        native.nativeBinding.stableKey = 0;
        native.nativeBinding.result = {.type = TaskI32, .passing = 4, .marshalling = 8};
        module.functions.push_back(std::move(native));

        bytecode::Instruction invoke = instruction(bytecode::Opcode::NativeInvoke);
        invoke.callee = 0;
        invoke.result = 0;
        invoke.resultType = TaskI32;
        bytecode::Instruction returnTask = instruction(bytecode::Opcode::Return);
        returnTask.operands = {0};
        bytecode::Function direct;
        direct.id = 1;
        direct.returnType = TaskI32;
        direct.blocks.push_back(bytecode::Block{.id = 0, .instructions = {invoke, returnTask}});
        module.functions.push_back(std::move(direct));

        bytecode::Instruction awaitInvoke = instruction(bytecode::Opcode::NativeInvoke);
        awaitInvoke.callee = 0;
        awaitInvoke.result = 0;
        awaitInvoke.resultType = TaskI32;
        bytecode::Instruction cancellation = instruction(bytecode::Opcode::CancellationCheck);
        cancellation.projectionIndex = 0;
        cancellation.asyncOperation = 1;
        bytecode::Instruction suspend = instruction(bytecode::Opcode::CoroutineSuspend);
        suspend.operands = {0};
        suspend.targets = {bytecode::BranchTarget{.block = 1}};
        suspend.projectionIndex = 0;
        suspend.asyncOperation = 1;
        bytecode::Instruction resume = instruction(bytecode::Opcode::CoroutineResume);
        resume.result = 1;
        resume.resultType = I32;
        resume.projectionIndex = 0;
        resume.asyncOperation = 1;
        bytecode::Instruction release = instruction(bytecode::Opcode::Release);
        release.operands = {0};
        bytecode::Instruction complete = instruction(bytecode::Opcode::CoroutineComplete);
        complete.operands = {1};

        bytecode::Function awaiting;
        awaiting.id = 2;
        awaiting.returnType = TaskI32;
        awaiting.flags = 0x0081u;
        awaiting.hasCoroutine = true;
        awaiting.coroutine.resultType = I32;
        awaiting.coroutine.frameSlots = {
            {.slot = 0, .value = 0, .type = TaskI32, .kind = 2},
            {.slot = 1, .value = 1, .type = I32, .kind = 3},
        };
        awaiting.coroutine.states = {
            {.index = 0,
             .suspendBlock = 0,
             .resumeBlock = 1,
             .awaitedTask = 0,
             .resumedValue = 1,
             .resultType = I32,
             .executor = 0,
             .cancellationPoint = true},
        };
        awaiting.blocks = {
            {.id = 0, .instructions = {awaitInvoke, cancellation, suspend}},
            {.id = 1, .instructions = {resume, release, complete}},
        };
        module.functions.push_back(std::move(awaiting));
        return module;
    }

    struct FakeTask
    {
        std::atomic_uint32_t references{1};
        std::mutex mutex;
        bool terminal = false;
        bool cancelled = false;
        bool faulted = false;
        std::int64_t result = 0;
        std::string failure = "foreign async failure";
        WioNativeTaskCompletionFn callback = nullptr;
        void* userData = nullptr;
        std::thread::id completionThread;

        void finish(const std::int64_t value, const bool fail = false)
        {
            WioNativeTaskCompletionFn completion = nullptr;
            void* data = nullptr;
            {
                std::scoped_lock lock{mutex};
                if (terminal)
                    return;
                terminal = true;
                faulted = fail;
                result = value;
                completionThread = std::this_thread::get_id();
                completion = callback;
                data = userData;
            }
            if (completion)
                completion(data, WIO_NATIVE_ABI_OK);
        }
    };

    std::mutex latestMutex;
    FakeTask* latestTask = nullptr;
    std::atomic_uint32_t bindCalls = 0;
    std::atomic_uint32_t pumpCalls = 0;

    void retainTask(void* const state) noexcept
    {
        static_cast<FakeTask*>(state)->references.fetch_add(1, std::memory_order_relaxed);
    }

    void releaseTask(void* const state) noexcept
    {
        auto* const task = static_cast<FakeTask*>(state);
        if (task->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete task;
    }

    std::uint64_t taskTypeId(const void*) noexcept
    {
        return NativeTaskTypeId;
    }

    constexpr WioNativeAbiHandleOps TaskHandleOps{WIO_NATIVE_ABI_VERSION, 0, &retainTask, &releaseTask, &taskTypeId};

    WioNativeAbiStatus invokeAsync(const WioNativeAbiValue*, const std::uint64_t, WioNativeAbiValue* const result,
                                   WioNativeAbiFailure*) noexcept
    {
        if (!result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* const task = new FakeTask;
        retainTask(task);
        {
            std::scoped_lock lock{latestMutex};
            latestTask = task;
        }
        result->kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->typeId = NativeTaskTypeId;
        result->payload.handle = {task, &TaskHandleOps};
        return WIO_NATIVE_ABI_OK;
    }

    FakeTask* takeLatestTask()
    {
        std::scoped_lock lock{latestMutex};
        FakeTask* const task = latestTask;
        latestTask = nullptr;
        return task;
    }

    WioNativeAbiStatus taskReady(const WioNativeAbiValue* const value, bool* const ready) noexcept
    {
        if (!value || !ready || value->typeId != NativeTaskTypeId)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        std::scoped_lock lock{static_cast<FakeTask*>(value->payload.handle.state)->mutex};
        *ready = static_cast<FakeTask*>(value->payload.handle.state)->terminal;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus cancelTask(const WioNativeAbiValue* const value) noexcept
    {
        if (!value || value->typeId != NativeTaskTypeId)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* const task = static_cast<FakeTask*>(value->payload.handle.state);
        WioNativeTaskCompletionFn completion = nullptr;
        void* data = nullptr;
        {
            std::scoped_lock lock{task->mutex};
            if (!task->terminal)
            {
                task->terminal = true;
                task->cancelled = true;
                completion = task->callback;
                data = task->userData;
            }
        }
        if (completion)
            completion(data, WIO_NATIVE_ABI_CANCELLED);
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus readTask(const WioNativeAbiValue* const value, WioNativeAbiValue* const result,
                                WioNativeAbiFailure* const failure) noexcept
    {
        if (!value || !result || value->typeId != NativeTaskTypeId)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* const task = static_cast<FakeTask*>(value->payload.handle.state);
        std::scoped_lock lock{task->mutex};
        if (!task->terminal)
            return WIO_NATIVE_ABI_NOT_READY;
        if (task->cancelled)
            return WIO_NATIVE_ABI_CANCELLED;
        if (task->faulted)
        {
            if (failure)
            {
                failure->status = WIO_NATIVE_ABI_EXCEPTION;
                failure->message = {task->failure.data(), task->failure.size()};
            }
            return WIO_NATIVE_ABI_EXCEPTION;
        }
        *result = {};
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = task->result;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus onTaskComplete(const WioNativeAbiValue* const value, const WioNativeTaskCompletionFn callback,
                                      void* const userData) noexcept
    {
        if (!value || !callback || value->typeId != NativeTaskTypeId)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* const task = static_cast<FakeTask*>(value->payload.handle.state);
        bool ready = false;
        bool cancelled = false;
        {
            std::scoped_lock lock{task->mutex};
            if (task->callback)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            task->callback = callback;
            task->userData = userData;
            ready = task->terminal;
            cancelled = task->cancelled;
        }
        if (ready)
            callback(userData, cancelled ? WIO_NATIVE_ABI_CANCELLED : WIO_NATIVE_ABI_OK);
        return WIO_NATIVE_ABI_OK;
    }

    void bindMain() noexcept
    {
        bindCalls.fetch_add(1, std::memory_order_relaxed);
    }

    std::uint64_t pumpMain() noexcept
    {
        pumpCalls.fetch_add(1, std::memory_order_relaxed);
        return 1;
    }

    void shutdown() noexcept
    {
    }

    constexpr WioNativeAbiFunctionDescriptor NativeFunction{
        WIO_NATIVE_ABI_VERSION, 0, 1, "native:async-i32", "fixture_async", "fixture_async_thunk", &invokeAsync};
    constexpr WioNativeAbiRegistry Registry{WIO_NATIVE_ABI_VERSION, 1, &NativeFunction};
    constexpr WioNativeTaskApi TaskApi{
        WIO_NATIVE_ABI_VERSION, &taskReady, &cancelTask, &readTask, &bindMain, &pumpMain, &shutdown};
    constexpr WioNativeTaskCompletionApi CompletionApi{WIO_NATIVE_ABI_VERSION, &onTaskComplete};
} // namespace

int main()
{
    bool ok = true;

    wio::wir::lowered::Module emitterModule;
    emitterModule.name = "native-async-sidecar";
    emitterModule.contract.kind = wio::wir::ModuleKind::WioLibrary;
    emitterModule.contract.logicalName = emitterModule.name;
    emitterModule.contract.stableKey = "library:" + emitterModule.name;
    emitterModule.contract.stableId = wio::wir::stableModuleHash(emitterModule.contract.stableKey);
    emitterModule.contract.callTable.stableId = 1;
    const wio::wir::TypeId emitterTask = emitterModule.types.intern(wio::wir::Type{
        .kind = wio::wir::TypeKind::AsyncTask,
        .arguments = {emitterModule.types.i32Type()},
        .ownership = wio::wir::OwnershipModel::ReferenceCounted,
        .cleanup = wio::wir::CleanupKind::ReleaseReference,
    });
    wio::wir::lowered::Function emitterFunction;
    emitterFunction.id = wio::wir::FunctionId{0};
    emitterFunction.name = "ForeignAsync";
    emitterFunction.returnType = emitterTask;
    emitterFunction.callableType = emitterModule.types.intern(wio::wir::Type{
        .kind = wio::wir::TypeKind::Function,
        .arguments = {emitterTask},
        .ownership = wio::wir::OwnershipModel::ReferenceCounted,
        .cleanup = wio::wir::CleanupKind::ReleaseReference,
    });
    emitterFunction.isExternal = true;
    emitterModule.functions.push_back(std::move(emitterFunction));
    const wio::codegen::WirCppGenerationResult emitted = wio::codegen::WirCppBackend{}.generate(
        emitterModule, {.emitMain = false, .emitLineDirectives = false, .verifyInput = false});
    ok &= expect(emitted.succeeded() && emitted.code().find("WioGetNativeTaskCompletionApi") != std::string::npos &&
                     emitted.code().find("WIO_NATIVE_ABI_CANCELLED") != std::string::npos,
                 "Wio libraries must publish the additive native task completion sidecar");
    ok &= expect(emitted.succeeded() && compileGeneratedCode(emitted.code()),
                 "generated native task completion sidecar must compile as standalone C++20");

    const bytecode::Module module = makeModule();
    const bytecode::VerificationResult verification = bytecode::Verifier{}.verify(module);
    ok &= expect(verification.succeeded(), "native async fixture must pass bytecode verification");

    {
        vm::Machine missingBridge{module};
        ok &= expect(missingBridge.registerNativeRegistry(Registry), "plain native registry should register");
        const vm::ExecutionResult result = missingBridge.invoke(1);
        ok &= expect(!result.succeeded() && result.error().code == "WVM1225",
                     "native async invocation without task APIs must fail deterministically");
        if (FakeTask* task = takeLatestTask())
            releaseTask(task);
    }

    vm::Machine machine{module};
    std::string registrationError;
    WioNativeTaskCompletionApi invalidCompletion = CompletionApi;
    invalidCompletion.onComplete = nullptr;
    ok &= expect(!machine.registerNativeRegistry(Registry, TaskApi, invalidCompletion, &registrationError),
                 "incomplete completion API must be rejected");
    ok &= expect(machine.registerNativeRegistry(Registry, TaskApi, CompletionApi, &registrationError),
                 "complete native async bridge must register");
    machine.bindMainExecutor();
    ok &= expect(bindCalls.load(std::memory_order_relaxed) == 1, "VM main binding must reach native task runtime");
    ok &= expect(machine.drainMainExecutor() >= 1 && pumpCalls.load(std::memory_order_relaxed) >= 1,
                 "VM main drain must pump native task runtime");

    const vm::ExecutionResult awaiting = machine.invoke(2);
    ok &= expect(awaiting.succeeded() && awaiting.value().kind() == vm::Value::Kind::AsyncTask,
                 "async Wio caller must return a VM task");
    FakeTask* success = takeLatestTask();
    ok &= expect(success != nullptr, "native thunk must create a foreign task");
    if (!success)
        return 1;
    const std::thread::id callerThread = std::this_thread::get_id();
    std::thread foreign{[success] { success->finish(42); }};
    foreign.join();
    const vm::ExecutionResult completed = machine.wait(awaiting.value());
    ok &= expect(completed.succeeded() && completed.value().asSignedInteger() == 42,
                 "foreign-thread native completion must resume the awaiting Wio coroutine");
    ok &= expect(success->completionThread != callerThread,
                 "native task completion fixture must actually run on a foreign thread");
    releaseTask(success);

    const vm::ExecutionResult cancellable = machine.invoke(1);
    FakeTask* cancelled = takeLatestTask();
    if (!cancelled)
        return 1;
    ok &= expect(cancellable.succeeded() && machine.cancel(cancellable.value()),
                 "VM cancellation must accept an adapted native task");
    {
        std::scoped_lock lock{cancelled->mutex};
        ok &= expect(cancelled->cancelled, "VM cancellation must propagate to the native task API");
    }
    const vm::ExecutionResult cancelledResult = machine.wait(cancellable.value());
    ok &= expect(!cancelledResult.succeeded() && cancelledResult.error().code == "WVM1165",
                 "adapted native cancellation must keep the VM cancellation contract");
    releaseTask(cancelled);

    const vm::ExecutionResult failing = machine.invoke(1);
    FakeTask* failure = takeLatestTask();
    if (!failure)
        return 1;
    std::thread failureThread{[failure] { failure->finish(0, true); }};
    failureThread.join();
    const vm::ExecutionResult failed = machine.wait(failing.value());
    ok &= expect(!failed.succeeded() && failed.error().code == "WVM1226" &&
                     failed.error().message.find("foreign async failure") != std::string::npos,
                 "native async failures must cross the ABI with a stable VM diagnostic");
    releaseTask(failure);

    FakeTask* teardown = nullptr;
    {
        vm::Machine scoped{module};
        ok &= expect(scoped.registerNativeRegistry(Registry, TaskApi, CompletionApi),
                     "teardown fixture bridge must register");
        ok &= expect(scoped.invoke(1).succeeded(), "teardown fixture task must start");
        teardown = takeLatestTask();
    }
    if (!teardown)
        return 1;
    {
        std::scoped_lock lock{teardown->mutex};
        ok &= expect(teardown->cancelled, "machine destruction must cancel every pending native task");
    }
    releaseTask(teardown);

    return ok ? 0 : 1;
}
