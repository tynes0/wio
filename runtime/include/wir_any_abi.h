#pragma once

#include "any.h"
#include "wir_container_abi.h"
#include <wio_native_any.h>

namespace wio::wir_backend::abi
{
    template <std::uint64_t Id> struct AnyAdapter : ContainerAdapter<runtime::Any, Id>
    {
        using Base = ContainerAdapter<runtime::Any, Id>;

        static WioNativeAbiStatus packNull(const WioNativeAbiValue*, WioNativeAbiValue* result,
                                           WioNativeAbiFailure* error) noexcept
        {
            return Base::boundary(error,
                                  [&]
                                  {
                                      require(result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                      *result = Codec<runtime::Any, Id>::write(runtime::Any{});
                                  });
        }

        static WioNativeAbiStatus isNull(const WioNativeAbiValue* value, bool* result,
                                         WioNativeAbiFailure* error) noexcept
        {
            return Base::boundary(error,
                                  [&]
                                  {
                                      require(result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                      *result = Base::storage(value).IsNull();
                                  });
        }
    };

    template <class T, std::uint64_t Id, std::uint64_t PayloadId> struct AnyPayloadAdapter : AnyAdapter<Id>
    {
        using Base = AnyAdapter<Id>;

        static WioNativeAbiStatus pack(const WioNativeAbiValue* value, WioNativeAbiValue* result,
                                       WioNativeAbiFailure* error) noexcept
        {
            return Base::boundary(error,
                                  [&]
                                  {
                                      require(value && result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                      auto payload = Codec<T, PayloadId>::read(*value);
                                      if constexpr (std::is_same_v<T, void*>)
                                          *result = Codec<runtime::Any, Id>::write(runtime::Any::FromOpaque(payload));
                                      else
                                          *result =
                                              Codec<runtime::Any, Id>::write(runtime::Any::Box(std::move(payload)));
                                  });
        }

        static WioNativeAbiStatus read(const WioNativeAbiValue* value, WioNativeAbiValue* result, bool* matched,
                                       WioNativeAbiFailure* error) noexcept
        {
            if (matched)
                *matched = false;
            return Base::boundary(error,
                                  [&]
                                  {
                                      require(result && matched, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                      const auto& any = Base::storage(value);
                                      if constexpr (std::is_same_v<T, void*>)
                                      {
                                          if (any.IsOpaque())
                                          {
                                              *result = Codec<T, PayloadId>::write(any.AsOpaque());
                                              *matched = true;
                                          }
                                      }
                                      else if (any.template IsBoxed<T>())
                                      {
                                          *result = Codec<T, PayloadId>::write(any.template AsBoxed<T>());
                                          *matched = true;
                                      }
                                  });
        }
    };
} // namespace wio::wir_backend::abi
