#pragma once

#include <cstdint>

namespace binding_import_smoke
{
    constexpr std::int32_t DefaultPort = 8080;
    constexpr float DefaultScale = 0.5f;

    enum class state : std::uint8_t
    {
        idle = 0,
        running = 1
    };

    enum class feature_mask : std::uint32_t
    {
        none = 0,
        alpha = 1 << 0,
        beta = 1 << 1
    };

    struct color3
    {
        std::uint8_t rgb[3];
        float scale;
    };

    const char* Describe(state value);
    void Scale(color3& value, float factor);
}
