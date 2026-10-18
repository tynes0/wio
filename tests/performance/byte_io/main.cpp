#include "probe.h"

#include <chrono>
#include <iostream>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(std::ifstream& file, const std::int64_t mode, const std::int64_t count)
{
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        file.clear();
        file.seekg(0);
        std::vector<std::uint8_t> bytes(65536);
        file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        const auto size = static_cast<std::size_t>(file.gcount());
        if (file.bad() || (mode == 0 && size != bytes.size()))
        {
            throw std::runtime_error("Unable to read benchmark payload");
        }

        bytes.resize(size);
        checksum += byte_io::Hash(bytes);
    }

    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 1 || inputCount < 1 || inputCount > 100000) return 1;
    byte_io::Setup();
    std::ifstream file("payload.bin", std::ios::binary);
    sink = Work(file, mode, inputCount);
    sink = Work(file, mode, inputCount);
    const auto start = std::chrono::steady_clock::now();
    const auto checksum = Work(file, mode, inputCount);
    sink = checksum;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << checksum
              << "\",\"elapsedNanoseconds\":" << elapsed << ",\"operations\":" << inputCount << "}\n";
}
