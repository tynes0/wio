#pragma once

#include "wir_sequence_projection.h"
#include <wio_native_abi.h>

namespace wio::wir_backend::abi
{
    template <class T>
    void* resolveContainerTerminal(void* storage, std::uint64_t nodeId,
                                   const WioNativeAbiContainerProjection& projection, std::uint64_t count,
                                   const std::type_info& requested)
    {
        if (!storage || count)
            return nullptr;
        if (projection.podTypeId)
            return nodeId == projection.podTypeId
                       ? NativePodProjectionAccess<T>::resolve(storage, nodeId, projection.typeId, projection.offset,
                                                               projection.size, requested)
                       : nullptr;
        return !projection.offset && !projection.size && nodeId == projection.typeId && requested == typeid(T)
                   ? storage
                   : nullptr;
    }

    // Generated specializations supply each container's stable child identities
    // and checked key codecs. This default handles only an exact terminal value.
    template <class T> struct NativeContainerProjectionAccess
    {
        static void* resolve(void* storage, std::uint64_t nodeId, const WioNativeAbiContainerProjection& projection,
                             const WioNativeAbiContainerStep*, std::uint64_t count, const std::type_info& requested)
        {
            return resolveContainerTerminal<T>(storage, nodeId, projection, count, requested);
        }
    };
} // namespace wio::wir_backend::abi
