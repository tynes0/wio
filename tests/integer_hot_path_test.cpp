#include <integer_format.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace
{
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

    struct RestoreLocale
    {
        std::locale original;

        ~RestoreLocale()
        {
            std::locale::global(original);
        }
    };

    template <class T> void check(T value)
    {
        std::ostringstream oracle;
        oracle << +value;
        const std::string prefix{"x\0", 2};
        auto target = prefix;
        wio::runtime::AppendInteger(target, value);
        if (target != prefix + oracle.str() || wio::runtime::StringifyInteger(value) != oracle.str())
            throw std::runtime_error("integer hot/cold paths changed append or stream semantics");
    }

    void values()
    {
        check(std::int8_t{-128});
        check(std::uint8_t{255});
        check((std::numeric_limits<std::int64_t>::min)());
        check((std::numeric_limits<std::int64_t>::max)());
        check((std::numeric_limits<std::uint64_t>::max)());
        check(std::size_t{1234567});
        std::uint64_t state = 0x28b935a8cbef14d7ULL;
        for (int index = 0; index < 2048; ++index)
        {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            check(state);
            check(-static_cast<std::int64_t>(state >> 1));
        }
    }
} // namespace

int main()
{
    try
    {
        const RestoreLocale restore;
        const std::locale grouped(std::locale::classic(), new Grouped);
        std::locale::global(std::locale::classic());
        values();
        std::locale::global(grouped);
        values();
        // Detect an invalid cached-classic shortcut after native locale changes.
        for (int index = 0; index < 128; ++index)
        {
            std::locale::global(index % 2 ? grouped : std::locale::classic());
            check(std::int64_t{-123456789});
            check(std::uint64_t{123456789});
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
