#pragma once

#include <cstdint>

namespace native_enum_flagset
{
    enum class state : std::uint8_t
    {
        idle = 0,
        playing = 1,
        paused = 2
    };

    enum class feature : std::uint32_t
    {
        none = 0,
        audio = 1u << 0u,
        debug = 1u << 1u,
        all = 3u
    };

    [[nodiscard]] constexpr feature operator|(const feature lhs, const feature rhs) noexcept
    {
        return static_cast<feature>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs)
        );
    }

    [[nodiscard]] constexpr bool HasFeature(const feature value, const feature mask) noexcept
    {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(mask)) ==
               static_cast<std::uint32_t>(mask);
    }

    [[nodiscard]] inline state Advance(const state value) noexcept
    {
        switch (value)
        {
        case state::idle:
            return state::playing;
        case state::playing:
            return state::paused;
        case state::paused:
            return state::idle;
        }

        return state::idle;
    }
}
