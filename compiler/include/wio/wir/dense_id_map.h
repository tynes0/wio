#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wio::wir
{
    // WIR ids produced by the compiler are dense and function-local. A regular
    // unordered_map pays for one allocation and one hash lookup per value,
    // which made verification disproportionately expensive. This table keeps
    // the normal dense case contiguous while retaining a bounded sparse
    // fallback so malformed external IR cannot force a huge allocation.
    template<typename Value>
    class DenseIdMap final
    {
    public:
        explicit DenseIdMap(const std::size_t expectedSize = 0)
            : denseLimit_(std::max<std::size_t>(64, expectedSize * 4 + 16)), values_(expectedSize),
              occupied_(expectedSize)
        {
        }

        bool insert(const std::uint32_t id, Value value)
        {
            if (id < denseLimit_)
            {
                const std::size_t index = id;
                if (index >= values_.size())
                {
                    values_.resize(index + 1);
                    occupied_.resize(index + 1);
                }
                if (occupied_[index])
                    return false;
                values_[index] = std::move(value);
                occupied_[index] = 1;
                return true;
            }
            return overflow_.emplace(id, std::move(value)).second;
        }

        [[nodiscard]] const Value* tryGet(const std::uint32_t id) const noexcept
        {
            const std::size_t index = id;
            if (index < occupied_.size() && occupied_[index])
                return &values_[index];
            const auto found = overflow_.find(id);
            return found == overflow_.end() ? nullptr : &found->second;
        }

        [[nodiscard]] bool contains(const std::uint32_t id) const noexcept { return tryGet(id) != nullptr; }

    private:
        std::size_t denseLimit_;
        std::vector<Value> values_;
        std::vector<std::uint8_t> occupied_;
        std::unordered_map<std::uint32_t, Value> overflow_;
    };
} // namespace wio::wir
