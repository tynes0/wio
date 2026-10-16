#define WIO_BENCHMARK_ALLOCATIONS
#include <wio_benchmark_allocations.h>

#include <intrinsics.h>

#include <cfenv>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    template <class T> void AppendNumeric(std::string& target, const T value)
    {
        wio::intrinsics::StringAppend(target, value);
    }

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    class Grouping final : public std::numpunct<char>
    {
        char do_decimal_point() const override
        {
            return ',';
        }

        char do_thousands_sep() const override
        {
            return '_';
        }

        std::string do_grouping() const override
        {
            return "\3";
        }
    };
} // namespace

int main()
{
    namespace memory = wio::runtime::benchmark;
    const auto originalLocale = std::locale();
    const auto originalRounding = std::fegetround();
    try
    {
        std::locale::global(std::locale::classic());
        std::fesetround(FE_TONEAREST);
        std::string boolVector;
        wio::runtime::AppendBasicFormatted(boolVector, std::vector<bool>{true, false});
        expect(boolVector == "[true, false]", "const vector<bool> proxy was not normalized");
        std::string builder;
        builder.reserve(4096);
        // Warm lazy host facilities before the measured allocation window.
        AppendNumeric(builder, std::int64_t{-1});
        AppendNumeric(builder, std::uint64_t{1});
        AppendNumeric(builder, -0.0);
        AppendNumeric(builder, true);
        const auto capacity = builder.capacity();
        expect(memory::BeginAllocations(), "allocation diagnostics unavailable");
        for (int index = 0; index < 1000; ++index)
        {
            builder.clear();
            builder.append("a\0b", 3);
            AppendNumeric(builder, std::numeric_limits<std::int64_t>::min());
            AppendNumeric(builder, std::numeric_limits<std::uint64_t>::max());
            AppendNumeric(builder, -0.0);
            AppendNumeric(builder, 1.23456789f);
            AppendNumeric(builder, true);
            AppendNumeric(builder, false);
        }

        const auto counters = memory::EndAllocations();
        expect(counters.allocations == 0 && counters.liveBlocks == 0, "reserved numeric append allocated");
        expect(builder.capacity() == capacity, "reserved capacity was discarded");
        expect(builder == std::string("a\0b", 3) + "-922337203685477580818446744073709551615-01.23457truefalse",
               "numeric append truncated or widened an exact value");
        const auto snapshot = builder;
        builder.clear();
        AppendNumeric(builder, 9007199254740993ULL);
        expect(builder == "9007199254740993", "u64 was widened through lossy double");
        expect(snapshot.size() > builder.size() && snapshot[1] == '\0', "owning snapshot was mutated");
        std::locale::global(std::locale(std::locale::classic(), new Grouping));
        builder.clear();
        AppendNumeric(builder, 1234567);
        builder.push_back('|');
        AppendNumeric(builder, 1234.5);
        builder.push_back('|');
        AppendNumeric(builder, static_cast<std::int8_t>(127));
        expect(builder == "1_234_567|1_234,5|127", "numeric locale or i8 display policy changed");
        std::locale::global(std::locale::classic());
        for (const auto mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
        {
            if (std::fesetround(mode) != 0)
                continue;
            std::ostringstream reference;
            reference << 1.234565;
            builder.clear();
            AppendNumeric(builder, 1.234565);
            expect(builder == reference.str(), "numeric append ignored the current rounding mode");
        }
    }
    catch (const std::exception& error)
    {
        std::locale::global(originalLocale);
        std::fesetround(originalRounding);
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::locale::global(originalLocale);
    std::fesetround(originalRounding);
    return 0;
}
