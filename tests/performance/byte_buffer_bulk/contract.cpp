#define WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"
#include "std_utility.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace heap = wio::runtime::std_heap;
namespace memory = wio::runtime::benchmark;
using Bytes = std::vector<std::uint8_t>;

void Expect(const bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Bytes Pattern(const std::size_t size, const unsigned seed)
{
    Bytes result(size);
    for (std::size_t index = 0; index < size; ++index)
    {
        result[index] = static_cast<std::uint8_t>((index * 19 + seed) & 255);
    }

    return result;
}

Bytes Previous(Bytes destination, std::size_t position, const Bytes& source)
{
    for (const auto value : source)
    {
        if (position < destination.size())
        {
            destination[position] = value;
        }
        else
        {
            destination.push_back(value);
        }

        ++position;
    }

    return destination;
}

int main()
{
    for (std::size_t length = 0; length <= 32; ++length)
    {
        for (std::size_t position = 0; position <= length; ++position)
        {
            for (std::size_t size = 0; size <= 40; ++size)
            {
                auto value = Pattern(length, 11);
                const auto source = Pattern(size, 71);
                const auto snapshot = source;
                const auto expected = Previous(value, position, source);
                const auto end = heap::WriteByteBuffer(value, position, source);
                Expect(value == expected && source == snapshot && end == position + size, "Bulk write changed byte semantics");
            }

            for (const bool reserve : {false, true})
            {
                auto value = Pattern(length, 17);
                const auto original = value;
                const auto expected = Previous(value, position, original);
                if (reserve)
                {
                    value.reserve(length * 2 + 1);
                }

                const auto end = heap::WriteByteBuffer(value, position, value);
                Expect(value == expected && end == position + original.size(), "Self-alias snapshot semantics changed");
            }
        }
    }

    auto value = Pattern(64, 5);
    value.reserve(512);
    const auto source = Pattern(128, 29);
    const auto* storage = value.data();
    const auto capacity = value.capacity();
    Expect(memory::BeginAllocations(), "Allocation window unavailable");
    for (int iteration = 0; iteration < 257; ++iteration)
    {
        value.clear();
        auto position = heap::WriteByteBuffer(value, 0, source);
        position = heap::WriteByteBuffer(value, position / 2, source);
        position = heap::WriteByteBuffer(value, 0, source);
        Expect(position == source.size(), "Bulk cursor changed");
    }

    const auto counters = memory::EndAllocations();
    Expect(counters.allocations == 0 && counters.frees == 0 && counters.requestedBytes == 0 &&
               counters.liveBytes == 0 && counters.liveBlocks == 0 && counters.failures == 0,
           "Reserved bulk writes allocated");
    Expect(value.data() == storage && value.capacity() == capacity, "Bulk write discarded reserved storage");
    const auto snapshot = value;
    try
    {
        (void)heap::WriteByteBuffer(value, std::numeric_limits<std::size_t>::max(), source);
        throw std::runtime_error("Invalid position accepted");
    }
    catch (const std::out_of_range&)
    {
        Expect(value == snapshot && value.data() == storage && value.capacity() == capacity,
               "Rejected write changed the destination");
    }

    std::cout << "byte-buffer-bulk-contract-ok: exhaustive overwrite/grow/empty/self-alias; reserved writes zero heap\n";
}
