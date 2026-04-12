#pragma once

#include <cstdint>

inline constexpr std::uint32_t WIO_MODULE_API_DESCRIPTOR_VERSION = 1u;

enum WioModuleCapability : std::uint32_t
{
    WIO_MODULE_CAP_API_VERSION = 1u << 0,
    WIO_MODULE_CAP_LOAD = 1u << 1,
    WIO_MODULE_CAP_UPDATE = 1u << 2,
    WIO_MODULE_CAP_UNLOAD = 1u << 3,
    WIO_MODULE_CAP_SAVE_STATE = 1u << 4,
    WIO_MODULE_CAP_RESTORE_STATE = 1u << 5
};

struct WioModuleApi
{
    std::uint32_t descriptorVersion;
    std::uint32_t capabilities;
    std::uint32_t stateSchemaVersion;
    std::uint32_t reserved;
    std::uint32_t (*apiVersion)();
    std::int32_t (*load)();
    void (*update)(float);
    std::int32_t (*saveState)();
    std::int32_t (*restoreState)(std::int32_t);
    void (*unload)();
};

using WioModuleGetApiFn = const WioModuleApi*(*)();
