#pragma once

#include <wio_native_abi.h>

// Optional, additive wire-v2 sidecar. Only the producer constructs and accesses
// its concrete C++ container. No foreign container layout crosses this API.
inline constexpr std::uint32_t WIO_NATIVE_SEQUENCE_VERSION = 1;

using WioNativeSequencePackFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                                       WioNativeAbiFailure*) noexcept;
using WioNativeSequenceCountFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, std::uint64_t*,
                                                        WioNativeAbiFailure*) noexcept;
using WioNativeSequenceReadFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                                       WioNativeAbiFailure*) noexcept;

struct WioNativeSequenceDescriptor
{
    std::uint64_t typeId;
    std::uint64_t elementTypeId;
    // pack copies elements and returns an owned runtime handle. read returns
    // an independent wire value. Release outputs, including partial failures,
    // with WioNativeAbiReleaseValue. Inputs are borrowed for the call only.
    // For a concrete fixed array, typeId includes its extent: pack requires
    // exactly that many elements and count must report that exact extent.
    // Fixed-array fields inside native PODs retain their separate POD-byte ABI.
    WioNativeSequencePackFn pack;
    WioNativeSequenceCountFn count;
    WioNativeSequenceReadFn read;
};

struct WioNativeSequenceApi
{
    std::uint32_t version;
    std::uint32_t typeCount;
    const WioNativeSequenceDescriptor* types;
};

using WioGetNativeSequenceApiFn = const WioNativeSequenceApi* (*)();
