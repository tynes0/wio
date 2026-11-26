#include <integer_format.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace
{
    class GroupedNumbers final : public std::numpunct<char>
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

    class RestoreLocale final
    {
    public:
        ~RestoreLocale()
        {
            std::locale::global(original_);
        }

    private:
        const std::locale original_;
    };

    template <class T> void check(const T value)
    {
        std::ostringstream expected;
        expected << +value;
        if (wio::runtime::StringifyInteger(value) != expected.str())
            throw std::runtime_error("integer formatting differs from stream contract");
    }

    template <class T> void boundaries()
    {
        check((std::numeric_limits<T>::min)());
        check((std::numeric_limits<T>::max)());
        check(T{0});
        check(T{1});
        if constexpr (std::is_signed_v<T>)
            check(T{-1});
    }

    void qualify()
    {
        boundaries<std::int8_t>();
        boundaries<std::uint8_t>();
        boundaries<std::int16_t>();
        boundaries<std::uint16_t>();
        boundaries<std::int32_t>();
        boundaries<std::uint32_t>();
        boundaries<std::int64_t>();
        boundaries<std::uint64_t>();
        boundaries<std::size_t>();
        std::uint64_t state = 0x72a6e48d19c3b5f0ULL;
        for (int index = 0; index < 4096; ++index)
        {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            check(state);
            const auto magnitude = static_cast<std::int64_t>(state >> 1);
            check(magnitude);
            check(-magnitude);
        }
    }
} // namespace

int main()
{
    try
    {
        const RestoreLocale restore;
        std::locale::global(std::locale::classic());
        qualify();
        std::locale::global(std::locale(std::locale::classic(), new GroupedNumbers));
        qualify();
        if (wio::runtime::StringifyInteger(1234567) != "1_234_567")
            throw std::runtime_error("custom global numeric grouping was lost");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
