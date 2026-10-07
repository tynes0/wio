#pragma once

#include <wio_native_abi.h>

// Additive wire-v2 sidecar. Layout belongs to the producer's C++ compiler, not
// the VM. Field adapters avoid casting VM storage to a foreign C++ object.
inline constexpr std::uint32_t WIO_NATIVE_POD_LAYOUT_VERSION = 1;

using WioNativePodPackFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, const WioNativeAbiValue*, std::uint64_t,
                                                  WioNativeAbiValue*, WioNativeAbiFailure*) noexcept;
using WioNativePodReadFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                                  WioNativeAbiFailure*) noexcept;

struct WioNativePodFieldDescriptor
{
    const char* name;
    std::uint64_t typeId;
    std::uint64_t offset;
    // Actual producer storage size, including empty fixed-array storage.
    // Fixed-array field adapters return POD slices with this field typeId;
    // consumers reconstruct elements using checked scalar/nested-POD layouts.
    std::uint64_t size;
    std::uint8_t typeKind;
};

struct WioNativePodLayoutDescriptor
{
    std::uint64_t typeId;
    std::uint64_t valueSize;
    std::uint64_t valueAlignment;
    std::uint32_t fieldCount;
    const WioNativePodFieldDescriptor* fields;
    // pack optionally receives the original POD as its first argument, keeping
    // C++ fields not exposed by Wio intact, and returns an owned POD wire value.
    // read returns an independently owned field when it has slice storage.
    // Callers release both with the
    // normal WioNativeAbiReleaseValue operation, including failure paths.
    WioNativePodPackFn pack;
    WioNativePodReadFn read;
};

struct WioNativePodLayoutApi
{
    std::uint32_t version;
    std::uint32_t typeCount;
    const WioNativePodLayoutDescriptor* types;
};
using WioGetNativePodLayoutApiFn = const WioNativePodLayoutApi* (*)();
