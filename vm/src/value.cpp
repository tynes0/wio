#include "wio/vm/value.h"

#include <memory>
#include <utility>

namespace wio::vm
{
    Value::Value(const Kind kind) noexcept : kind_(kind)
    {
    }

    Value::Value(const Value& other)
    {
        copyFrom(other);
    }

    Value::Value(Value&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    Value& Value::operator=(const Value& other)
    {
        if (this == &other)
            return *this;
        destroy();
        copyFrom(other);
        return *this;
    }

    Value& Value::operator=(Value&& other) noexcept
    {
        if (this == &other)
            return *this;
        destroy();
        moveFrom(std::move(other));
        return *this;
    }

    Value::~Value()
    {
        destroy();
    }

    Value Value::null() noexcept
    {
        return Value{Kind::Null};
    }

    Value Value::boolean(const bool value) noexcept
    {
        Value result{Kind::Boolean};
        result.scalar_.boolean = value;
        return result;
    }

    Value Value::signedInteger(const std::int64_t value) noexcept
    {
        Value result{Kind::SignedInteger};
        result.scalar_.signedInteger = value;
        return result;
    }

    Value Value::unsignedInteger(const std::uint64_t value) noexcept
    {
        Value result{Kind::UnsignedInteger};
        result.scalar_.unsignedInteger = value;
        return result;
    }

    Value Value::floating(const double value) noexcept
    {
        Value result{Kind::Float64};
        result.scalar_.floating = value;
        return result;
    }

    Value Value::string(std::string value)
    {
        Value result{Kind::String};
        std::construct_at(&result.text_.string, std::move(value));
        return result;
    }

    Value Value::place(PlaceStorage* const storage) noexcept
    {
        Value result{Kind::Place};
        result.scalar_.place = storage;
        return result;
    }

    PlaceStorage* Value::asPlace() const noexcept
    {
        return kind_ == Kind::Place ? scalar_.place : nullptr;
    }

    std::string_view Value::asString() const noexcept
    {
        return kind_ == Kind::String ? std::string_view{text_.string} : std::string_view{};
    }

    bool Value::operator==(const Value& other) const noexcept
    {
        if (kind_ != other.kind_)
            return false;
        switch (kind_)
        {
        case Kind::Empty:
        case Kind::Null:
            return true;
        case Kind::Boolean:
            return scalar_.boolean == other.scalar_.boolean;
        case Kind::SignedInteger:
            return scalar_.signedInteger == other.scalar_.signedInteger;
        case Kind::UnsignedInteger:
            return scalar_.unsignedInteger == other.scalar_.unsignedInteger;
        case Kind::Float64:
            return scalar_.floating == other.scalar_.floating;
        case Kind::String:
            return text_.string == other.text_.string;
        case Kind::Place:
            return scalar_.place == other.scalar_.place;
        }
        return false;
    }

    void Value::destroy() noexcept
    {
        if (kind_ == Kind::String)
            std::destroy_at(&text_.string);
        kind_ = Kind::Empty;
        scalar_.unsignedInteger = 0;
    }

    void Value::copyFrom(const Value& other)
    {
        kind_ = other.kind_;
        if (kind_ == Kind::String)
            std::construct_at(&text_.string, other.text_.string);
        else
            scalar_ = other.scalar_;
    }

    void Value::moveFrom(Value&& other) noexcept
    {
        kind_ = other.kind_;
        if (kind_ == Kind::String)
        {
            std::construct_at(&text_.string, std::move(other.text_.string));
            std::destroy_at(&other.text_.string);
        }
        else
            scalar_ = other.scalar_;
        other.kind_ = Kind::Empty;
        other.scalar_.unsignedInteger = 0;
    }
} // namespace wio::vm
