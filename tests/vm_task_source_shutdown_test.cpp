#include <wio/vm/machine.h>
#include <wio_module_contract.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
    namespace bc = wio::bytecode;
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    bc::Module fixture()
    {
        bc::Module module;
        module.abiDescriptorVersion = 11;
        module.contract.callTableStableId = 1;
        module.strings = {"worker-main", "void", "i32", "coroutine<i32>", "coroutine<void>"};
        module.types = {bc::Type{.kind = 1, .name = 1}, bc::Type{.kind = 5, .name = 2},
                        bc::Type{.kind = 34, .name = 3, .arguments = {1}},
                        bc::Type{.kind = 34, .name = 4, .arguments = {0}}};
        for (std::uint32_t index = 0; index < 4; ++index)
            module.contract.reflection.push_back(
                bc::Reflection{.stableTypeId = WioStableSdkId(module.strings[index + 1].c_str()),
                               .logicalName = index + 1,
                               .type = index});
        module.constants = {bc::Constant{}, bc::Constant{.kind = bc::ConstantKind::SignedInteger, .bits = 42}};
        bc::Function fn;
        fn.id = fn.name = 0;
        fn.returnType = 2;
        fn.flags = 0x81;
        fn.hasCoroutine = true;
        fn.coroutine.resultType = 1;
        fn.coroutine.maySwitchThreads = true;
        fn.coroutine.frameSlots.push_back({.slot = 0, .value = 0, .type = 1, .kind = 3});
        for (std::uint32_t index = 0; index < 2; ++index)
        {
            const auto executor = static_cast<std::uint8_t>(index == 0 ? 2 : 1);
            bc::Block block;
            block.id = index;
            block.instructions.push_back(bc::Instruction{.opcode = bc::Opcode::CancellationCheck,
                                                         .constant = 0,
                                                         .projectionIndex = index,
                                                         .asyncOperation = 2,
                                                         .asyncExecutor = executor});
            block.instructions.push_back(bc::Instruction{.opcode = bc::Opcode::CoroutineSuspend,
                                                         .targets = {bc::BranchTarget{.block = index + 1}},
                                                         .constant = 0,
                                                         .projectionIndex = index,
                                                         .asyncOperation = 2,
                                                         .asyncExecutor = executor});
            fn.blocks.push_back(std::move(block));
            fn.coroutine.states.push_back({.index = index,
                                           .suspendBlock = index,
                                           .resumeBlock = index + 1,
                                           .resultType = 0,
                                           .executor = executor,
                                           .cancellationPoint = true});
        }
        bc::Block done;
        done.id = 2;
        done.instructions = {
            bc::Instruction{.opcode = bc::Opcode::Constant, .result = 0, .resultType = 1, .constant = 1},
            bc::Instruction{.opcode = bc::Opcode::CoroutineComplete, .operands = {0}, .constant = 0}};
        fn.blocks.push_back(std::move(done));
        module.functions.push_back(std::move(fn));
        return module;
    }
    void completed(void* state, WioNativeAbiStatus status) noexcept
    {
        auto* counters = static_cast<std::atomic<unsigned>*>(state);
        counters[0].fetch_add(1);
        counters[1].store(static_cast<unsigned>(status));
    }

    std::mutex barrierMutex;
    std::condition_variable barrier;
    bool entered = false;
    bool released = false;

    WioNativeAbiStatus blocking(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                WioNativeAbiFailure*) noexcept
    {
        std::unique_lock lock{barrierMutex};
        entered = true;
        barrier.notify_all();
        barrier.wait(lock, [] { return released; });
        *result = {};
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = 7;
        return WIO_NATIVE_ABI_OK;
    }

    void nonblockingTaskSurface()
    {
        auto module = fixture();
        module.strings.push_back("native:blocked-worker");
        bc::Function native;
        native.id = 1;
        native.name = 5;
        native.returnType = 1;
        native.flags = 0x42;
        native.hasNativeBinding = true;
        native.nativeBinding.stableKey = 5;
        native.nativeBinding.result = {.type = 1, .passing = 0, .marshalling = 1};
        module.functions.push_back(std::move(native));
        auto& instructions = module.functions[0].blocks[1].instructions;
        instructions.insert(
            instructions.begin(),
            bc::Instruction{
                .opcode = bc::Opcode::NativeInvoke, .result = 1, .resultType = 1, .callee = 1, .constant = 0});
        wio::vm::Machine machine{module, wio::vm::MachineOptions{.workerThreadCount = 1}};
        WioNativeAbiFunctionDescriptor descriptor{
            WIO_NATIVE_ABI_VERSION, 0, 1, "native:blocked-worker", "blocking", "blocking_thunk", &blocking};
        WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, 1, &descriptor};
        std::string error;
        expect(machine.registerNativeRegistry(registry, &error), error.c_str());
        auto readyTask = machine.makeExternalTask();
        expect(machine.complete(readyTask, wio::vm::Value::signedInteger(42)), "ready task completion failed");
        WioNativeAbiValue readyWire{};
        expect(machine.exportTaskSource(readyTask, 2, readyWire, error), error.c_str());
        auto execution = machine.invoke(0);
        expect(execution.succeeded(), execution.error().message.c_str());
        {
            std::unique_lock lock{barrierMutex};
            expect(barrier.wait_for(lock, std::chrono::seconds{5}, [] { return entered; }),
                   "worker never entered native barrier");
        }
        // A regression cannot hang the test: release a blocked worker after
        // two seconds, then assert that task APIs did not wait for this release.
        std::thread watchdog{[]
                             {
                                 std::unique_lock lock{barrierMutex};
                                 (void)barrier.wait_for(lock, std::chrono::seconds{2}, [] { return released; });
                                 released = true;
                                 barrier.notify_all();
                             }};
        const auto start = std::chrono::steady_clock::now();
        WioNativeAbiValue pendingWire{};
        const bool exported = machine.exportTaskSource(execution.value(), 2, pendingWire, error);
        WioNativeTaskSource source{};
        const bool original = machine.taskSource(pendingWire, source);
        bool ready = true;
        const auto query = original ? source.ready(source.state, &ready) : WIO_NATIVE_ABI_INVALID_ARGUMENT;
        std::atomic<unsigned> events[2]{};
        const auto subscription =
            original ? source.onComplete(source.state, completed, events) : WIO_NATIVE_ABI_INVALID_ARGUMENT;
        const auto* readySource = static_cast<const WioNativeTaskSource*>(readyWire.payload.slice.data);
        WioNativeAbiValue answer{};
        const auto read = readySource->read(readySource->state, &answer, nullptr);
        const auto duration = std::chrono::steady_clock::now() - start;
        {
            std::scoped_lock lock{barrierMutex};
            released = true;
            barrier.notify_all();
        }
        watchdog.join();
        machine.quiesceAsync();
        WioNativeAbiReleaseValue(&pendingWire);
        WioNativeAbiReleaseValue(&readyWire);
        const bool correct = exported && original && query == WIO_NATIVE_ABI_OK && !ready &&
                             subscription == WIO_NATIVE_ABI_OK && read == WIO_NATIVE_ABI_OK &&
                             answer.payload.signedInteger == 42;
        WioNativeAbiReleaseValue(&answer);
        expect(correct && duration < std::chrono::milliseconds{500},
               "task export/query/read/subscription waited behind unrelated worker execution");
        expect(events[0] == 1 && events[1] == WIO_NATIVE_ABI_CANCELLED, "barrier task did not cancel once");
    }
} // namespace

