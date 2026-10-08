#pragma once

#include <wio_native_abi.h>

// Additive wire-v2 sidecar. A native proxy owns a host claim, not a copied VM
// object. Weak proxy caches must not keep that claim alive or create RC cycles.
inline constexpr std::uint32_t WIO_NATIVE_OBJECT_VERSION = 1;
inline constexpr std::uint32_t WIO_NATIVE_OBJECT_HOST_PROXY = 1u;
inline constexpr std::uint32_t WIO_NATIVE_OBJECT_WEAK_CACHE = 2u;

struct WioNativeObjectHost;
using WioNativeObjectInvokeFn = WioNativeAbiStatus (*)(const WioNativeObjectHost*, std::uint64_t methodId,
                                                       const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                                       WioNativeAbiFailure*) noexcept;

struct WioNativeObjectHost
{
    std::uint32_t version = WIO_NATIVE_OBJECT_VERSION;
    std::uint32_t flags = 0; // v1: caller-thread invocation only
    std::uint64_t typeId = 0;
    std::uint64_t objectId = 0;
    std::uint64_t generation = 0;
    WioNativeAbiHandle owner{};
    WioNativeObjectInvokeFn invoke = nullptr;
};

using WioNativeObjectCreateFn = WioNativeAbiStatus (*)(const WioNativeObjectHost*, WioNativeAbiValue*,
                                                       WioNativeAbiFailure*) noexcept;
// Returns a borrowed host token; it remains valid only while the input object
// has a live strong claim. Recovering identity does not acquire another claim.
using WioNativeObjectHostFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, WioNativeObjectHost*,
                                                     WioNativeAbiFailure*) noexcept;
// weak owns producer WeakRef storage. The caller releases it with the usual
// handle operations. lock returns one owned strong wire or STALE_HANDLE.
using WioNativeObjectWeakFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, WioNativeAbiHandle*,
                                                     WioNativeAbiFailure*) noexcept;
using WioNativeObjectLockFn = WioNativeAbiStatus (*)(const WioNativeAbiHandle*, WioNativeAbiValue*,
                                                     WioNativeAbiFailure*) noexcept;

struct WioNativeObjectDescriptor
{
    std::uint64_t typeId;
    std::uint32_t flags;
    WioNativeObjectCreateFn create;
    WioNativeObjectHostFn host;
    WioNativeObjectWeakFn weak;
    WioNativeObjectLockFn lock;
};

struct WioNativeObjectApi
{
    std::uint32_t version;
    std::uint32_t typeCount;
    const WioNativeObjectDescriptor* types;
};

using WioGetNativeObjectApiFn = const WioNativeObjectApi* (*)();
