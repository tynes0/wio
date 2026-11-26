#include "std_console.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace
{
    namespace detail = wio::runtime::console::detail;
    namespace format = detail::format_detail;

    void expect(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    template <class... Args> void check(std::string_view pattern, std::string_view expected, const Args&... args)
    {
        auto result = detail::FormatComposite(pattern, args...);
        expect(result && result.Value() == expected, "indexed format bytes changed");
    }
} // namespace

int main()
{
    try
    {
        check("{{{1:D4}}}/{0,-4}", "{0012}/x   ", std::string("x"), 12);
        check("{0:X4}/{0:D3}/{1:F2}", "001F/031/1.25", 31, 1.25);
        check("{0:D}/{1:D}", "-9223372036854775808/18446744073709551615", std::numeric_limits<std::int64_t>::min(),
              std::numeric_limits<std::uint64_t>::max());
        check("{0:X}", "FFFFFFFF", std::int32_t(-1));
        check("{0}", std::string_view("a\0b", 3), std::string("a\0b", 3));
        check("{0:000.00}", "012.50", 12.5);
        check("{0:N2}", "1,234.50", 1234.5);
        check("{0:Q}", "Q", 3); // A custom section without placeholders is literal text.
        for (const auto pattern : {"{", "}", "{2}", "{0:D99999}"})
            if (detail::FormatComposite(pattern, 3))
                throw std::runtime_error(std::string("invalid indexed format accepted: ") + pattern);

        // A nonzero backing region makes accidental dependence on zero padding
        // visible. Conversion must commit exactly the characters it wrote.
        using Scratch = format::StackText<1024>;
        alignas(Scratch) std::array<std::byte, sizeof(Scratch)> bytes;
        std::fill(bytes.begin(), bytes.end(), std::byte{0xA5});
        auto* scratch = new (bytes.data()) Scratch(format::UninitializedScratch{});
        for (const auto value : {std::int64_t(0), std::int64_t(-7), std::numeric_limits<std::int64_t>::min(),
                                 std::numeric_limits<std::int64_t>::max()})
        {
            for (const auto spec : {"", "D", "D20", "G", "X16", "B64", "F3", "N2", "C2", "P1", "000.00", "Q", "D2000"})
            {
                Scratch zeroed;
                scratch->Clear();
                auto expected = format::FormatIntegral(value, spec, zeroed);
                auto actual = format::FormatIntegral(value, spec, *scratch);
                expect(bool(expected) == bool(actual), "integer scratch changed error contract");
                if (expected)
                    expect(expected.Value() == actual.Value(), "integer scratch read unwritten bytes");
            }
        }
        for (const auto value : {0.0, -0.0, 0.125, -12345.5, std::numeric_limits<double>::max(),
                                 std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            for (const auto spec : {"", "G", "F3", "E4", "N2", "C2", "P1", "000.00", "Q", "F999"})
            {
                Scratch zeroed;
                scratch->Clear();
                auto expected = format::FormatFloating(value, spec, zeroed);
                auto actual = format::FormatFloating(value, spec, *scratch);
                expect(bool(expected) == bool(actual), "floating scratch changed error contract");
                if (expected)
                    expect(expected.Value() == actual.Value(), "floating scratch exposed unwritten bytes");
            }
        }
        scratch->~Scratch();
        std::cout << "indexed-format-contract-ok\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
