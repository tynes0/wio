#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

volatile std::uint64_t sink;
volatile std::int64_t inputCount;

std::uint64_t Compute(std::int64_t count)
{
    std::uint64_t state = 12345;
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        state = (state * 48271) % 2147483647;
        checksum += state;
    }
    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    const auto count = std::stoll(argv[1]);
    if (count < 1 || count > 1000000)
        return 1;
    inputCount = count;
    sink = Compute(inputCount);
    sink = Compute(inputCount);
    const auto start = std::chrono::steady_clock::now();
    const auto checksum = Compute(inputCount);
    sink = checksum;
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << checksum << "\",\"elapsedNanoseconds\":" << elapsed
              << ",\"operations\":" << count << "}\n";
    return 0;
}
