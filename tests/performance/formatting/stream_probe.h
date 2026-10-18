#pragma once

#include "floating_format.h"
#include "integer_format.h"

namespace stream_probe
{
    class Numbers final : public std::num_put<char>
    {
        static iter_type put(iter_type out)
        {
            static const auto bytes = std::string(256, 'x') + std::string{"\0tail", 5};
            for (const char byte : bytes)
                *out++ = byte;
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
    };

    inline bool Check()
    {
        struct Restore
        {
            std::locale previous;
            ~Restore()
            {
                std::locale::global(previous);
            }
        } restore;
        std::locale::global(std::locale(std::locale::classic(), new Numbers));
        std::ostringstream expected;
        expected << -12345LL << 1.25;
        std::string target;
        target.reserve(2048);
        wio::runtime::AppendInteger(target, -12345LL);
        wio::runtime::AppendFloating(target, 1.25);
        return target == expected.str();
    }
} // namespace stream_probe
