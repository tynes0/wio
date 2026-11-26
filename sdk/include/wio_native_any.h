#pragma once

#include <wio_native_abi.h>

// Optional wire-v2 sidecar. Payload ids are stable Wio identities, never
// producer-local C++ type-token addresses. All outputs start zero-initialized
// and must be released by the caller, including on partial failure.
inline constexpr std::uint32_t WIO_NATIVE_ANY_VERSION = 1;

using WioNativeAnyPackFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, WioNativeAbiValue*,
                                                  WioNativeAbiFailure*) noexcept;
using WioNativeAnyReadFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, WioNativeAbiValue*, bool*,
                                                  WioNativeAbiFailure*) noexcept;
using WioNativeAnyIsNullFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, bool*, WioNativeAbiFailure*) noexcept;

struct WioNativeAnyPayloadDescriptor
{
    std::uint64_t typeId;
    WioNativeAnyPackFn pack;
    WioNativeAnyReadFn read;
};

struct WioNativeAnyApi
{
    std::uint32_t version;
    std::uint32_t payloadCount;
    std::uint64_t typeId;
    const WioNativeAnyPayloadDescriptor* payloads;
    WioNativeAnyPackFn packNull;
    WioNativeAnyIsNullFn isNull;
};

using WioGetNativeAnyApiFn = const WioNativeAnyApi* (*)();
