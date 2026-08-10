#include "std_async.h"

#include <cstdint>
#include <iostream>
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

        std::cout << "async-runtime-stress-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "async runtime stress failed: " << error.what() << '\n';
        return 1;
    }
}
