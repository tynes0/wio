#define WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"

#include "format.h"

#include <cfenv>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace
{
    namespace memory = wio::runtime::benchmark;

    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    template <class... Args> std::string copyFormat(const std::string_view format, Args&&... args)
    {
        std::ostringstream stream;
        wio::runtime::detail::appendFormatLiterals(stream, format, std::forward<Args>(args)...);
        return stream.str();
    }

    struct Seek
    {
        friend std::ostream& operator<<(std::ostream& stream, const Seek&)
        {
            const std::string bytes(256, 's');
            stream << bytes << std::string{"\0tail", 5};
            stream.seekp(3);
            return stream << "XYZ";
        }
    };

    struct Partial
    {
        friend std::ostream& operator<<(std::ostream& stream, const Partial&)
        {
            stream << std::string(256, 'p') << std::string{"\0partial", 8};
            stream.setstate(std::ios::badbit);
            return stream;
        }
    };

    struct Throwing
    {
        friend std::ostream& operator<<(std::ostream&, const Throwing&)
        {
            throw std::runtime_error("hook failure");
        }
    };

    struct Hook
    {
        int& calls;
        std::string _WF_ToString() const
        {
            return std::string(192, static_cast<char>('a' + calls++));
        }
    };

    class Grouped final : public std::numpunct<char>
    {
        char do_thousands_sep() const override
        {
            return '_';
        }
        std::string do_grouping() const override
        {
            return "\3";
        }
    };

    template <class... Args> void check(const std::string_view format, const Args&... args)
    {
        expect(wio::runtime::Format(format, args...) == copyFormat(format, args...), "stream extraction changed bytes");
    }

    void diagnostic(const bool observe)
    {
        const std::string payload(192, 'x');
        constexpr int count = 128;
        const auto measure = [&](const bool copy)
        {
            // Warm host stream state outside the allocation window.
            auto warm = copyFormat("payload={};index={};ok={}", payload, 7, true);
            expect(!warm.empty(), "warmup failed");
            expect(memory::BeginAllocations(), "allocation diagnostic unavailable");
            for (int index = 0; index < count; ++index)
            {
                const auto result = copy ? copyFormat("payload={};index={};ok={}", payload, index, true)
                                         : wio::runtime::Format("payload={};index={};ok={}", payload, index, true);
                expect(result.size() > payload.size(), "missing formatted output");
            }

            return memory::EndAllocations();
        };
        const auto copy = measure(true);
        const auto current = measure(false);
        const auto original = copyFormat("payload={};index={};ok={}", payload, 7, true);
        const auto candidate = wio::runtime::Format("payload={};index={};ok={}", payload, 7, true);
        std::cout << "{\"copyAllocations\":" << copy.allocations << ",\"currentAllocations\":" << current.allocations
                  << ",\"copyRequestedBytes\":" << copy.requestedBytes
                  << ",\"currentRequestedBytes\":" << current.requestedBytes << ",\"copyPeakBytes\":" << copy.peakBytes
                  << ",\"currentPeakBytes\":" << current.peakBytes << ",\"resultSize\":" << candidate.size()
                  << ",\"copyCapacity\":" << original.capacity() << ",\"currentCapacity\":" << candidate.capacity()
                  << ",\"currentLiveBytes\":" << current.liveBytes << "}\n";
        expect(copy.liveBytes == 0 && current.liveBytes == 0 && copy.liveBlocks == 0 && current.liveBlocks == 0,
               "format result leaked");
        if (!observe)
            expect(copy.allocations >= current.allocations + count, "stream result still takes an owning copy");
    }
} // namespace

int main(int argc, char**)
{
    try
    {
        struct Restore
        {
            std::locale locale;
            int mode = std::fegetround();
            ~Restore()
            {
                std::locale::global(locale);
                std::fesetround(mode);
            }
        } restore;
        std::locale::global(std::locale::classic());
        for (const std::size_t size : {0U, 1U, 14U, 15U, 16U, 22U, 23U, 31U, 192U, 511U, 512U, 513U, 8192U})
        {
            std::string payload(size, 'x');
            if (!payload.empty())
                payload[payload.size() / 2] = '\0';
            check(std::string_view(payload));
            check("prefix:{}:{}:{}", payload, true, -7);
            check("{}", payload, "unused");
        }

        check("{}:{}:{}:{}", 1, false);
        check("no markers", 1, 2);
        check("{}", static_cast<const char*>(nullptr));
        check("{}", std::vector<int>{1, 2, 3});
        check("{}", std::map<int, std::string>{{1, "hello"}, {2, std::string{"a\0b", 3}}});
        check("{}suffix", Seek{});
        check("{}suffix", Partial{});
        for (const int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
        {
            if (std::fesetround(mode) != 0)
                continue;
            check("{} {} {}", -0.0, 1.234565, std::numeric_limits<double>::max());
        }

        std::locale::global(std::locale(std::locale::classic(), new Grouped));
        check("{} {}", 1234567, 1234567.125);
        std::locale::global(std::locale::classic());
        int calls = 0;
        const auto actual = wio::runtime::Format("{}:{}", Hook{calls}, Hook{calls});
        expect(calls == 2, "hook count changed");
        calls = 0;
        expect(actual == copyFormat("{}:{}", Hook{calls}, Hook{calls}), "hook ordering changed");
        bool threw = false;
        try
        {
            (void)wio::runtime::Format("{}", Throwing{});
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string_view(error.what()) == "hook failure";
        }

        expect(threw, "native hook exception was swallowed");
        diagnostic(argc > 1);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
