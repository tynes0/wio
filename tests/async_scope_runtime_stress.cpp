#include "std_async.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    using wio::runtime::AsyncTask;

    AsyncTask<int> DelayedValue(const int value, const std::uint64_t milliseconds)
    {
        co_await wio::runtime::AsyncSleep(milliseconds);
        co_return value;
    }

    AsyncTask<void> DelayedVoid(const std::uint64_t milliseconds)
    {
        co_await wio::runtime::AsyncSleep(milliseconds);
    }

    AsyncTask<int> Immediate(const int value)
    {
        co_return value;
    }

    AsyncTask<void> FailingChild()
    {
        co_await wio::runtime::AsyncYield();
        throw std::runtime_error("scoped-child-failure");
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
        auto completed = Immediate(42);
        Require(completed.Get() == 42, "immediate task result");
        completed.Cancel();
        Require(completed.Get() == 42, "late cancellation preserves completion");

        void* joinedHandle = wio::runtime::AsyncScopeCreate();
        auto left = wio::runtime::AsyncScopeSpawn(joinedHandle, DelayedValue(20, 4));
        auto right = wio::runtime::AsyncScopeSpawn(joinedHandle, DelayedValue(22, 2));
        auto side = wio::runtime::AsyncScopeSpawn(joinedHandle, DelayedVoid(3));
        Require(wio::runtime::AsyncScopeCount(joinedHandle) == 3, "heterogeneous child count");
        wio::runtime::AsyncScopeJoin(joinedHandle).Get();
        Require(left.Get() + right.Get() == 42, "joined child values");
        side.Get();
        Require(wio::runtime::AsyncScopeClosed(joinedHandle), "join closes scope");
        Require(wio::runtime::AsyncScopeCount(joinedHandle) == 0, "join releases children");
        wio::runtime::AsyncScopeDestroy(joinedHandle);

        void* failureHandle = wio::runtime::AsyncScopeCreate();
        auto failing = wio::runtime::AsyncScopeSpawn(failureHandle, FailingChild());
        auto sibling = wio::runtime::AsyncScopeSpawn(failureHandle, DelayedValue(1, 5000));
        bool observedFailure = false;
        try
        {
            wio::runtime::AsyncScopeJoin(failureHandle).Get();
        }
        catch (const std::runtime_error& error)
        {
            observedFailure = std::string(error.what()) == "scoped-child-failure";
        }
        Require(observedFailure, "scope propagates first child failure");
        Require(failing.IsFaulted(), "failing child retains fault state");
        Require(sibling.IsCancelled(), "scope cancels siblings after failure");
        wio::runtime::AsyncScopeDestroy(failureHandle);

        void* deadlineHandle = wio::runtime::AsyncScopeCreate();
        auto deadlineChild = wio::runtime::AsyncScopeSpawn(deadlineHandle, DelayedVoid(5000));
        Require(wio::runtime::AsyncScopeDeadline(deadlineHandle, 2), "scope deadline scheduled");
        bool observedDeadlineCancellation = false;
        try
        {
            wio::runtime::AsyncScopeJoin(deadlineHandle).Get();
        }
        catch (const wio::runtime::AsyncCancelled&)
        {
            observedDeadlineCancellation = true;
        }
        Require(observedDeadlineCancellation, "deadline cancels scope join");
        Require(deadlineChild.IsCancelled(), "deadline cancellation reaches child");
        wio::runtime::AsyncScopeDestroy(deadlineHandle);

        void* abandonedHandle = wio::runtime::AsyncScopeCreate();
        auto abandoned = wio::runtime::AsyncScopeSpawn(abandonedHandle, DelayedVoid(5000));
        const auto destroyStarted = std::chrono::steady_clock::now();
        wio::runtime::AsyncScopeDestroy(abandonedHandle);
        Require(abandoned.IsCancelled(), "destroy cancels abandoned child");
        Require(std::chrono::steady_clock::now() - destroyStarted < std::chrono::seconds(1),
            "destroy waits for prompt cooperative cleanup");

        wio::runtime::ShutdownAsyncRuntime();
        std::cout << "async-scope-runtime-stress-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "async scope runtime stress failed: " << error.what() << '\n';
        return 1;
    }
}
