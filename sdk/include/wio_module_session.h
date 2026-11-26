#pragma once

#include "wio_module_contract.h"
#include "wio_native_abi.h"

// Additive instance-based host surface. Opening never starts user lifecycle;
// closing destroys the instance outside the OS library-unload callback.
inline constexpr std::uint32_t WIO_SDK_MODULE_SESSION_VERSION = 1u;

struct WioSdkModuleSession
{
    void* context = nullptr;
    const WioSdkModuleContract* contract = nullptr;
};

struct WioSdkModuleSessionApi
{
    std::uint32_t descriptorVersion;
    std::uint32_t descriptorSize;
    void* factory;
    std::int32_t (*open)(void* factory, WioSdkModuleSession* result) noexcept;
    // On WRONG_THREAD/reentrancy the session stays live. Otherwise close must
    // destroy it and clear both fields, even if its unload callback failed.
    std::int32_t (*close)(WioSdkModuleSession* session) noexcept;
    std::int32_t (*start)(const WioSdkModuleSession* session) noexcept;
    std::int32_t (*update)(const WioSdkModuleSession* session, double deltaSeconds) noexcept;
    std::int32_t (*apiVersion)(const WioSdkModuleSession* session, std::uint32_t* result) noexcept;
    std::int32_t (*saveState)(const WioSdkModuleSession* session, WioNativeAbiValue* result) noexcept;
    std::int32_t (*restoreState)(const WioSdkModuleSession* session, const WioNativeAbiValue* value) noexcept;
};

using WioGetSdkModuleSessionApiFn = const WioSdkModuleSessionApi* (*)();

[[nodiscard]] inline bool WioValidateSdkModuleSessionApi(const WioSdkModuleSessionApi* api) noexcept
{
    return api && api->descriptorVersion == WIO_SDK_MODULE_SESSION_VERSION &&
           api->descriptorSize >= sizeof(WioSdkModuleSessionApi) && api->factory && api->open && api->close &&
           api->start && api->update && api->apiVersion && api->saveState && api->restoreState;
}
