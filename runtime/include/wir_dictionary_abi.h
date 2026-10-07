#pragma once

#include "wir_container_abi.h"
#include <wio_native_dictionary.h>

namespace wio::wir_backend::abi
{
    template <class T, std::uint64_t Id, std::uint64_t KeyId, std::uint64_t ValueId> struct DictionaryAdapter final
    {
        using Key = typename T::key_type;
        using Mapped = typename T::mapped_type;
        // Share the checked producer box and exception boundary with sequences.
        using Boundary = ContainerAdapter<T, Id>;

        static WioNativeAbiStatus pack(const WioNativeAbiValue* entries, std::uint64_t count, WioNativeAbiValue* result,
                                       WioNativeAbiFailure* output) noexcept
        {
            if (result)
                *result = {};
            return Boundary::boundary(output,
                                      [&]
                                      {
                                          require(result && (!count || entries), WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          T value;
                                          require(count <= value.max_size() &&
                                                      count <= std::numeric_limits<std::size_t>::max() / 2,
                                                  WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          if constexpr (requires { value.reserve(std::size_t{}); })
                                              value.reserve(static_cast<std::size_t>(count));
                                          for (std::uint64_t index = 0; index < count; ++index)
                                          {
                                              auto key = Codec<Key, KeyId>::read(entries[index * 2]);
                                              auto mapped = Codec<Mapped, ValueId>::read(entries[index * 2 + 1]);
                                              require(value.emplace(std::move(key), std::move(mapped)).second,
                                                      WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          }
                                          *result = Codec<T, Id>::write(std::move(value));
                                      });
        }

        static WioNativeAbiStatus count(const WioNativeAbiValue* value, std::uint64_t* result,
                                        WioNativeAbiFailure* output) noexcept
        {
            return Boundary::count(value, result, output);
        }

        static WioNativeAbiStatus read(const WioNativeAbiValue* value, WioNativeAbiValue* entries, std::uint64_t count,
                                       WioNativeAbiFailure* output) noexcept
        {
            return Boundary::boundary(output,
                                      [&]
                                      {
                                          const auto& dictionary = Boundary::storage(value);
                                          require(count == dictionary.size() && (!count || entries),
                                                  WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          std::size_t index = 0;
                                          for (const auto& [key, mapped] : dictionary)
                                          {
                                              entries[index++] = Codec<Key, KeyId>::write(key);
                                              entries[index++] = Codec<Mapped, ValueId>::write(mapped);
                                          }
                                      });
        }
    };
} // namespace wio::wir_backend::abi
