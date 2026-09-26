#include "std_async.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    wio::runtime::AsyncTask<bool> HandoffToMain()
    {
        const bool ranOnWorker = co_await wio::runtime::RunAsync<bool>([]
        {
            return !wio::runtime::IsAsyncMainThread();
        });
        if (!ranOnWorker)
            throw std::runtime_error("worker action ran on the main executor");

        co_await wio::runtime::AsyncMainAwaiter{};
        if (!wio::runtime::IsAsyncMainThread())
            throw std::runtime_error("main handoff resumed on the wrong thread");
        co_return true;
    }

    void WaitOnMain(const wio::runtime::AsyncTask<bool>& task)
    {
        while (!task.IsReady())
        {
            wio::runtime::DrainAsyncMainExecutor();
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
        for (std::uint64_t iteration = 0; iteration < 4096; ++iteration)
        {
            auto task = HandoffToMain();
            WaitOnMain(task);
        }
        wio::runtime::DrainAsyncMainExecutor();
        if (wio::runtime::AsyncMainPendingCount() != 0)
            throw std::runtime_error("main executor retained completed work");

        wio::runtime::ShutdownAsyncRuntime();
        std::cout << "async-main-runtime-stress-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "async main runtime stress failed: " << error.what() << '\n';
        return 1;
    }
}
