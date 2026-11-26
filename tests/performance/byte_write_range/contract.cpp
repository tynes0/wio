#define WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"
#include "std_io.h"
#include "probe.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

namespace io = wio::runtime::std_io;
namespace memory = wio::runtime::benchmark;

void Expect(const bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int main()
{
    byte_write_range::Setup();
    auto input = io::Open("payload.bin", io::open_read_binary);
    auto bytes = io::ReadByteValues(input, 65536);
    io::Close(input);
    const auto* storage = bytes.data();
    const auto capacity = bytes.capacity();
    auto output = io::Open("output.bin", io::OpenMode::write | io::OpenMode::create | io::OpenMode::truncate);
    Expect(memory::BeginAllocations(), "Allocation tracking unavailable");
    for (int iteration = 0; iteration < 257; ++iteration)
    {
        io::Seek(output, 0);
        Expect(io::WriteByteValuesRange(output, bytes, 7, 65519) == 65519, "Range write count changed");
    }

    const auto counters = memory::EndAllocations();
    Expect(counters.allocations == 0 && counters.frees == 0 && counters.requestedBytes == 0 &&
               counters.liveBytes == 0 && counters.liveBlocks == 0 && counters.failures == 0,
           "Borrowed range allocated payload storage");
    Expect(bytes.data() == storage && bytes.capacity() == capacity && bytes.size() == 65536,
           "Write changed source storage");
    const auto position = io::Tell(output);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const std::pair<std::size_t, std::size_t> invalidRanges[] = {
        {65537, 0}, {65536, 1}, {1, maximum}, {maximum, 1}
    };
    for (const auto [offset, count] : invalidRanges)
    {
        try
        {
            (void)io::WriteByteValuesRange(output, bytes, offset, count);
            throw std::runtime_error("Invalid range accepted");
        }
        catch (const std::out_of_range&)
        {
            Expect(io::Tell(output) == position, "Invalid range performed I/O");
        }
    }

    Expect(io::WriteByteValuesRange(output, bytes, bytes.size(), 0) == 0 && io::Tell(output) == position,
           "Empty end range changed cursor");
    io::Close(output);
    (void)byte_write_range::CheckOutput();
    Expect(io::WriteByteValuesRange(output, bytes, bytes.size(), 0) == 0, "Empty closed-file compatibility changed");
    std::vector<unsigned char> empty;
    empty.reserve(1024);
    Expect(io::WriteByteValuesRange(output, empty, 0, 0) == 0, "Spare capacity treated as length");
    try
    {
        (void)io::WriteByteValuesRange(output, empty, 0, 1);
        throw std::runtime_error("Reserved uninitialized range accepted");
    }
    catch (const std::out_of_range&)
    {
    }

    try
    {
        (void)io::WriteByteValuesRange(output, bytes, 7, 17);
        throw std::runtime_error("Closed file accepted nonempty range");
    }
    catch (const io::FileWriteException& error)
    {
        Expect(error.Error() == wio::runtime::detail::io::WriteError::null_file &&
                   error.BytesRequested() == 17 && error.BytesWritten() == 0,
               "Closed-file error metadata changed");
    }

    // A native span can itself be a slice; offsets are relative to that view.
    output = io::Open("span.bin", io::OpenMode::write | io::OpenMode::create | io::OpenMode::truncate);
    const auto view = std::span<const unsigned char>(bytes).subspan(5, 24);
    Expect(io::WriteByteValuesRange(output, view, 2, 17) == 17, "Span-relative range changed");
    io::Close(output);
    input = io::Open("span.bin", io::open_read_binary);
    const auto actual = io::ReadByteValues(input, 17);
    io::Close(input);
    Expect(std::equal(actual.begin(), actual.end(), bytes.begin() + 7), "Span selected wrong bytes");
    std::cout << "byte-write-range-contract-ok: 257 writes; zero allocation/free; range and file errors\n";
}
