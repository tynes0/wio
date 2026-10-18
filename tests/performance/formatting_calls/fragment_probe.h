#pragma once

#include <locale>
#include <string>

namespace fragment_probe
{
    inline int calls = 0;

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

    class Counted final : public std::num_put<char>
    {
        static iter_type put(iter_type out, const std::string& value)
        {
            const auto bytes = "N" + std::to_string(++calls) + ":" + value;
            for (const char byte : bytes)
                *out++ = byte;
            return out;
        }
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
    };

    inline void SetMode(int mode)
    {
        static const std::locale grouped(std::locale::classic(), new Grouped);
        static const std::locale counted(std::locale::classic(), new Counted);
        std::locale::global(mode == 0 ? std::locale::classic() : mode == 1 ? grouped : counted);
    }

    inline void Reset()
    {
        calls = 0;
    }
    inline int Calls()
    {
        return calls;
    }
} // namespace fragment_probe
