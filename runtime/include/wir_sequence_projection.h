#pragma once

#include <cstdint>
#include <memory>
#include <typeinfo>
#include <type_traits>
#include <vector>

namespace wio::wir_backend::abi
{
    template <class T> struct SequenceProjectionAccess
    {
        static void* resolve(void* storage, const std::uint64_t*, std::uint64_t count,
                             const std::type_info& requested) noexcept
        {
            return count == 0 && requested == typeid(T) ? storage : nullptr;
        }
    };

    template <class T, class Allocator> struct SequenceProjectionAccess<std::vector<T, Allocator>>
    {
        using Sequence = std::vector<T, Allocator>;

        static void* resolve(void* storage, const std::uint64_t* indices, std::uint64_t count,
                             const std::type_info& requested) noexcept
        {
            if (count == 0)
                return requested == typeid(Sequence) ? storage : nullptr;
            if (!storage || !indices || count > 64)
                return nullptr;
            auto& sequence = *static_cast<Sequence*>(storage);
            if (indices[0] >= sequence.size())
                return nullptr;
            if constexpr (std::is_same_v<T, bool>)
                return nullptr; // vector<bool> has no addressable bool element.
            else
                return SequenceProjectionAccess<T>::resolve(std::addressof(sequence[indices[0]]), indices + 1,
                                                            count - 1, requested);
        }
    };
} // namespace wio::wir_backend::abi
