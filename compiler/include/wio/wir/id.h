#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace wio::wir
{
    template<typename Tag>
    class Id final
    {
    public:
        using ValueType = std::uint32_t;
        static constexpr ValueType InvalidValue = (std::numeric_limits<ValueType>::max)();

        constexpr Id() = default;
        explicit constexpr Id(const ValueType value) : value_(value) {}

        [[nodiscard]] static constexpr Id invalid() { return Id{}; }
        [[nodiscard]] constexpr bool isValid() const { return value_ != InvalidValue; }
        [[nodiscard]] explicit constexpr operator bool() const { return isValid(); }
        [[nodiscard]] constexpr ValueType value() const { return value_; }

        auto operator<=>(const Id&) const = default;

    private:
        ValueType value_ = InvalidValue;
    };

    struct TypeIdTag;
    struct FunctionIdTag;
    struct BlockIdTag;
    struct ValueIdTag;

    using TypeId = Id<TypeIdTag>;
    using FunctionId = Id<FunctionIdTag>;
    using BlockId = Id<BlockIdTag>;
    using ValueId = Id<ValueIdTag>;
}
