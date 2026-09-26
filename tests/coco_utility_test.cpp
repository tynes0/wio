#include "coco.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    void Require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::size_t CountOccurrences(const std::string& text, const std::string& value)
    {
        std::size_t count = 0;
        std::size_t offset = 0;
        while ((offset = text.find(value, offset)) != std::string::npos)
        {
            ++count;
            offset += value.size();
        }
        return count;
    }
}

int main()
{
    try
    {
        coco::timer<coco::time_units::microseconds> timer(coco::dont_start{}, "utility-test");
        timer.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        timer.stop();
        Require(timer.get_time() >= 1'000, "timer did not observe elapsed time");
        Require(timer.get_casted_time<coco::time_units::milliseconds>() >= 1,
            "timer duration conversion failed");

        coco::timer_statistics statistics;
        for (const long long value : {1LL, 2LL, 3LL, 4LL})
            statistics.add_measurement(value);
        Require(statistics.get_measurement_count() == 4, "statistics count mismatch");
        Require(statistics.calculate_average() == 2.5, "statistics average mismatch");
        Require(statistics.calculate_median() == 2.5, "statistics median mismatch");
        Require(statistics.calculate_variance() == 1.25, "statistics variance mismatch");
        Require(std::abs(statistics.calculate_standard_deviation() - std::sqrt(1.25)) < 0.000001,
            "statistics standard deviation mismatch");

        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto tracePath = std::filesystem::temp_directory_path() /
            ("wio-coco-trace-" + std::to_string(nonce) + ".json");
        coco::instrumentor::get().begin_session("utility-test", tracePath);

        constexpr std::size_t threadCount = 4;
        constexpr std::size_t eventsPerThread = 64;
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for (std::size_t thread = 0; thread < threadCount; ++thread)
        {
            workers.emplace_back([thread]
            {
                for (std::size_t event = 0; event < eventsPerThread; ++event)
                {
                    coco::instrumentation_timer profile(
                        "worker-\"" + std::to_string(thread) + "-" + std::to_string(event));
                }
            });
        }
        for (auto& worker : workers)
            worker.join();
        coco::instrumentor::get().end_session();

        std::ifstream trace(tracePath, std::ios::binary);
        const std::string contents{
            std::istreambuf_iterator<char>(trace),
            std::istreambuf_iterator<char>()};
        trace.close();
        std::filesystem::remove(tracePath);

        Require(contents.starts_with("{\"displayTimeUnit\":\"ms\""), "trace header mismatch");
        Require(contents.ends_with("]}"), "trace footer mismatch");
        Require(contents.find("worker-\\\"") != std::string::npos, "trace name was not JSON escaped");
        Require(CountOccurrences(contents, "\"ph\":\"X\"") == threadCount * eventsPerThread,
            "concurrent trace event count mismatch");
        return 0;
    }
    catch (...)
    {
        coco::instrumentor::get().end_session();
        return 1;
    }
}
