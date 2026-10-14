#include <wio_native_sdk.h>

#include <array>
#include <atomic>
#include <iostream>

extern "C" const WioSdkModuleSessionApi* WioStaticAsyncContractSessionApi() noexcept;
extern "C" const WioSdkModuleTaskApi* WioStaticAsyncContractTaskApi() noexcept;
extern "C" const WioSdkModuleContract* WioStaticAsyncContract() noexcept;

namespace
{
    using Host = wio::sdk::NativeModule;
    using Value = wio::sdk::NativeValue;
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    template <class F> void rejects(F action)
    {
        bool failed = false;
        try
        {
            action();
        }
        catch (const std::exception&)
        {
            failed = true;
        }
        expect(failed, "invalid task operation was accepted");
    }
    void qualify(Host module, Host other)
    {
        std::array args{Value::integer(41)};
        auto pending = module.invoke("Pending", args);
        if (module.taskReady(pending))
        {
            // Preserve the VM fault text instead of misreporting a fault as
            // synchronous completion/blocking of the exported async call.
            (void)module.readTask(pending);
        }
        expect(!module.taskReady(pending), "invoke blocked on a pending day-long task");
        rejects([&] { module.readTask(pending); });
        expect(!module.waitTaskReady(pending, std::chrono::milliseconds{1}) && !module.taskReady(pending),
               "timeout wait blocked or implicitly cancelled its task");
        module.cancelTask(pending);
        for (const auto* name : {"ForeignService", "InvalidService", "InvalidCallback"})
        {
            auto unsupported = module.invoke(name);
            bool rejected = false;
            try
            {
                (void)module.waitTask(unsupported);
            }
            catch (const std::exception& error)
            {
                rejected =
                    std::string_view{error.what()}.find("Native function is not registered") != std::string_view::npos;
            }
            expect(rejected, "foreign symbol or incompatible runtime signature was treated as a VM builtin");
        }
        auto task = module.invoke("Work", args);
        rejects([&] { other.taskReady(task); });
        bool wrongThread = false;
        std::thread foreign{[&]
                            {
                                try
                                {
                                    module.taskReady(task);
                                }
                                catch (const std::exception&)
                                {
                                    wrongThread = true;
                                }
                            }};
        foreign.join();
        expect(wrongThread, "foreign-thread task query bypassed session affinity");
        std::atomic<unsigned> delivered{0};
        module.onTaskComplete(task,
                              [&](std::int32_t status)
                              {
                                  if (status == WIO_NATIVE_ABI_OK)
                                  {
                                      expect(module.taskReady(task) && module.readTask(task).asInteger() == 42,
                                             "owner completion callback could not read its result");
                                      rejects([&] { module.stop(); });
                                      rejects([&] { module.cancelTask(task); });
                                      rejects([&] { module.pumpMain(); });
                                      ++delivered;
                                  }
                              });
        auto clone = task.clone();
        auto moved = std::move(clone);
        expect(module.waitTaskReady(moved, std::chrono::seconds{3}), "task did not make progress");
        expect(module.readTask(task).asInteger() == 42 && delivered == 1, "task read/completion failed");
        module.onTaskComplete(task,
                              [&](std::int32_t)
                              {
                                  ++delivered;
                                  task.reset();
                                  throw std::runtime_error("contained");
                              });
        expect(delivered == 2, "ready-task subscription was not delivered synchronously once");

        auto cancelled = module.invoke("Pending", args);
        unsigned cancelledCount = 0;
        module.onTaskComplete(cancelled,
                              [&](std::int32_t status)
                              {
                                  if (status == WIO_NATIVE_ABI_CANCELLED)
                                      ++cancelledCount;
                              });
        module.cancelTask(cancelled);
        module.cancelTask(cancelled);
        expect(module.taskReady(cancelled) && cancelledCount == 1, "cancel did not settle exactly once");
        rejects([&] { module.readTask(cancelled); });

        auto failure = module.invoke("Fail");
        expect(module.waitTaskReady(failure, std::chrono::seconds{3}), "faulted task did not settle");
        rejects([&] { module.readTask(failure); });
        auto unit = module.invoke("Unit");
        expect(module.waitTask(unit).raw().kind == WIO_NATIVE_ABI_VOID, "void task failed");
        std::array timerArgs{Value::unsignedInteger(1)};
        auto timer = module.invoke("Timer", timerArgs);
        expect(module.waitTask(timer).raw().kind == WIO_NATIVE_ABI_VOID,
               "ordinary function returning a timer task failed");
        std::array unicodeArgs{Value::text(std::u32string{U'A', U'\0', U'\U0001f680'})};
        auto unicode = module.invoke("Unicode", unicodeArgs);
        expect(module.waitTask(unicode).asText() == std::u32string{U'\u27e6', U'A', U'\0', U'\U0001f680', U'\u27e7'},
               "Unicode async payload changed scalars");
        for (std::uint32_t i = 0; i < module.contract()->exportCount; ++i)
        {
            const auto& entry = module.contract()->exports[i];
            if (entry.kind == WIO_SDK_EXPORT_GENERIC_FUNCTION_SPECIALIZATION)
            {
                auto generic = module.invoke(entry.stableId, args);
                expect(module.waitTask(generic).asInteger() == 41, "generic async specialization failed");
            }
        }
        std::array textArgs{Value::string(std::string{"A\0B", 3})};
        auto text = module.invoke("Message", textArgs);
        module.onTaskComplete(text, [&](std::int32_t) { text.reset(); });
        auto answer = module.waitTask(text);
        expect(answer.asString() == std::string{"async:A\0B", 9},
               "reentrant task reset invalidated waiting/result storage");

        auto abandoned = module.invoke("Pending", args);
        unsigned closeCount = 0;
        module.onTaskComplete(abandoned,
                              [&](std::int32_t status)
                              {
                                  if (status == WIO_NATIVE_ABI_CANCELLED)
                                      ++closeCount;
                              });
        auto raw = abandoned.raw();
        const auto* source = static_cast<const WioNativeTaskSource*>(raw.payload.slice.data);
        module.stop();
        expect(closeCount == 1 && answer.asString() == std::string{"async:A\0B", 9},
               "close did not cancel pending task or preserve decoded result");
        bool ready = false;
        expect(source->ready(source->state, &ready) == WIO_NATIVE_ABI_STALE_HANDLE, "closed task retained a live VM");
        rejects([&] { module.readTask(abandoned); });
        module.close();
        other.close();
        std::thread release{[a = std::move(abandoned), b = std::move(answer), c = std::move(moved)]() mutable
                            {
                                a.reset();
                                b.reset();
                                c.reset();
                            }};
        release.join();
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 2, "shared module path required");
        auto path = std::filesystem::u8path(argv[1]);
        qualify(Host::open(path), Host::open(path));
        const auto* sessions = WioStaticAsyncContractSessionApi();
        const auto* tasks = WioStaticAsyncContractTaskApi();
        expect(WioStaticAsyncContract() == nullptr, "legacy default getter admitted an async module");
        expect(WioValidateSdkModuleTaskApi(tasks, sessions), "static task/session sidecar identity invalid");
        auto lifetime = std::make_shared<int>(0);
        auto invalid = *tasks;
        invalid.ready = nullptr;
        rejects([&] { Host::attachSession(*sessions, lifetime, &invalid); });
        invalid = *tasks;
        auto foreignSessions = *sessions;
        invalid.sessions = &foreignSessions;
        rejects([&] { Host::attachSession(*sessions, lifetime, &invalid); });
        qualify(Host::attachSession(*sessions, lifetime, tasks), Host::attachSession(*sessions, lifetime, tasks));
        std::cout << "vm-sdk-async-artifacts-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
