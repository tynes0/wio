#include "probe.h"

#include <chrono>
#include <iostream>
#include <vector>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(std::ofstream& file, const std::vector<unsigned char>& buffer,
                   const std::int64_t mode, const std::int64_t count)
{
    std::uint64_t written = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        file.seekp(0);
        if (mode == 0)
        {
            const std::vector<unsigned char> copy(buffer.begin() + 7, buffer.begin() + 65526);
            file.write(reinterpret_cast<const char*>(copy.data()), copy.size());
        }
        else
        {
            file.write(reinterpret_cast<const char*>(buffer.data() + 7), 65519);
        }

        if (!file)
        {
            throw std::runtime_error("Range-write reference failed");
        }

        written += 65519;
    }

    return written;
}

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 1 || inputCount < 1 || inputCount > 100000) return 1;
    byte_write_range::Setup();
    std::ifstream input("payload.bin", std::ios::binary);
    std::vector<unsigned char> buffer(65536);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    if (!input) return 2;
    std::ofstream output("output.bin", std::ios::binary | std::ios::trunc);
    sink = Work(output, buffer, mode, inputCount);
    sink = Work(output, buffer, mode, inputCount);
    const auto start = std::chrono::steady_clock::now();
    const auto written = Work(output, buffer, mode, inputCount);
    sink = written;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    output.close();
    if (!output) return 3;
    const auto checksum = written ^ byte_write_range::CheckOutput();
    std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << checksum
              << "\",\"elapsedNanoseconds\":" << elapsed << ",\"operations\":" << inputCount << "}\n";
}