int main()
{
    try
    {
        nonblockingTaskSurface();
        for (unsigned iteration = 0; iteration < 80; ++iteration)
        {
            auto module = std::make_shared<const bc::Module>(fixture());
            std::weak_ptr<const bc::Module> weak = module;
            wio::vm::Machine machine{module, wio::vm::MachineOptions{.workerThreadCount = 1}};
            auto execution = machine.invoke(0);
            expect(execution.succeeded(), "async bytecode did not start");
            WioNativeAbiValue task{};
            std::string error;
            expect(machine.exportTaskSource(execution.value(), 2, task, error), error.c_str());
            WioNativeTaskSource source{};
            expect(machine.taskSource(task, source), "task source ownership check failed");
            {
                wio::vm::Machine other{module};
                WioNativeTaskSource foreign{};
                expect(!other.taskSource(task, foreign), "raw task source crossed VM generations");
                auto forged = task;
                forged.typeId ^= 1;
                expect(!machine.taskSource(forged, foreign), "raw task source accepted a forged type identity");
            }
            bool ready = true;
            expect(source.ready(source.state, &ready) == WIO_NATIVE_ABI_OK && !ready,
                   "main-affined task unexpectedly completed without pumping");
            std::atomic<unsigned> events[2]{};
            expect(source.onComplete(source.state, completed, events) == WIO_NATIVE_ABI_OK, "subscription failed");
            auto timer = machine.sleepFor(std::chrono::hours{24});
            auto extreme = machine.sleepFor(std::chrono::nanoseconds::max());
            expect(extreme.taskState() == wio::vm::AsyncTaskState::Pending,
                   "extreme timer overflowed its deadline into immediate completion");
            WioNativeAbiValue timerWire{};
            expect(machine.exportTaskSource(timer, 3, timerWire, error), "timer source export failed");
            const auto* timerSource = static_cast<const WioNativeTaskSource*>(timerWire.payload.slice.data);
            std::atomic<unsigned> timerEvents[2]{};
            expect(timerSource->onComplete(timerSource->state, completed, timerEvents) == WIO_NATIVE_ABI_OK,
                   "timer subscription failed");
            machine.quiesceAsync();
            machine.shutdown();
            machine.shutdown();
            expect(extreme.taskState() == wio::vm::AsyncTaskState::Cancelled, "extreme timer survived queue shutdown");
            module.reset();
            expect(events[0] == 1 && events[1] == WIO_NATIVE_ABI_CANCELLED && timerEvents[0] == 1 &&
                       timerEvents[1] == WIO_NATIVE_ABI_CANCELLED,
                   "close failed to settle worker/main and timer subscriptions exactly once");
            expect(source.ready(source.state, &ready) == WIO_NATIVE_ABI_STALE_HANDLE && !weak.expired(),
                   "closed source retained a live VM or lost its immutable metadata");
            std::thread releaser{[&]
                                 {
                                     WioNativeAbiReleaseValue(&task);
                                     WioNativeAbiReleaseValue(&timerWire);
                                 }};
            releaser.join();
            expect(weak.expired(), "task-source metadata/thread state leaked after foreign release");
        }
        std::cout << "task-source-shutdown-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
