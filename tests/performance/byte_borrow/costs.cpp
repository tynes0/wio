// Runtime-only attribution aid, not a cross-language or qualified speed gate.
#include "std_io.h"
#include "../byte_io/probe.h"

#include <atomic>
#include <chrono>
#include <iostream>

namespace
{
    namespace io = wio::runtime::std_io;
    volatile std::uint64_t sink;

    std::uint64_t sample(const std::vector<std::uint8_t>& bytes)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        return bytes.size() + bytes.front() + bytes.back();
    }

    std::uint64_t work(io::File& file, std::vector<std::uint8_t>& buffer, const int mode, const std::int64_t count)
    {
        std::uint64_t checksum = 0;
        for (std::int64_t index = 0; index < count; ++index)
        {
            if (mode == 0)
            {
                checksum += byte_io::Hash(buffer);
                continue;
            }

            io::Seek(file, 0);
            if (mode == 1 || mode == 3)
            {
                const auto owned = io::ReadByteValues(file, buffer.size());
                checksum += mode == 1 ? byte_io::Hash(owned) : sample(owned);
            }
            else
            {
                if (io::ReadByteValuesInto(file, buffer) != buffer.size())
                    throw std::runtime_error("incomplete attribution payload");
                checksum += mode == 2 ? byte_io::Hash(buffer) : sample(buffer);
            }
        }

        return checksum;
    }
} // namespace

int main(int argc, char** argv)
{
    const auto count = argc == 2 ? std::stoll(argv[1]) : 4097;
    if (count < 1 || count > 100000)
        return 1;
    byte_io::Setup();
    auto file = io::Open("payload.bin", io::open_read_binary);
    auto buffer = io::ReadByteValues(file, 65536);
    struct Sample
    {
        int round, mode;
        std::int64_t nanoseconds;
        std::uint64_t checksum;
    };
    std::vector<Sample> samples;
    samples.reserve(25);
    for (int mode = 0; mode < 5; ++mode)
    {
        sink = work(file, buffer, mode, 17);
    }

    for (int round = 0; round < 5; ++round)
    {
        for (int offset = 0; offset < 5; ++offset)
        {
            const int mode = (round + (round % 2 ? 4 - offset : offset)) % 5;
            const auto begin = std::chrono::steady_clock::now();
            const auto checksum = work(file, buffer, mode, count);
            const auto end = std::chrono::steady_clock::now();
            sink = checksum;
            samples.push_back(
                {round, mode, std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count(), checksum});
        }
    }

    io::Close(file);
    std::cout << "[";
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        const auto& row = samples[index];
        if (index)
            std::cout << ",";
        std::cout << "{\"round\":" << row.round << ",\"mode\":" << row.mode << ",\"count\":" << count
                  << ",\"nanoseconds\":" << row.nanoseconds << ",\"checksum\":\"" << row.checksum << "\"}";
    }

    std::cout << "]\n";
}
