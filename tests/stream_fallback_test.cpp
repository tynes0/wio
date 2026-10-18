#define WIO_BENCHMARK_ALLOCATIONS
#include "wio_benchmark_allocations.h"

#include "floating_format.h"
#include "integer_format.h"

#include <bit>
#include <cfenv>
#include <iostream>
#include <limits>

namespace
{
    namespace memory = wio::runtime::benchmark;
    constexpr int repetitions = 128;

    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    struct Restore
    {
        std::locale locale;
        int rounding = std::fegetround();

        ~Restore()
        {
            std::locale::global(locale);
            std::fesetround(rounding);
        }
    };

    class LongNumbers final : public std::num_put<char>
    {
        const bool fail_;
        const std::string bytes_ = std::string(256, 'x') + std::string{"\0tail", 5};

        iter_type put(iter_type out) const
        {
            for (const char byte : bytes_)
                *out++ = byte;
            if (fail_)
                throw std::runtime_error("partial facet output");
            return out;
        }

        iter_type do_put(iter_type out, std::ios_base&, char_type, long) const override
        {
            return put(out);
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, unsigned long) const override
        {
            return put(out);
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, long long) const override
        {
            return put(out);
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, unsigned long long) const override
        {
            return put(out);
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, double) const override
        {
            return put(out);
        }

    public:
        explicit LongNumbers(bool fail = false) : fail_(fail)
        {
        }
    };

    template <class T> void copyAppend(std::string& target, T value)
    {
        std::ostringstream stream;
        stream << value;
        target.append(stream.str());
    }

    template <class T> void optimizedAppend(std::string& target, T value)
    {
        if constexpr (std::is_floating_point_v<T>)
            wio::runtime::AppendFloating(target, value);
        else
            wio::runtime::AppendInteger(target, value);
    }

    template <class T> void check(T value)
    {
        for (const bool reserve : {false, true})
        {
            std::string expected{"a\0b", 3}, actual = expected;
            if (reserve)
                actual.reserve(4096);
            copyAppend(expected, value);
            optimizedAppend(actual, value);
            expect(actual == expected, "stream fallback changed bytes or partial-output behavior");
        }
    }

    template <class Append, class T> memory::AllocationSnapshot allocations(Append append, T value)
    {
        std::string target;
        target.reserve(4096);
        append(target, value); // Warm lazy host stream/facet state outside the window.
        expect(memory::BeginAllocations(), "allocation diagnostic unavailable");
        for (int index = 0; index < repetitions; ++index)
        {
            target.clear();
            append(target, value);
        }

        const auto counters = memory::EndAllocations();
        expect(counters.liveBlocks == 0 && counters.liveBytes == 0 && counters.failures == 0,
               "stream allocations survived their append call");
        return counters;
    }

    template <class Append, class T> memory::AllocationSnapshot growingAllocations(Append append, T value)
    {
        expect(memory::BeginAllocations(), "growing allocation diagnostic unavailable");
        for (int index = 0; index < repetitions; ++index)
        {
            std::string target;
            append(target, value);
        }

        const auto counters = memory::EndAllocations();
        expect(counters.liveBlocks == 0 && counters.liveBytes == 0 && counters.failures == 0,
               "growing stream allocations survived their append call");
        return counters;
    }

    void report(const char* kind, const memory::AllocationSnapshot& copy, const memory::AllocationSnapshot& current)
    {
        std::cout << "{\"kind\":\"" << kind << "\",\"iterations\":" << repetitions
                  << ",\"copyAllocations\":" << copy.allocations << ",\"currentAllocations\":" << current.allocations
                  << ",\"copyRequestedBytes\":" << copy.requestedBytes
                  << ",\"currentRequestedBytes\":" << current.requestedBytes << ",\"copyPeakBytes\":" << copy.peakBytes
                  << ",\"currentPeakBytes\":" << current.peakBytes << ",\"currentLiveBytes\":" << current.liveBytes
                  << "}\n";
    }
} // namespace

int main(int argc, char**)
{
    try
    {
        const Restore restore;
        std::locale::global(std::locale::classic());
        std::fesetround(FE_TONEAREST);
        std::uint64_t bits = 0x73231c4bd19f2a87ULL;
        for (int index = 0; index < 4096; ++index)
        {
            bits = bits * 6364136223846793005ULL + 1442695040888963407ULL;
            check(std::bit_cast<double>(bits));
            check(std::bit_cast<float>(static_cast<std::uint32_t>(bits)));
        }

        for (const int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
        {
            if (std::fesetround(mode) != 0)
                continue;
            for (double value : {0.0, -0.0, -0x1.a349dp+20, 1.234565, -1.234565, std::numeric_limits<double>::min(),
                                 std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::max(),
                                 std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()})
            {
                check(value);
                expect(std::fegetround() == mode, "formatter changed rounding mode");
            }
        }

        std::fesetround(FE_TONEAREST);
        const std::locale longLocale(std::locale::classic(), new LongNumbers);
        const std::locale partialLocale(std::locale::classic(), new LongNumbers(true));
        for (const auto& locale : {longLocale, partialLocale})
        {
            std::locale::global(locale);
            check(std::int64_t{-12345});
            check(std::uint64_t{12345});
            check(1.25);
            check(-0.0f);
        }

        std::locale::global(longLocale);
        const auto integerCopy = allocations(copyAppend<std::int64_t>, std::int64_t{-12345});
        const auto integerCurrent = allocations(optimizedAppend<std::int64_t>, std::int64_t{-12345});
        const auto floatCopy = allocations(copyAppend<double>, 1.25);
        const auto floatCurrent = allocations(optimizedAppend<double>, 1.25);
        const auto growingCopy = growingAllocations(copyAppend<double>, 1.25);
        const auto growingCurrent = growingAllocations(optimizedAppend<double>, 1.25);
        std::locale::global(std::locale::classic());
        report("integer", integerCopy, integerCurrent);
        report("floating", floatCopy, floatCurrent);
        report("growing-floating", growingCopy, growingCurrent);
        expect(growingCopy.allocations == growingCurrent.allocations &&
                   growingCopy.requestedBytes == growingCurrent.requestedBytes,
               "growing target lost the owning stream snapshot boundary");
        // --observe permits the unchanged baseline implementation. CTest's
        // ordinary invocation requires the actual extra-copy removal.
        if (argc == 1)
        {
            // A copied string may also own iterator-debug bookkeeping (MSVC
            // Debug). Require at least its payload allocation per iteration,
            // not one globally fixed count across standard-library builds.
            expect(integerCopy.allocations >= integerCurrent.allocations + repetitions,
                   "integer stream retained its temporary owning copy");
            expect(floatCopy.allocations >= floatCurrent.allocations + repetitions,
                   "floating stream retained its temporary owning copy");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
