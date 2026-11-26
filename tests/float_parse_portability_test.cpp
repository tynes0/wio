#include "float_parse.h"

#include <array>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        const std::array<std::string_view, 14> values{"1.25", "-0", "1e3", "1e-308", "1e999", "1e-999", "0x1p2",
                                                      "1,5",  "1e", " 1",  "+1",     "nan",   "inf",    "invalid"};
        for (const auto text : values)
        {
            double parsed = 41;
            const auto result = wio::runtime::detail::parseFloatCLocale(text, parsed);
            double native = 41;
            const auto reference = wio::runtime::detail::parseFloatChars(text, native);
            if (result.ec != reference.ec || result.ptr != reference.ptr ||
                (result.ec == std::errc{} && !(parsed == native || (std::isnan(parsed) && std::isnan(native)))))
                throw std::runtime_error("portable/native float parse mismatch: " + std::string{text});
        }
        const std::string bounded = "12.75trailing";
        double parsed = 0;
        auto result = wio::runtime::detail::parseFloatCLocale(std::string_view{bounded.data(), 5}, parsed);
        if (result.ec != std::errc{} || result.ptr != bounded.data() + 5 || parsed != 12.75)
            throw std::runtime_error("float parser read beyond the bounded view");
        const std::string embedded{"1\0.5", 4};
        result = wio::runtime::detail::parseFloatCLocale(embedded, parsed);
        if (result.ptr != embedded.data() + 1 || parsed != 1)
            throw std::runtime_error("float parser lost embedded NUL end position");
        float single = 0;
        result = wio::runtime::detail::parseFloatCLocale("1.5", single);
        if (result.ec != std::errc{} || single != 1.5f)
            throw std::runtime_error("portable f32 parser failed");
        std::cout << "Portable float conversion matches native general grammar and bounded views\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
