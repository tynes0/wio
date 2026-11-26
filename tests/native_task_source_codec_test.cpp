#include "wir_task_abi.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    using namespace wio::wir_backend::abi;
    using Task = wio::runtime::AsyncTask<std::int32_t>;
    using Transport = TaskCodec<std::int32_t, 101, 102>;

    void check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Source final
    {
        std::atomic<int> references{1};
        std::atomic<int> deliveries{0};
        std::atomic<int> pumps{0};
        std::atomic<WioNativeAbiStatus> terminal{WIO_NATIVE_ABI_NOT_READY};
        std::mutex mutex;
        std::vector<std::pair<WioNativeTaskCompletionFn, void*>> callbacks;
        bool inlineCompletion = false;
        bool pumpCompletion = false;
        WioNativeTaskSource descriptor;

        Source() : descriptor{WIO_NATIVE_ABI_VERSION, 0, 101, 102, this, &ready, &cancel, &read, &subscribe, &pump}
        {
        }
        static void retain(void* state) noexcept
        {
            ++static_cast<Source*>(state)->references;
        }
        static void release(void* state) noexcept
        {
            --static_cast<Source*>(state)->references;
        }
        static std::uint64_t typeId(const void*) noexcept
        {
            return 101;
        }
        inline static const WioNativeAbiHandleOps ops{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &typeId};

        void complete(WioNativeAbiStatus status)
        {
            std::vector<std::pair<WioNativeTaskCompletionFn, void*>> pending;
            {
                std::scoped_lock lock{mutex};
                if (terminal != WIO_NATIVE_ABI_NOT_READY)
                    return;
                terminal = status;
                pending.swap(callbacks);
            }
            for (const auto& [callback, userdata] : pending)
            {
                ++deliveries;
                callback(userdata, status);
            }
        }
        static WioNativeAbiStatus ready(void* state, bool* result) noexcept
        {
            *result = static_cast<Source*>(state)->terminal != WIO_NATIVE_ABI_NOT_READY;
            return WIO_NATIVE_ABI_OK;
        }
        static WioNativeAbiStatus cancel(void* state) noexcept
        {
            static_cast<Source*>(state)->complete(WIO_NATIVE_ABI_CANCELLED);
            return WIO_NATIVE_ABI_OK;
        }
        static WioNativeAbiStatus read(void* state, WioNativeAbiValue* result, WioNativeAbiFailure* error) noexcept
        {
            const auto status = static_cast<Source*>(state)->terminal.load();
            if (status == WIO_NATIVE_ABI_EXCEPTION)
                return failure(error, status, "mock native fault");
            if (status != WIO_NATIVE_ABI_OK)
                return status;
            *result = Codec<std::int32_t, 102>::write(73);
            return WIO_NATIVE_ABI_OK;
        }
        static WioNativeAbiStatus subscribe(void* state, WioNativeTaskCompletionFn callback, void* userdata) noexcept
        {
            auto& source = *static_cast<Source*>(state);
            if (source.inlineCompletion)
                source.complete(WIO_NATIVE_ABI_OK);
            WioNativeAbiStatus status;
            {
                std::scoped_lock lock{source.mutex};
                status = source.terminal;
                if (status == WIO_NATIVE_ABI_NOT_READY)
                {
                    source.callbacks.emplace_back(callback, userdata);
                    return WIO_NATIVE_ABI_OK;
                }
            }
            ++source.deliveries;
            callback(userdata, status);
            return WIO_NATIVE_ABI_OK;
        }
        static WioNativeAbiStatus pump(void* state, std::uint64_t* count) noexcept
        {
            auto& source = *static_cast<Source*>(state);
            ++source.pumps;
            *count = 0;
            if (source.pumpCompletion)
            {
                source.complete(WIO_NATIVE_ABI_OK);
                *count = 1;
            }
            return WIO_NATIVE_ABI_OK;
        }
        WioNativeAbiValue wire()
        {
            WioNativeAbiValue value{};
            value.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
            value.flags = WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_TASK_SOURCE;
            value.typeId = 101;
            value.payload.slice = {&descriptor, sizeof(descriptor)};
            value.owner = {this, &ops, 1};
            return value;
        }
        void balanced()
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
            while (references != 0 && std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();
            check(references == 0, "task source ownership leaked or was released twice");
            check(callbacks.empty(), "task source subscription was abandoned");
        }
    };

    Task forward(Task task)
    {
        co_return co_await task;
    }

    void readyAndInline()
    {
        for (bool terminal : {true, false})
        {
            Source source;
            if (terminal)
                source.complete(WIO_NATIVE_ABI_OK);
            else
                source.inlineCompletion = true;
            auto wire = source.wire();
            {
                auto task = Transport::read(wire);
                check(task.Get() == 73 && task.SharedState()->Ready(), "inline task completion was lost");
                auto echoed = Transport::write(task);
                check(echoed.payload.slice.data == &source.descriptor &&
                          echoed.flags == (WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_TASK_SOURCE),
                      "identity-preserving task helper produced a new task");
                WioNativeAbiReleaseValue(&echoed);
            }
            WioNativeAbiReleaseValue(&wire);
            source.balanced();
            check(source.deliveries == 1, "terminal source callback was not delivered exactly once");
        }
    }

    void pendingAndBlocking()
    {
        Source source;
        source.pumpCompletion = true;
        auto wire = source.wire();
        {
            Frame boundary;
            auto task = Transport::read(wire);
            check(forward(task).Get() == 73, "composed native blocking wait did not pump its host executor");
            check(source.pumps > 0, "blocking wait bypassed the host main executor");
        }
        WioNativeAbiReleaseValue(&wire);
        source.balanced();

        for (int round = 0; round < 100; ++round)
        {
            Source raced;
            auto input = raced.wire();
            {
                std::thread worker{[&] { raced.complete(WIO_NATIVE_ABI_OK); }};
                auto task = Transport::read(input);
                worker.join();
                check(task.Get() == 73, "foreign-thread completion race lost the result");
            }
            WioNativeAbiReleaseValue(&input);
            raced.balanced();
            check(raced.deliveries == 1, "foreign-thread race resumed a task more than once");
        }
    }

    void cancellationAndFault()
    {
        for (bool alreadyCancelled : {true, false})
        {
            Source source;
            if (alreadyCancelled)
                source.complete(WIO_NATIVE_ABI_CANCELLED);
            auto wire = source.wire();
            {
                auto task = Transport::read(wire);
                if (!alreadyCancelled)
                    task.Cancel();
                bool cancelled = false;
                try
                {
                    (void)task.Get();
                }
                catch (const wio::runtime::AsyncCancelled&)
                {
                    cancelled = true;
                }
                check(cancelled && task.SharedState()->Cancelled(),
                      "host cancellation was turned into an ordinary fault");
                check(source.terminal == WIO_NATIVE_ABI_CANCELLED, "native cancellation did not reach the source");
            }
            WioNativeAbiReleaseValue(&wire);
            source.balanced();
            check(source.deliveries == 1, "cancelled task callback was not delivered exactly once");
        }
        Source source;
        source.complete(WIO_NATIVE_ABI_EXCEPTION);
        auto wire = source.wire();
        {
            auto task = Transport::read(wire);
            bool faulted = false;
            try
            {
                (void)task.Get();
            }
            catch (const std::runtime_error& error)
            {
                faulted = std::string{error.what()} == "mock native fault";
            }
            check(faulted && task.SharedState()->Faulted(), "host task failure message was lost");
        }
        WioNativeAbiReleaseValue(&wire);
        source.balanced();
    }

    void malformed()
    {
        Source source;
        auto wire = source.wire();
        const auto reject = [](const WioNativeAbiValue& value)
        {
            bool rejected = false;
            try
            {
                (void)Transport::read(value);
            }
            catch (const Error&)
            {
                rejected = true;
            }
            check(rejected, "malformed task source was accepted");
        };
        auto forged = wire;
        forged.typeId = 999;
        reject(forged);
        forged = wire;
        forged.flags &= ~WIO_NATIVE_ABI_VALUE_OWNED;
        reject(forged);
        forged = wire;
        forged.flags |= WIO_NATIVE_ABI_VALUE_CONSUMED;
        reject(forged);
        forged = wire;
        --forged.payload.slice.size;
        reject(forged);
        source.descriptor.resultTypeId = 999;
        reject(wire);
        source.descriptor.resultTypeId = 102;
        source.descriptor.abiVersion = 999;
        reject(wire);
        source.descriptor.abiVersion = WIO_NATIVE_ABI_VERSION;
        source.descriptor.pumpMain = nullptr;
        reject(wire);
        WioNativeAbiReleaseValue(&wire);
        source.balanced();
    }
} // namespace

int main()
{
    try
    {
        readyAndInline();
        pendingAndBlocking();
        cancellationAndFault();
        malformed();
        wio::runtime::ShutdownAsyncRuntime();
        std::cout << "native-task-source-codec-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
