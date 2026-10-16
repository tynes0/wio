#include "probe.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(const std::int64_t mode, const std::int64_t count)
{
    std::uint64_t checksum = 0;
    if (mode == 0)
    {
        for (std::int64_t index = 0; index < count; ++index)
        {
            std::ostringstream value;
            value << static_cast<double>(index % 64) * 0.125 - 4.0;
            checksum += format_probe::Hash(value.str());
        }

        return checksum;
    }

    if (mode == 1)
    {
        std::string builder;
        builder.reserve(64);
        for (std::int64_t index = 0; index < count; ++index)
        {
            std::ostringstream value;
            value << "reading=" << static_cast<double>(index % 64) * 0.125 - 4.0 << ";sample=" << index << ";ok=true";
            builder.append(value.str());
            builder.push_back('\n');
        }

        const auto snapshot = builder;
        return format_probe::Hash(snapshot);
    }

    const std::vector<double> numbers{-4.0, -0.125, 0.0, 0.125, 1.5, 7.875, 32.0, 12500000.0};
    for (std::int64_t index = 0; index < count; ++index)
    {
        std::ostringstream value;
        value << '[';
        for (std::size_t item = 0; item < numbers.size(); ++item)
        {
            if (item)
                value << ", ";
            value << numbers[item];
        }

        value << ']';
        checksum += format_probe::Hash(value.str());
    }

    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 2 || inputCount < 1 || inputCount > 100000)
        return 1;
    sink = Work(mode, inputCount);
    sink = Work(mode, inputCount);
    const auto start = std::chrono::steady_clock::now();
    const auto checksum = Work(mode, inputCount);
    sink = checksum;
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << checksum << "\",\"elapsedNanoseconds\":" << elapsed
              << ",\"operations\":" << inputCount << "}\n";
}
