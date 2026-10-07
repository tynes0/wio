#pragma once

#include "wir_abi.h"
#include <wio_native_sequence.h>

namespace wio::wir_backend::abi
{
    template <class T, std::uint64_t Id, std::uint64_t ElementId> struct SequenceAdapter final
    {
        using Element = typename T::value_type;

        static const T& storage(const WioNativeAbiValue* value)
        {
            require(value && value->kind == WIO_NATIVE_ABI_RUNTIME_VALUE && value->typeId == Id &&
                    value->payload.handle.state && value->payload.handle.ops == &Box<T, Id>::ops);
            return static_cast<const Box<T, Id>*>(value->payload.handle.state)->value;
        }

        template <class F> static WioNativeAbiStatus boundary(WioNativeAbiFailure* output, F&& action) noexcept
        {
            if (output)
                *output = {};
            try
            {
                action();
                return WIO_NATIVE_ABI_OK;
            }
            catch (const Error& error)
            {
                return failure(output, error.status, error.what());
            }
            catch (const std::exception& error)
            {
                return failure(output, WIO_NATIVE_ABI_EXCEPTION, error.what());
            }
            catch (...)
            {
                return failure(output, WIO_NATIVE_ABI_EXCEPTION, "native sequence adapter failed");
            }
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
                                T value;
                                require(count <= value.max_size(), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                value.reserve(static_cast<std::size_t>(count));
                                for (std::uint64_t index = 0; index < count; ++index)
                                    value.push_back(Codec<Element, ElementId>::read(elements[index]));
                                *result = Codec<T, Id>::write(std::move(value));
                            });
        }

        static WioNativeAbiStatus count(const WioNativeAbiValue* value, std::uint64_t* result,
                                        WioNativeAbiFailure* output) noexcept
        {
            if (result)
                *result = 0;
            return boundary(output,
                            [&]
                            {
                                require(result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                *result = storage(value).size();
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
