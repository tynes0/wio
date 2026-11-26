#pragma once

#include "wio_module_session.h"

inline constexpr std::uint32_t WIO_SDK_MODULE_TASK_VERSION = 1u;

// Separate sidecar: older session-v1 descriptors remain layout-compatible.
// Task operations are affine and scoped to their original session/generation.
struct WioSdkModuleTaskApi
{
    std::uint32_t descriptorVersion;
    std::uint32_t descriptorSize;
    const WioSdkModuleSessionApi* sessions;
    std::int32_t (*ready)(const WioSdkModuleSession*, const WioNativeAbiValue*, bool*) noexcept;
    std::int32_t (*cancel)(const WioSdkModuleSession*, const WioNativeAbiValue*) noexcept;
    std::int32_t (*read)(const WioSdkModuleSession*, const WioNativeAbiValue*, WioNativeAbiValue*,
                         WioNativeAbiFailure*) noexcept;
    std::int32_t (*onComplete)(const WioSdkModuleSession*, const WioNativeAbiValue*, WioNativeTaskCompletionFn,
                               void*) noexcept;
    std::int32_t (*pumpMain)(const WioSdkModuleSession*, std::uint64_t*) noexcept;
};

using WioGetSdkModuleTaskApiFn = const WioSdkModuleTaskApi* (*)();

[[nodiscard]] inline bool WioValidateSdkModuleTaskApi(const WioSdkModuleTaskApi* tasks,
                                                      const WioSdkModuleSessionApi* sessions) noexcept
{
    return tasks && tasks->descriptorVersion == WIO_SDK_MODULE_TASK_VERSION &&
           tasks->descriptorSize >= sizeof(WioSdkModuleTaskApi) && tasks->sessions == sessions &&
           WioValidateSdkModuleSessionApi(sessions) && tasks->ready && tasks->cancel && tasks->read &&
           tasks->onComplete && tasks->pumpMain;
}
