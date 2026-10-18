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

    std::cout << "byte-io-contract-ok: allocations 2->1, requested/peak 131072->65536; exact/partial/EOF/snapshot\n";
}
