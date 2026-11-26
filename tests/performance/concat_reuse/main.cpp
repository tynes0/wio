#include <chrono>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>

volatile std::uint64_t sink;
volatile std::int64_t inputCount;

std::uint64_t Hash(const std::string& value)
{
    std::atomic_signal_fence(std::memory_order_seq_cst);
    std::uint64_t result = 14695981039346656037ULL;
    for (const unsigned char byte : value)
    {
        result = (result ^ byte) * 1099511628211ULL;
    }

    return result;
}

std::uint64_t Work(const std::int64_t mode, const std::int64_t count)
{
    const std::string a("copy\0payload:0123456789abcdef0123456789abcdef", 45);
    const std::string b = "second:0123456789abcdef0123456789abcdef";
    const std::string c = "third:0123456789abcdef0123456789abcdef";
    if (mode < 2)
    {
        const std::string source = mode == 0 ? a : "A🌍é日本語:payload:0123456789abcdef";
        std::string output;
        for (std::int64_t index = 0; index < count; ++index)
        {
            output = output + source;
        }

        return Hash(output);
    }

    const std::string ta = mode == 2 ? a : "A🌍é日本語:payload:0123456789abcdef";
    const std::string tb = mode == 2 ? b : "B🌙é日本語:payload:0123456789abcdef";
    const std::string tc = mode == 2 ? c : "C⭐é日本語:payload:0123456789abcdef";
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        const auto value = ta + tb + tc + ta + tb + tc;
        checksum ^= Hash(value);
    }

    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 3 || inputCount < 1 || inputCount > 100000)
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
