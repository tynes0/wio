#pragma once

#include <wio_native_abi.h>

// Optional wire-v2 sidecar. Entry buffers alternate key/value wire cells;
// entryCount counts pairs, not cells. Container storage stays producer-owned.
inline constexpr std::uint32_t WIO_NATIVE_DICTIONARY_VERSION = 1;
inline constexpr std::uint32_t WIO_NATIVE_DICTIONARY_ORDERED = 1u;

using WioNativeDictionaryPackFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                                         WioNativeAbiFailure*) noexcept;
using WioNativeDictionaryCountFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, std::uint64_t*,
                                                          WioNativeAbiFailure*) noexcept;
// Read all entries in one linear traversal. The caller supplies exactly
// entryCount * 2 zero-initialized cells and releases every output, even on
// failure. Each cell independently owns its slice/handle when necessary.
using WioNativeDictionaryReadFn = WioNativeAbiStatus (*)(const WioNativeAbiValue*, WioNativeAbiValue*, std::uint64_t,
                                                         WioNativeAbiFailure*) noexcept;

struct WioNativeDictionaryDescriptor
{
    std::uint64_t typeId;
    std::uint64_t keyTypeId;
    std::uint64_t valueTypeId;
    std::uint32_t flags;
    WioNativeDictionaryPackFn pack;
    WioNativeDictionaryCountFn count;
    WioNativeDictionaryReadFn read;
};

struct WioNativeDictionaryApi
{
    std::uint32_t version;
    std::uint32_t typeCount;
    const WioNativeDictionaryDescriptor* types;
};

using WioGetNativeDictionaryApiFn = const WioNativeDictionaryApi* (*)();
