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

        template <std::size_t... Indices>
        static T fixedValue(const WioNativeAbiValue* elements, std::index_sequence<Indices...>)
        {
            // Initialize each element directly. Default-constructing an array
            // first would execute native POD member initializers before
            // overwriting already materialized producer values.
            return T{Codec<Element, ElementId>::read(elements[Indices])...};
        }

        static WioNativeAbiStatus pack(const WioNativeAbiValue* elements, std::uint64_t count,
                                       WioNativeAbiValue* result, WioNativeAbiFailure* output) noexcept
        {
            if (result)
                *result = {};
            return boundary(output,
                            [&]
                            {
                                require(result && (!count || elements), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                if constexpr (requires(T value) { value.reserve(std::size_t{}); })
                                {
                                    T value;
                                    require(count <= value.max_size(), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                    value.reserve(static_cast<std::size_t>(count));
                                    for (std::uint64_t index = 0; index < count; ++index)
                                        value.push_back(Codec<Element, ElementId>::read(elements[index]));
                                    *result = Codec<T, Id>::write(std::move(value));
                                }
                                else
                                {
                                    constexpr auto extent = std::tuple_size_v<T>;
                                    require(count == extent, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                    *result =
                                        Codec<T, Id>::write(fixedValue(elements, std::make_index_sequence<extent>{}));
                                }
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
