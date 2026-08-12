#include "std_async.h"

#include <cstdint>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using wio::runtime::AsyncTask;

    AsyncTask<int> DelayedValue(const int value, const std::uint64_t milliseconds)
    {
        co_await wio::runtime::AsyncSleep(milliseconds);
        co_return value;
    }

    AsyncTask<int> Observe(const AsyncTask<int> task, const int offset)
    {
        co_return co_await task + offset;
    }

    AsyncTask<int> Immediate(const int value)
    {
        co_return value;
    }

    AsyncTask<void> Throwing()
    {
        co_await wio::runtime::AsyncYield();
        throw std::runtime_error("async-stress-failure");
    }

    AsyncTask<std::size_t> ContinuationThread()
    {
        co_await wio::runtime::AsyncYield();
        co_return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    AsyncTask<void> CancelledBody(std::atomic<bool>& reachedBody)
    {
        co_await wio::runtime::AsyncSleep(5000);
        reachedBody.store(true, std::memory_order_release);
    }

    void Require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        const auto shared = DelayedValue(40, 2);
        std::vector<AsyncTask<int>> observers{
            Observe(shared, 1),
            Observe(shared, 2),
            Observe(shared, 3)
        };
        const auto values = wio::runtime::BlockOn(wio::runtime::WhenAll(std::move(observers)));
        Require(values.size() == 3, "multiple awaiter result count");
        Require(values[0] == 41 && values[1] == 42 && values[2] == 43, "multiple awaiter values");

        for (int iteration = 0; iteration < 2048; ++iteration)
        {
            auto completed = Immediate(iteration);
            Require(wio::runtime::BlockOn(completed) == iteration, "immediate completion value");
        }

        bool observedFailure = false;
        try
        {
            wio::runtime::BlockOn(Throwing());
        }
        catch (const std::runtime_error& error)
        {
            observedFailure = std::string(error.what()) == "async-stress-failure";
        }
        Require(observedFailure, "task failure propagation");

        bool observedTimeout = false;
        try
        {
            wio::runtime::BlockOn(wio::runtime::WithTimeout(DelayedValue(1, 50), 1));
        }
        catch (const wio::runtime::AsyncTimeout&)
        {
            observedTimeout = true;
        }
        Require(observedTimeout, "timeout propagation");

        auto cancelled = DelayedValue(2, 5);
        cancelled.Cancel();
        bool observedCancellation = false;
        try
        {
            wio::runtime::BlockOn(cancelled);
        }
        catch (const wio::runtime::AsyncCancelled&)
        {
            observedCancellation = true;
        }
        Require(observedCancellation, "cancellation propagation");

        const auto cancellationStarted = std::chrono::steady_clock::now();
        auto cancelledTimer = wio::runtime::AsyncSleep(5000);
        cancelledTimer.Cancel();
        try { wio::runtime::BlockOn(cancelledTimer); }
        catch (const wio::runtime::AsyncCancelled&) { }
        const auto cancellationElapsed = std::chrono::steady_clock::now() - cancellationStarted;
        Require(cancellationElapsed < std::chrono::seconds(1), "cancelled timer resumes promptly");

        std::atomic<bool> reachedCancelledBody{false};
        auto cancelledBody = CancelledBody(reachedCancelledBody);
        cancelledBody.Cancel();
        try { wio::runtime::BlockOn(cancelledBody); }
        catch (const wio::runtime::AsyncCancelled&) { }
        Require(!reachedCancelledBody.load(std::memory_order_acquire), "cancelled coroutine does not run past suspension");

        const auto continuationThread = wio::runtime::BlockOn(ContinuationThread());
        const auto blockingThread = wio::runtime::BlockOn(wio::runtime::RunBlockingAsync<std::size_t>([]
        {
            return std::hash<std::thread::id>{}(std::this_thread::get_id());
        }));
        Require(wio::runtime::AsyncBlockingWorkerCount() >= 1, "blocking worker count");
        Require(wio::runtime::AsyncBlockingQueueCapacity() >= 1, "blocking queue capacity");
        Require(continuationThread != blockingThread, "blocking work uses a separate executor");
        for (int iteration = 0; iteration < 512; ++iteration)
        {
            const auto value = wio::runtime::BlockOn(wio::runtime::RunBlockingAsync<int>(
                [iteration] { return iteration; }));
            Require(value == iteration, "immediate blocking completion handoff");
        }

        {
            wio::runtime::AsyncBlockingScheduler bounded(1, 1);
            std::mutex gateMutex;
            std::condition_variable gateChanged;
            bool firstStarted = false;
            bool releaseFirst = false;
            std::atomic<bool> queuedRan{false};
            Require(bounded.Submit([&]
            {
                std::unique_lock lock(gateMutex);
                firstStarted = true;
                gateChanged.notify_all();
                gateChanged.wait(lock, [&] { return releaseFirst; });
            }), "bounded pool accepts active work");
            {
                std::unique_lock lock(gateMutex);
                gateChanged.wait(lock, [&] { return firstStarted; });
            }
            Require(bounded.Submit([&] { queuedRan.store(true, std::memory_order_release); }),
                "bounded pool accepts work up to capacity");
            Require(!bounded.Submit([] {}), "bounded pool rejects excess work");
            {
                std::lock_guard lock(gateMutex);
                releaseFirst = true;
            }
            gateChanged.notify_all();
            bounded.Shutdown();
            Require(queuedRan.load(std::memory_order_acquire), "bounded pool drains accepted work on shutdown");
        }

        Require(wio::runtime::AsyncRuntimeRunning(), "runtime is running before explicit shutdown");
        wio::runtime::ShutdownAsyncRuntime();
        Require(!wio::runtime::AsyncRuntimeRunning(), "explicit shutdown stops both executors");

        bool observedStoppedRuntime = false;
        try { wio::runtime::BlockOn(wio::runtime::AsyncSleep(1)); }
        catch (const wio::runtime::AsyncRuntimeStopped&) { observedStoppedRuntime = true; }
        Require(observedStoppedRuntime, "new work is rejected after shutdown");

        std::cout << "async-runtime-stress-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "async runtime stress failed: " << error.what() << '\n';
        return 1;
    }
}
