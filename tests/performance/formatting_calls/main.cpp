#include "probe.h"

#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::string Number(const std::uint64_t value)
{
    char buffer[32];
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return {buffer, converted.ptr};
}

struct Counter
{
    std::uint64_t count = 0;

    virtual ~Counter() = default;

    virtual std::string ToString()
    {
        return "item:" + Number(++count);
    }
};

std::uint64_t Work(const std::int64_t mode, const std::int64_t count)
{
    auto counter = std::make_shared<Counter>();
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        // Sequence effectful hooks explicitly; C++ '+' operand evaluation order
        // is not the contract being benchmarked.
        std::string value;
        if (mode == 0)
        {
            auto first = counter->ToString();
            value = "<" + first + ">";
        }
        else
        {
            auto number = Number(static_cast<std::uint64_t>(index));
            auto first = counter->ToString();
            auto second = counter->ToString();
            value = number + ":" + first + "|" + second + ":true";
        }
        checksum += format_probe::Hash(value);
    }
    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 1 || inputCount < 1 || inputCount > 100000)
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
