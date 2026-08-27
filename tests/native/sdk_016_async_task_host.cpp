#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "wio_sdk.h"

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "SDK async host expected one module path.\n";
        return 2;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        const auto info = module.inspect();
        if (!module.supports_async() ||
            !info.has_capability(WIO_MODULE_CAP_ASYNC_TASK_HOST_V1) ||
            info.async_exports.size() != 3u)
            throw std::runtime_error("async task host metadata is incomplete");

        module.bind_async_main();
        auto delayedDouble = module.load_async<std::int32_t(std::int32_t, wio::sdk::WioU64)>("DelayedDouble");
        auto delayedUnit = module.load_async<void(wio::sdk::WioU64)>("DelayedUnit");
        auto cancellableValue = module.load_async<std::int32_t(wio::sdk::WioU64)>("CancellableValue");

        auto valueTask = delayedDouble(21, wio::sdk::WioU64(40u));
        auto retainedTask = valueTask;
        std::atomic<bool> workerCompletion{false};
        std::atomic<bool> mainCompletion{false};
        valueTask.on_complete([&](const wio::sdk::AsyncTaskState state)
        {
            workerCompletion.store(state == wio::sdk::AsyncTaskState::Ready);
        });
        valueTask.on_complete([&](const wio::sdk::AsyncTaskState state)
        {
            mainCompletion.store(state == wio::sdk::AsyncTaskState::Ready);
        }, wio::sdk::AsyncCompletionTarget::MainExecutor);

        if (valueTask.wait_for(1ms))
            throw std::runtime_error("deadline wait did not report its timeout");
        if (!valueTask.wait_for(2s) || valueTask.get() != 42 || retainedTask.get() != 42)
            throw std::runtime_error("typed async result contract failed");

        // Task readiness is published before completion callbacks are dispatched.
        // Give both callback registrations a bounded opportunity to publish while
        // continuously proving that main-executor delivery remains pump-driven.
        const auto callbackDeadline = std::chrono::steady_clock::now() + 2s;
        while ((!workerCompletion.load() || module.pending_async_main() == 0u) &&
               std::chrono::steady_clock::now() < callbackDeadline)
        {
            if (mainCompletion.load())
                throw std::runtime_error("main completion callback ran without an explicit pump");
            std::this_thread::yield();
        }
        if (!workerCompletion.load())
            throw std::runtime_error("current-executor completion callback was not delivered");
        if (mainCompletion.load() || module.pending_async_main() == 0u)
            throw std::runtime_error("main completion callback did not remain explicitly pump-driven");
        if (module.pump_async_main() == 0u || !mainCompletion.load())
            throw std::runtime_error("main completion callback pump failed");

        auto unitTask = delayedUnit(wio::sdk::WioU64(2u));
        if (!unitTask.wait_for(2s))
            throw std::runtime_error("void async task did not complete");
        unitTask.get();

        auto cancelledTask = cancellableValue(wio::sdk::WioU64(5000u));
        cancelledTask.cancel();
        if (!cancelledTask.wait_for(2s) || !cancelledTask.cancelled())
            throw std::runtime_error("async cancellation was not observable");
        bool cancelledGetRejected = false;
        try { (void)cancelledTask.get(); }
        catch (const wio::sdk::Error&) { cancelledGetRejected = true; }
        if (!cancelledGetRejected)
            throw std::runtime_error("cancelled async result remained readable");

        auto staleTask = delayedDouble(1, wio::sdk::WioU64(1u));
        if (!staleTask.wait_for(2s))
            throw std::runtime_error("stale-binding probe task did not settle");
        module.request_async_shutdown();
        module.close();
        bool staleRejected = false;
        try { (void)staleTask.status(); }
        catch (const wio::sdk::Error& error)
        {
            staleRejected = error.code() == wio::sdk::ErrorCode::StaleBinding;
        }
        if (!staleRejected)
            throw std::runtime_error("async task remained callable after module close");

        std::cout << "sdk-async-task-ok value=42 timeout=1 cancel=1 callbacks=2 stale=1\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
