#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace wio::vm
{
    class Value final
    {
    public:
        enum class Kind : std::uint8_t
        {
            Empty,
            Null,
            Boolean,
            SignedInteger,
            UnsignedInteger,
            Float64,
            String
        };

        Value() noexcept = default;
        Value(const Value& other);
        Value(Value&& other) noexcept;
        Value& operator=(const Value& other);
        Value& operator=(Value&& other) noexcept;
        ~Value();

        [[nodiscard]] static Value null() noexcept;
        [[nodiscard]] static Value boolean(bool value) noexcept;
        [[nodiscard]] static Value signedInteger(std::int64_t value) noexcept;
        [[nodiscard]] static Value unsignedInteger(std::uint64_t value) noexcept;
        [[nodiscard]] static Value floating(double value) noexcept;
        [[nodiscard]] static Value string(std::string value);

        [[nodiscard]] Kind kind() const noexcept
        {
            return kind_;
        }
        [[nodiscard]] bool isEmpty() const noexcept
        {
            return kind_ == Kind::Empty;
        }
        [[nodiscard]] bool asBoolean() const noexcept
        {
            return scalar_.boolean;
        }
        [[nodiscard]] std::int64_t asSignedInteger() const noexcept
        {
            return scalar_.signedInteger;
        }
        [[nodiscard]] std::uint64_t asUnsignedInteger() const noexcept
        {
            return scalar_.unsignedInteger;
        }
        [[nodiscard]] double asFloat64() const noexcept
        {
            return scalar_.floating;
        }
        [[nodiscard]] std::string_view asString() const noexcept;

        [[nodiscard]] bool operator==(const Value& other) const noexcept;

    private:
        union Scalar
        {
            bool boolean;
            std::int64_t signedInteger;
            std::uint64_t unsignedInteger;
            double floating;

            constexpr Scalar() noexcept : unsignedInteger(0)
            {
            }
        } scalar_;

        union Text
        {
            constexpr Text() noexcept : empty{}
            {
            }
            ~Text()
            {
            }

            char empty;
            std::string string;
        } text_;

        explicit Value(Kind kind) noexcept;
        void destroy() noexcept;
        void copyFrom(const Value& other);
        void moveFrom(Value&& other) noexcept;

        Kind kind_ = Kind::Empty;
    };
} // namespace wio::vm
