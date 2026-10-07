#pragma once

#include "wir_container_abi.h"
#include <wio_native_sequence.h>

namespace wio::wir_backend::abi
{
    template <class T, std::uint64_t Id, std::uint64_t ElementId> struct SequenceAdapter final : ContainerAdapter<T, Id>
    {
        using Element = typename T::value_type;
        using ContainerAdapter<T, Id>::boundary;
        using ContainerAdapter<T, Id>::storage;

        static WioNativeAbiStatus pack(const WioNativeAbiValue* elements, std::uint64_t count,
                                       WioNativeAbiValue* result, WioNativeAbiFailure* output) noexcept
        {
            if (result)
                *result = {};
            return boundary(output,
                            [&]
                            {
                                require(result && (!count || elements), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                T value;
                                require(count <= value.max_size(), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                value.reserve(static_cast<std::size_t>(count));
                                for (std::uint64_t index = 0; index < count; ++index)
                                    value.push_back(Codec<Element, ElementId>::read(elements[index]));
                                *result = Codec<T, Id>::write(std::move(value));
                            });
        }

        static WioNativeAbiStatus read(const WioNativeAbiValue* value, std::uint64_t index, WioNativeAbiValue* result,
                                       WioNativeAbiFailure* output) noexcept
        {
            if (result)
                *result = {};
            return boundary(output,
                            [&]
                            {
                                require(result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                const auto& sequence = storage(value);
                                require(index < sequence.size(), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                // Explicit Element also handles std::vector<bool>'s proxy.
                                *result = Codec<Element, ElementId>::write(
                                    Element(sequence[static_cast<std::size_t>(index)]));
                            });
        }
    };
} // namespace wio::wir_backend::abi
