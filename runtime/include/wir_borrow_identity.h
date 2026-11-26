#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

namespace wio::wir_backend
{
    // Callback cells currently use independent tagged-value snapshots. Reject
    // aggregate/descendant overlap rather than silently splitting one live cell.
    // Ordinary native calls use the richer canonical projection protocol.
    template <class T> bool containsBorrowStorage(const T& root, const void* target, unsigned depth = 0)
    {
        const auto first = reinterpret_cast<std::uintptr_t>(std::addressof(root));
        const auto address = reinterpret_cast<std::uintptr_t>(target);
        if (address >= first && address - first < sizeof(T))
            return true;
        if (depth >= 64)
            throw std::runtime_error("callback borrow nesting exceeds the supported limit");
        if constexpr (requires {
                          typename T::key_type;
                          typename T::mapped_type;
                      })
        {
            for (const auto& [key, value] : root)
                if (containsBorrowStorage(key, target, depth + 1) || containsBorrowStorage(value, target, depth + 1))
                    return true;
        }
        else if constexpr (requires {
                               typename T::value_type;
                               root.data();
                               root.size();
                           })
        {
            // String character borrows are not callback argument cells.
            if constexpr (!std::is_same_v<typename T::value_type, char> &&
                          !std::is_same_v<typename T::value_type, char32_t>)
                for (const auto& value : root)
                    if (containsBorrowStorage(value, target, depth + 1))
                        return true;
        }
        else if constexpr (requires {
                               root.has_value();
                               *root;
                           })
        {
            if (root && containsBorrowStorage(*root, target, depth + 1))
                return true;
        }
        return false;
    }

    template <std::size_t Count> class CallbackBorrowCells
    {
        struct Cell
        {
            const void* address = nullptr;
            const std::type_info* type = nullptr;
            bool (*contains)(const void*, const void*) = nullptr;
        };
        std::array<Cell, Count> cells_{};
        std::size_t count_ = 0;

    public:
        template <class T> void addCell(const T& value)
        {
            const auto* address = std::addressof(value);
            const auto contains = [](const void* root, const void* target)
            { return containsBorrowStorage(*static_cast<const T*>(root), target); };
            for (std::size_t index = 0; index < count_; ++index)
            {
                const auto& previous = cells_[index];
                if (previous.address == address && *previous.type == typeid(T))
                    return;
                if (previous.contains(previous.address, address) || contains(address, previous.address))
                    throw std::runtime_error(
                        "overlapping callback aggregate/descendant borrows are unsupported; pass one aggregate borrow");
            }
            if (count_ == Count)
                throw std::runtime_error("callback borrow argument count exceeds its contract");
            cells_[count_++] = {address, &typeid(T), contains};
        }
        template <class T> void add(const T& argument)
        {
            if constexpr (requires {
                              argument.borrowIdentity();
                              argument.read();
                          })
                addCell(argument.read());
            else if constexpr (requires {
                                   argument.has_value();
                                   *argument;
                               })
            {
                if (argument)
                    add(*argument);
            }
        }
    };
} // namespace wio::wir_backend
