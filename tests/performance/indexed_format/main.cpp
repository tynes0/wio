#include "probe.h"

#include <charconv>
#include <chrono>
#include <iostream>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

std::uint64_t Work(const std::int64_t mode, const std::int64_t count)
{
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        char buffer[128];
        std::string row;
        if (mode == 0)
        {
            auto end = std::to_chars(buffer, buffer + sizeof(buffer), index).ptr;
            row.append(8 - (end - buffer), '0');
            row.append(buffer, end);
            row += '/';
            end = std::to_chars(buffer, buffer + sizeof(buffer), index, 16).ptr;
            for (auto* p = buffer; p != end; ++p)
                if (*p >= 'a' && *p <= 'f')
                    *p -= 'a' - 'A';
            row.append(8 - (end - buffer), '0');
            row.append(buffer, end);
        }
        else
        {
            const auto end =
                std::to_chars(buffer, buffer + sizeof(buffer), index / 8.0, std::chars_format::fixed, 3).ptr;
            row.append(buffer, end);
            row += "/hi      ";
        }

        checksum += indexed_format::Hash(row);
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
