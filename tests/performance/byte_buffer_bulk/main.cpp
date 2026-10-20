#include "probe.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

struct ByteBuffer
{
    std::vector<std::uint8_t> data;
    std::size_t position = 0;

    void WriteBlock(const std::vector<std::uint8_t>& source)
    {
        const auto overwrite = std::min(source.size(), data.size() - position);
        data.insert(data.end(), source.begin() + overwrite, source.end());
        if (overwrite != 0)
        {
            std::memcpy(data.data() + position, source.data(), overwrite);
        }

        position += source.size();
    }
};

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(ByteBuffer& buffer, std::vector<std::uint8_t> source, const std::int64_t count)
{
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        buffer.data.clear();
        buffer.position = 0;
        buffer.WriteBlock(source);
        buffer.position = 512;
        buffer.WriteBlock(source);
        buffer.position = 0;
        buffer.WriteBlock(source);
        if (buffer.data.size() != 1536 || buffer.position != 1024)
        {
            throw std::runtime_error("ByteBuffer reference state changed");
        }

        checksum += buffer.data.size() + buffer.position;
    }

    return checksum;
}

int main(int argc, char** argv)
{
    if (argc != 3 || std::stoll(argv[1]) != 0) return 1;
    inputCount = std::stoll(argv[2]);
    if (inputCount < 1 || inputCount > 100000) return 1;
    const auto source = byte_buffer_bulk::Make();
    ByteBuffer buffer;
    buffer.data.reserve(2048);
    sink = Work(buffer, source, inputCount);
    sink = Work(buffer, source, inputCount);
    const auto start = std::chrono::steady_clock::now();
    const auto checksum = Work(buffer, source, inputCount);
    sink = checksum;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    const auto bytes = buffer.data;
    std::cout << "{\"schemaVersion\":1,\"checksum\":\"" << (checksum ^ byte_buffer_bulk::Check(bytes))
              << "\",\"elapsedNanoseconds\":" << elapsed << ",\"operations\":" << inputCount << "}\n";
}
