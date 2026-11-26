#include <floating_format.h>
#include <value_format.h>

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    template <class T> void check(const T value)
    {
        std::ostringstream reference;
        reference << value;
        std::string actual("prefix\0", 7);
        wio::runtime::AppendFloating(actual, value);
        if (actual != std::string("prefix\0", 7) + reference.str())
        {
            std::cerr << "format mismatch: " << std::hexfloat << value << " expected=" << reference.str()
                      << " actual=" << actual.substr(7) << '\n';
            throw std::runtime_error("floating output differs from default stream");
        }
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

    class CustomNumbers final : public std::num_put<char>
    {
        iter_type do_put(iter_type output, std::ios_base&, char_type, double) const override
        {
            for (const auto item : std::string_view("custom"))
                *output++ = item;
            return output;
        }
    };
} // namespace

int main()
{
    const auto originalLocale = std::locale();
    const auto originalRounding = std::fegetround();
    try
    {
        std::locale::global(std::locale::classic());
        std::fesetround(FE_TONEAREST);
        // Keep the macOS CI counterexample explicit, independent of the random
        // bit-pattern sequence. The reference is the current host's stream, not
        // a hard-coded spelling from another C++ standard library.
        check(-0x1.a349dp+20f);
        check(-0x1.a349dp+20);
        std::uint64_t bits = 0x123456789abcdef0ULL;
        for (int index = 0; index < 30000; ++index)
        {
            bits = bits * 6364136223846793005ULL + 1442695040888963407ULL;
            check(std::bit_cast<double>(bits));
            check(std::bit_cast<float>(static_cast<std::uint32_t>(bits >> 16)));
        }

        for (const auto mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
        {
            if (std::fesetround(mode) != 0)
                continue;
            for (const double value :
                 {0.0, -0.0, 1.234565, -1.234565, 9.999995, 0.00009999995, 999999.5, -999999.5,
                  std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min(),
                  std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
                  -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
                  -std::numeric_limits<double>::quiet_NaN()})
            {
                check(value);
                expect(std::fegetround() == mode, "formatter changed rounding mode");
            }
        }

        std::fesetround(FE_TONEAREST);
        for (const auto& locale : {std::locale(std::locale::classic(), new Grouping),
                                   std::locale(std::locale::classic(), new CustomNumbers)})
        {
            std::locale::global(locale);
            check(12345.625);
            check(-0.0);
            check(1.234567e27f);
        }

        std::locale::global(std::locale::classic());
        std::string output;
        const std::vector<std::vector<double>> nested{{1.5, -0.0}, {12500000.0, 0.125}};
        wio::runtime::AppendBasicFormatted(output, nested);
        expect(output == "[[1.5, -0], [1.25e+07, 0.125]]", "nested vectors changed formatting");
        output.clear();
        const std::map<std::string, std::vector<std::int8_t>> map{{"bytes", {-128, 0, 127}}};
        wio::runtime::AppendBasicFormatted(output, map);
        expect(output == "{bytes: [-128, 0, 127]}", "map/byte formatting changed");
        output.clear();
        const std::vector<bool> booleans{true, false};
        wio::runtime::AppendBasicFormatted(output, booleans);
        expect(output == "[true, false]", "vector<bool> proxy changed formatting");
        output.clear();
        const std::vector<std::string> nul{std::string("a\0b", 3), "tail"};
        wio::runtime::AppendBasicFormatted(output, nul);
        expect(output == std::string("[a\0b, tail]", 11), "container formatting truncated embedded NUL");
        struct UserValue
        {
        };
        static_assert(!wio::runtime::IsBasicFormattable<UserValue>);
        static_assert(!wio::runtime::IsBasicFormattable<std::vector<UserValue>>);
        static_assert(wio::runtime::IsBasicFormattable<decltype(nested)>);
        std::feraiseexcept(FE_INVALID);
        wio::runtime::AppendFloating(output, 2.5);
        expect((std::fetestexcept(FE_INVALID) & FE_INVALID) != 0, "formatter cleared pre-existing FP flags");
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
