#define WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"

#define main generated_wio_main
#include "main.wio.cpp"
#undef main

namespace
{
    namespace memory = wio::runtime::benchmark;

    struct Reserved
    {
        friend std::ostringstream& operator<<(std::ostringstream& stream, const Reserved&)
        {
            std::string value(217, 'r');
            value.reserve(1024 * 1024);
            stream.str(std::move(value));
            return stream;
        }
    };

    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    std::string copyStringify(const generated_stream::Label& value)
    {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    }

    std::string copyBare(const generated_stream::Label& value)
    {
        std::string result;
        result.append(copyStringify(value));
        return result;
    }

    std::string copyAffix(const generated_stream::Label& value)
    {
        std::string result{"pre\0", 4};
        result.append(copyStringify(value));
        result.append("post");
        return result;
    }

    std::string currentAffix(const generated_stream::Label& value)
    {
        std::string result{"pre\0", 4};
        wio::wir_backend::append_formatted_literal_tail(result, value, "post");
        return result;
    }

    template <class Format> memory::AllocationSnapshot measure(Format format)
    {
        expect(memory::BeginAllocations(), "allocation diagnostic unavailable");
        for (int index = 0; index < 128; ++index)
        {
            const auto result = format(generated_stream::Label{192, index});
            expect(result.size() >= 216, "output missing");
        }

        const auto counters = memory::EndAllocations();
        expect(counters.liveBytes == 0 && counters.liveBlocks == 0 && counters.failures == 0,
               "generated formatter leak");
        return counters;
    }
} // namespace

int main()
{
    try
    {
        for (const int mode : {0, 1, 2})
        {
            generated_stream::Reset(mode);
            for (const std::int64_t size : {0, 3, 15, 16, 192, 8192})
            {
                const generated_stream::Label value{size, 1234567};
                expect(wio::wir_backend::format_single(value) == copyBare(value), "bare stream contract changed");
                expect(currentAffix(value) == copyAffix(value), "affixed stream contract changed");
            }
        }

        generated_stream::Reset(3);
        bool threw = false;
        try
        {
            (void)wio::wir_backend::format_single(generated_stream::Label{192, 7});
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string_view(error.what()) == "generated stream hook failure";
        }

        expect(threw, "stream operator exception changed");
        const auto reserved = wio::wir_backend::format_single(Reserved{});
        expect(reserved == std::string(217, 'r') && reserved.capacity() - reserved.size() <= reserved.size(),
               "short result retained an oversized native stream buffer");
        generated_stream::Reset(0);
        const auto copy = measure(copyBare);
        const auto current = measure([](const auto& value) { return wio::wir_backend::format_single(value); });
        const auto affixCopy = measure(copyAffix);
        const auto affixCurrent = measure(currentAffix);
        const auto original = copyBare({192, 7});
        const auto candidate = wio::wir_backend::format_single(generated_stream::Label{192, 7});
        std::cout << "{\"copyAllocations\":" << copy.allocations << ",\"currentAllocations\":" << current.allocations
                  << ",\"copyRequestedBytes\":" << copy.requestedBytes
                  << ",\"currentRequestedBytes\":" << current.requestedBytes << ",\"copyPeakBytes\":" << copy.peakBytes
                  << ",\"currentPeakBytes\":" << current.peakBytes << ",\"copyCapacity\":" << original.capacity()
                  << ",\"currentCapacity\":" << candidate.capacity()
                  << ",\"affixCopyAllocations\":" << affixCopy.allocations
                  << ",\"affixCurrentAllocations\":" << affixCurrent.allocations
                  << ",\"affixCopyRequestedBytes\":" << affixCopy.requestedBytes
                  << ",\"affixCurrentRequestedBytes\":" << affixCurrent.requestedBytes
                  << ",\"affixCopyCapacity\":" << copyAffix({192, 7}).capacity()
                  << ",\"affixCurrentCapacity\":" << currentAffix({192, 7}).capacity()
                  << ",\"affixCopyPeakBytes\":" << affixCopy.peakBytes
                  << ",\"affixCurrentPeakBytes\":" << affixCurrent.peakBytes << "}\n";
        expect(copy.allocations >= current.allocations + 128, "bare formatter retained its second assembly copy");
        expect(affixCopy.allocations >= affixCurrent.allocations + 256,
               "affix formatter retained snapshot/suffix growth");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
