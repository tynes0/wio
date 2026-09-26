#include "std_async.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    wio::runtime::AsyncTask<bool> HandoffToMain()
    {
        const auto child = wio::runtime::RunAsync<bool>([]
        {
            return !wio::runtime::IsAsyncMainThread();
        });
        const bool ranOnWorker = co_await child;
        if (!ranOnWorker)
            throw std::runtime_error("worker action ran on the main executor");

        co_await wio::runtime::AsyncMainAwaiter{};
        if (!wio::runtime::IsAsyncMainThread())
            throw std::runtime_error("main handoff resumed on the wrong thread");
        co_return true;
    }

    wio::runtime::AsyncTask<void> DetachedWorker(std::atomic<std::uint64_t>& completed)
    {
        co_await wio::runtime::AsyncYield();
        completed.fetch_add(1, std::memory_order_release);
    }

    wio::runtime::AsyncTask<void> ImmediateVoid()
    {
        co_return;
    }

    void WaitOnMain(const wio::runtime::AsyncTask<bool>& task)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!task.IsReady())
        {
            wio::runtime::DrainAsyncMainExecutor();
            if (std::chrono::steady_clock::now() >= deadline)
            {
                throw std::runtime_error(
                    "main handoff timed out (worker pending=" +
                    std::to_string(wio::runtime::DefaultAsyncScheduler().PendingCount()) +
                    ", main pending=" +
                    std::to_string(wio::runtime::AsyncMainPendingCount()) + ")");
            }
            std::this_thread::yield();
        }
        if (!task.Get())
            throw std::runtime_error("main handoff returned false");
    }
}

int main()
{
    try
    {
        wio::runtime::BindAsyncMainExecutor();
        wio::runtime::AsyncTask<bool> retainedTask;
        for (std::uint64_t iteration = 0; iteration < 4096; ++iteration)
        {
            retainedTask = HandoffToMain();
            WaitOnMain(retainedTask);
        }
        wio::runtime::DrainAsyncMainExecutor();
        if (wio::runtime::AsyncMainPendingCount() != 0)
            throw std::runtime_error("main executor retained completed work");

        // Dropping the public task handle while a coroutine is suspended must
        // not release its frame until the scheduler's resume call returns.
        std::atomic<std::uint64_t> detachedCompleted{0};
        for (std::uint64_t iteration = 0; iteration < 1024; ++iteration)
            (void)DetachedWorker(detachedCompleted);
        const auto detachedDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (detachedCompleted.load(std::memory_order_acquire) != 1024)
        {
            if (std::chrono::steady_clock::now() >= detachedDeadline)
                throw std::runtime_error("detached worker frame did not complete");
            std::this_thread::yield();
        }

        // A coroutine that completes before its call returns keeps the final
        // frame until the returned task object itself is released.
        for (std::uint64_t iteration = 0; iteration < 4096; ++iteration)
            (void)ImmediateVoid();

        // Application state retains its final completed task until after the
        // runtime is shut down. Exercise that destruction order explicitly.
        wio::runtime::ShutdownAsyncRuntime();
        retainedTask = {};
        std::cout << "async-main-runtime-stress-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "async main runtime stress failed: " << error.what() << '\n';
        return 1;
    }
}
