#define WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"
#include "std_io.h"
#include "probe.h"

#include <iostream>

namespace
{
    namespace io = wio::runtime::std_io;
    namespace memory = wio::runtime::benchmark;

    void expect(const bool value, const char* message)
    {
        if (!value)
        {
            throw std::runtime_error(message);
        }
    }

    std::vector<unsigned char> previous(io::File& file, const std::size_t size, const bool exact)
    {
        const auto bytes = exact ? io::ReadBytes(file, size) : io::ReadSomeBytes(file, size);
        std::vector<unsigned char> result;
        result.reserve(bytes.size());
        for (const auto value : bytes)
        {
            result.push_back(static_cast<unsigned char>(value));
        }

        return result;
    }

    template <class Read> memory::AllocationSnapshot measure(io::File& file, Read read)
    {
        io::Seek(file, 0);
        expect(memory::BeginAllocations(), "allocation diagnostic unavailable");
        {
            const auto bytes = read();
            expect(bytes.size() == 65536 && bytes[0] == 3 && bytes[255] == 242, "payload changed");
        }

        return memory::EndAllocations();
    }

    void partialError(io::File& file, const bool old)
    {
        io::Seek(file, 65530);
        try
        {
            (void)(old ? previous(file, 17, true) : io::ReadByteValues(file, 17));
            throw std::runtime_error("missing partial-read error");
        }
        catch (const io::FileReadException& error)
        {
            expect(error.Error() == wio::runtime::detail::io::ReadError::partial_read, "partial-read category changed");
            expect(error.BytesRequested() == 17 && error.BytesRead() == 6, "partial-read byte counts changed");
            expect(io::Tell(file) == 65536, "partial-read cursor changed");
        }
    }
} // namespace

int main()
{
    byte_io::Setup();
    auto file = io::Open("payload.bin", io::open_read_binary);
    for (const bool exact : {true, false})
    {
        const auto control = measure(file, [&] { return previous(file, 65536, exact); });
        const auto current = measure(
            file, [&] { return exact ? io::ReadByteValues(file, 65536) : io::ReadSomeByteValues(file, 65536); });
        expect(control.allocations == 2 && current.allocations == 1, "full read did not eliminate one allocation");
        expect(control.requestedBytes == 131072 && current.requestedBytes == 65536,
               "full read did not halve requested bytes");
        expect(control.peakBytes == 131072 && current.peakBytes == 65536, "peak payload storage changed");
        expect(current.liveBytes == 0 && current.liveBlocks == 0 && current.failures == 0,
               "read allocation lifetime is unbalanced");
    }

    partialError(file, true);
    partialError(file, false);

    // Reusing a caller-owned payload must do no heap work in either read path.
    std::vector<unsigned char> buffer(65536, 0xCC);
    const auto* storage = buffer.data();
    const auto capacity = buffer.capacity();
    for (const bool exact : {true, false})
    {
        io::Seek(file, 0);
        expect(memory::BeginAllocations(), "reuse allocation diagnostic unavailable");
        for (int iteration = 0; iteration < 17; ++iteration)
        {
            io::Seek(file, 0);
            const auto count = exact ? io::ReadByteValuesInto(file, buffer) : io::ReadSomeByteValuesInto(file, buffer);
            expect(count == buffer.size() && buffer[0] == 3 && buffer[255] == 242, "reuse payload changed");
        }

        const auto counters = memory::EndAllocations();
        expect(counters.allocations == 0 && counters.frees == 0 && counters.requestedBytes == 0 &&
                   counters.liveBytes == 0 && counters.liveBlocks == 0 && counters.failures == 0,
               "reuse allocated or released payload storage");
        expect(buffer.data() == storage && buffer.capacity() == capacity && buffer.size() == 65536,
               "reuse changed caller storage");
    }

    io::Seek(file, 65530);
    std::fill(buffer.begin(), buffer.end(), 0xCC);
    expect(io::ReadSomeByteValuesInto(file, buffer) == 6, "reuse short-read count changed");
    expect(buffer[0] == ((65530 * 17 + 3) & 255) && buffer[5] == 242 && buffer[6] == 0xCC,
           "reuse overwrote the unread suffix");
    expect(io::ReadSomeByteValuesInto(file, buffer) == 0 && buffer[6] == 0xCC,
           "reuse EOF changed storage");
    io::Seek(file, 65530);
    try
    {
        (void)io::ReadByteValuesInto(file, buffer);
        throw std::runtime_error("reuse exact accepted a partial read");
    }
    catch (const io::FileReadException& error)
    {
        expect(error.Error() == wio::runtime::detail::io::ReadError::partial_read &&
                   error.BytesRequested() == 65536 && error.BytesRead() == 6 && io::Tell(file) == 65536,
               "reuse exact partial-read metadata changed");
    }

    // A span can target a subrange without touching neighboring canaries.
    io::Seek(file, 0);
    std::vector<unsigned char> guarded(258, 0xCC);
    expect(io::ReadByteValuesInto(file, std::span<unsigned char>(guarded).subspan(1, 256)) == 256 &&
               guarded.front() == 0xCC && guarded.back() == 0xCC && guarded[1] == 3 && guarded[256] == 242,
           "reuse span escaped its destination range");
    io::Seek(file, 65530);
    const auto tail = io::ReadSomeByteValues(file, 1024 * 1024);
    expect(tail.size() == 6 && tail.capacity() <= 12, "excessive short-read capacity retained");
    const auto eof = io::ReadSomeByteValues(file, 1024 * 1024);
    expect(eof.empty() && eof.capacity() == 0, "EOF retained speculative capacity");
    io::Seek(file, 0);
    const auto snapshot = io::ReadByteValues(file, 256);
    io::Seek(file, 0);
    auto later = io::ReadByteValues(file, 256);
    later[0] = 99;
    expect(snapshot[0] == 3, "owned reads alias each other");
    for (std::size_t index = 0; index < snapshot.size(); ++index)
    {
        expect(snapshot[index] == ((index * 17 + 3) & 255), "byte value was transcoded");
    }

    io::Close(file);
    std::vector<unsigned char> empty;
    empty.reserve(1024);
    expect(io::ReadByteValuesInto(file, empty) == 0 && io::ReadSomeByteValuesInto(file, empty) == 0 &&
               empty.size() == 0 && empty.capacity() >= 1024,
           "reuse treated spare capacity as initialized elements");
    try
    {
        (void)io::ReadSomeByteValuesInto(file, buffer);
        throw std::runtime_error("reuse accepted a closed handle");
    }
    catch (const io::FileReadException& error)
    {
        expect(error.Error() == wio::runtime::detail::io::ReadError::null_file && buffer.data() == storage &&
                   buffer.size() == 65536 && buffer.capacity() == capacity,
               "reuse closed-handle contract changed");
    }
    expect(io::ReadByteValues(file, 0).empty() && io::ReadSomeByteValues(file, 0).empty(),
           "zero-count invalid-handle compatibility changed");
    try
    {
        (void)io::ReadSomeByteValues(file, 1);
        throw std::runtime_error("closed handle accepted");
    }
    catch (const io::FileReadException& error)
    {
        expect(error.Error() == wio::runtime::detail::io::ReadError::null_file, "closed-handle error changed");
    }

    std::cout << "byte-io-contract-ok: owned allocations 2->1; reused payload zero allocations; exact/partial/EOF/span\n";
}
