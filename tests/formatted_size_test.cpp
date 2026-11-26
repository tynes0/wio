#include <formatted_size.h>

#include <iostream>
#include <limits>
#include <string>

namespace
{
    template <class... Sizes> void rejects(std::size_t limit, Sizes... sizes)
    {
        try
        {
            static_cast<void>(wio::runtime::detail::CheckedFormattedSize(limit, sizes...));
        }
        catch (const std::length_error&)
        {
            return;
        }
        throw std::runtime_error("oversized formatted string was accepted");
    }
} // namespace

int main()
{
    using wio::runtime::detail::CheckedFormattedSize;
    try
    {
        const auto maximum = (std::numeric_limits<std::size_t>::max)();
        if (CheckedFormattedSize(0) != 0 ||
            CheckedFormattedSize(10, std::size_t{3}, std::size_t{0}, std::size_t{7}) != 10 ||
            CheckedFormattedSize(maximum, maximum - 1, std::size_t{1}) != maximum)
            throw std::runtime_error("formatted size boundaries changed");
        rejects(0, std::size_t{1});
        rejects(10, std::size_t{6}, std::size_t{5});
        rejects(maximum, maximum, std::size_t{1});
        rejects(maximum, maximum - 1, std::size_t{1}, std::size_t{1});
        const std::string nul{"a\0b", 3};
        if (CheckedFormattedSize(3, nul.size()) != 3)
            throw std::runtime_error("embedded NUL changed byte length");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
