#include "probe.h"

#include <chrono>
#include <iostream>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(std::ifstream& file, std::vector<std::uint8_t>& buffer,
                   const std::int64_t mode, const std::int64_t count)
{
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        file.clear();
        file.seekg(0);
        std::vector<std::uint8_t> owned;
        if (mode < 2)
        {
            owned.resize(65536);
        }

        auto& bytes = mode < 2 ? owned : buffer;
        file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        const auto size = static_cast<std::size_t>(file.gcount());
        if (file.bad() || ((mode == 0 || mode >= 2) && size != bytes.size()))
        {
            throw std::runtime_error("Unable to read benchmark payload");
        }

        if (mode < 2)
        {
            bytes.resize(size);
        }

        checksum += byte_io::Hash(bytes);
    }

    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 3 || inputCount < 1 || inputCount > 100000) return 1;
    byte_io::Setup();
    std::ifstream file("payload.bin", std::ios::binary);
    std::vector<std::uint8_t> buffer(mode >= 2 ? 65536 : 0);
    sink = Work(file, buffer, mode, inputCount);
    sink = Work(file, buffer, mode, inputCount);
    const auto start = std::chrono::steady_clock::now();
    const auto checksum = Work(file, buffer, mode, inputCount);
    sink = checksum;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << checksum
              << "\",\"elapsedNanoseconds\":" << elapsed << ",\"operations\":" << inputCount << "}\n";
}
