#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <typeinfo>
#include <type_traits>
#include <vector>

namespace wio::wir_backend::abi
{
    // Only generated, registered native POD specializations may expose fields.
    template <class T> struct NativePodProjectionAccess
    {
        static void* resolve(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t,
                             const std::type_info&) noexcept
        {
            return nullptr;
        }
    };

    template <class T> struct PodFieldProjectionAccess
    {
        static void* resolve(void* storage, std::uint64_t nodeId, std::uint64_t targetId, std::uint64_t offset,
                             std::uint64_t size, const std::type_info& requested) noexcept
        {
            if (!storage)
                return nullptr;
            if (offset == 0 && size == sizeof(T) && nodeId == targetId && requested == typeid(T))
                return storage;
            return NativePodProjectionAccess<T>::resolve(storage, nodeId, targetId, offset, size, requested);
        }
    };

    template <class T> struct SequenceProjectionAccess
    {
        static void* resolve(void* storage, const std::uint64_t*, std::uint64_t count,
                             const std::type_info& requested) noexcept
        {
            return count == 0 && requested == typeid(T) ? storage : nullptr;
        }

        static void* resolvePod(void* storage, const std::uint64_t*, std::uint64_t count, std::uint64_t podId,
                                std::uint64_t targetId, std::uint64_t offset, std::uint64_t size,
                                const std::type_info& requested) noexcept
        {
            return count == 0 ? NativePodProjectionAccess<T>::resolve(storage, podId, targetId, offset, size, requested)
                              : nullptr;
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

        static void* resolvePod(void* storage, const std::uint64_t* indices, std::uint64_t count, std::uint64_t podId,
                                std::uint64_t targetId, std::uint64_t offset, std::uint64_t size,
                                const std::type_info& requested) noexcept
        {
            if (!storage || !indices || !count || count > 64)
                return nullptr;
            auto& sequence = *static_cast<Sequence*>(storage);
            if (indices[0] >= sequence.size())
                return nullptr;
            if constexpr (std::is_same_v<T, bool>)
                return nullptr;
            else
                return SequenceProjectionAccess<T>::resolvePod(std::addressof(sequence[indices[0]]), indices + 1,
                                                               count - 1, podId, targetId, offset, size, requested);
        }
    };
    template <class T, std::size_t Extent> struct SequenceProjectionAccess<std::array<T, Extent>>
    {
        using Sequence = std::array<T, Extent>;

        static void* resolve(void* storage, const std::uint64_t* indices, std::uint64_t count,
                             const std::type_info& requested) noexcept
        {
            if (count == 0)
                return requested == typeid(Sequence) ? storage : nullptr;
            if (!storage || !indices || count > 64 || indices[0] >= Extent)
                return nullptr;
            auto& sequence = *static_cast<Sequence*>(storage);
            return SequenceProjectionAccess<T>::resolve(std::addressof(sequence[indices[0]]), indices + 1, count - 1,
                                                        requested);
        }

        static void* resolvePod(void* storage, const std::uint64_t* indices, std::uint64_t count, std::uint64_t podId,
                                std::uint64_t targetId, std::uint64_t offset, std::uint64_t size,
                                const std::type_info& requested) noexcept
        {
            if (!storage || !indices || !count || count > 64 || indices[0] >= Extent)
                return nullptr;
            auto& sequence = *static_cast<Sequence*>(storage);
            return SequenceProjectionAccess<T>::resolvePod(std::addressof(sequence[indices[0]]), indices + 1, count - 1,
                                                           podId, targetId, offset, size, requested);
        }
    };
} // namespace wio::wir_backend::abi
