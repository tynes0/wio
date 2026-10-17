#pragma once

#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace wio::runtime::detail
{
    // Only already-rendered string lengths may enter this preflight. Reading a
    // length must not invoke user formatting or reorder observable callbacks.
    template <class... Sizes>
        requires((std::is_same_v<Sizes, std::size_t>) && ...)
    [[nodiscard]] std::size_t CheckedFormattedSize(const std::size_t limit, const Sizes... sizes)
    {
        std::size_t required = 0;
        [[maybe_unused]] const auto add = [&](const std::size_t size)
        {
            if (size > limit - required)
                throw std::length_error("Wio formatted string is too large.");

            required += size;
        };
        (add(sizes), ...);
        return required;
    }
} // namespace wio::runtime::detail
