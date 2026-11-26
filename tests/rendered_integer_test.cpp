#include "rendered_integer.h"

#include <iostream>
#include <limits>
#include <optional>

namespace
{
    using wio::runtime::detail::RenderedInteger;
    static_assert(sizeof(RenderedInteger) <= sizeof(std::optional<std::string>));

    template <class T>
    concept CanRender = requires(RenderedInteger& part, T value) { part.format(value); };
    static_assert(CanRender<std::int64_t> && CanRender<std::uint64_t>);
    static_assert(!CanRender<bool> && !CanRender<char> && !CanRender<std::int8_t> && !CanRender<double>);

    struct Restore
    {
        std::locale previous;
        ~Restore()
        {
            std::locale::global(previous);
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

    class Large final : public std::num_put<char>
    {
        iter_type do_put(iter_type out, std::ios_base&, char_type, long value) const override
        {
            return put(out, std::to_string(value));
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, unsigned long value) const override
        {
            return put(out, std::to_string(value));
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, long long value) const override
        {
            return put(out, std::to_string(value));
        }
        iter_type do_put(iter_type out, std::ios_base&, char_type, unsigned long long value) const override
        {
            return put(out, std::to_string(value));
        }
        static iter_type put(iter_type out, const std::string& value)
        {
            const auto bytes = std::string(256, 'x') + std::string{"\0", 1} + value;
            for (const char byte : bytes)
                *out++ = byte;
            return out;
        }
    };

    template <class T> void check(RenderedInteger& part, T value)
    {
        const auto expected = wio::runtime::StringifyInteger(value);
        part.format(value);
        if (part.view() != expected || part.size() != expected.size())
            throw std::runtime_error("fragment differs from canonical integer formatting");
        const auto stable = std::string(part.view());
        const auto locale = std::locale();
        std::locale::global(std::locale::classic());
        if (part.view() != stable)
            throw std::runtime_error("rendering was deferred across a locale change");
        std::locale::global(locale);
    }
} // namespace

int main()
{
    try
    {
        const Restore restore;
        const std::locale grouped(std::locale::classic(), new Grouped);
        const std::locale large(std::locale::classic(), new Large);
        RenderedInteger part;
        for (int iteration = 0; iteration < 64; ++iteration)
        {
            std::locale::global(iteration % 3 == 0 ? std::locale::classic() : iteration % 3 == 1 ? grouped : large);
            check(part, (std::numeric_limits<std::int64_t>::min)());
            check(part, (std::numeric_limits<std::uint64_t>::max)());
            check(part, std::int16_t{-32768});
            check(part, std::uint16_t{65535});
            check(part, std::int32_t{0});
            check(part, std::uint32_t{123456789});
            part.clear();
            part.clear();
            if (!part.view().empty())
                throw std::runtime_error("fragment cleanup is not idempotent");
        }
        std::uint64_t state = 0x3a981fd43c4d8e71ULL;
        for (int iteration = 0; iteration < 2048; ++iteration)
        {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            std::locale::global(iteration % 2 ? grouped : std::locale::classic());
            check(part, state);
            check(part, -static_cast<std::int64_t>(state >> 1));
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
