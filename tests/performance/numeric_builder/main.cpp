#include "probe.h"

#include <charconv>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

volatile std::int64_t inputCount;
volatile std::uint64_t sink;

template <class T> void Append(std::string& target, T value)
{
    char buffer[128];
    const auto converted = [&]
    {
        if constexpr (std::is_floating_point_v<T>)
            return std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 6);
        else
            return std::to_chars(buffer, buffer + sizeof(buffer), value);
    }();
    if (converted.ec != std::errc{})
        throw std::runtime_error("reference numeric buffer exhausted");
    target.append(buffer, converted.ptr);
}

std::uint64_t Work(const std::int64_t mode, const std::int64_t count)
{
    std::string builder;
    builder.reserve(mode == 2 ? static_cast<std::size_t>(count) * 16 : 64);
    std::uint64_t checksum = 0;
    for (std::int64_t index = 0; index < count; ++index)
    {
        if (mode == 0)
        {
            builder.append("id=");
            Append(builder, static_cast<std::uint64_t>(index));
            builder.append(",value=");
            Append(builder, static_cast<double>(index % 64) * 0.125 - 4.0);
            builder.append(",ok=");
            builder.append(index % 2 == 0 ? "true" : "false");
            builder.push_back('\n');
        }
        else if (mode == 1)
        {
            builder.clear();
            Append(builder, -9223372036854775807LL + index);
            builder.push_back(':');
            Append(builder, 18446744073709551615ULL - static_cast<std::uint64_t>(index));
            builder.push_back(':');
            Append(builder, static_cast<std::int8_t>(index % 127));
            const auto snapshot = builder;
            checksum += format_probe::Hash(snapshot);
        }
        else
        {
            Append(builder, static_cast<float>(index % 64) * 0.125f - 4.0f);
            builder.push_back(',');
        }
    }

    if (mode == 1)
        return checksum;
    const auto snapshot = builder;
    return format_probe::Hash(snapshot);
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 1;
    const auto mode = std::stoll(argv[1]);
    inputCount = std::stoll(argv[2]);
    if (mode < 0 || mode > 2 || inputCount < 1 || inputCount > 100000)
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
