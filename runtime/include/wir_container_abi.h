#pragma once

#include "wir_abi.h"

namespace wio::wir_backend::abi
{
    template <class T, std::uint64_t Id> struct ContainerAdapter
    {
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
                return failure(output, WIO_NATIVE_ABI_EXCEPTION, "native container adapter failed");
            }
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
    };
} // namespace wio::wir_backend::abi
