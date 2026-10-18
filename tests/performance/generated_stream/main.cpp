#include "probe.h"

#include <chrono>
#include <iostream>
#include <sstream>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(const std::int64_t mode, const std::int64_t count)
{
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        std::ostringstream stream;
        if (mode == 1)
            stream << std::string_view{"prefix\0", 7};
        stream << generated_stream::Label{mode == 0 ? 192 : 8192, index};
        if (mode == 1)
            stream << "suffix";
        checksum += generated_stream::Hash(stream.str());
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
