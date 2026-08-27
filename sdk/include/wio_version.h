#pragma once

#include <cstdint>
#include <string_view>

#define WIO_SDK_VERSION_MAJOR 0
#define WIO_SDK_VERSION_MINOR 16
#define WIO_SDK_VERSION_PATCH 0
#define WIO_SDK_VERSION_STRING "0.16.0"

namespace wio::sdk
{
    struct ProductVersion
    {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;
        std::string_view prerelease;

        [[nodiscard]] constexpr bool is_prerelease() const noexcept
        {
            return !prerelease.empty();
        }
    };

    inline constexpr ProductVersion product_version{
        WIO_SDK_VERSION_MAJOR,
        WIO_SDK_VERSION_MINOR,
        WIO_SDK_VERSION_PATCH,
        {}
    };

    inline constexpr std::string_view product_version_string = WIO_SDK_VERSION_STRING;
}
