#include "wio/vm/value.h"

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

namespace wio::vm
{
    class AggregateStorage final
    {
    public:
        AggregateStorage(const Value::Kind kind, const std::uint32_t type, std::vector<Value> values)
            : kind_(kind), type_(type), values_(std::move(values))
        {
        }

        void retain() noexcept
        {
            references_.fetch_add(1, std::memory_order_relaxed);
        }

        void release() noexcept
        {
            if (references_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                delete this;
        }

        [[nodiscard]] std::vector<Value>& values() noexcept
        {
            return values_;
        }

        [[nodiscard]] const std::vector<Value>& values() const noexcept
        {
            return values_;
        }

        [[nodiscard]] Value::Kind kind() const noexcept
        {
            return kind_;
        }

        [[nodiscard]] std::uint32_t type() const noexcept
        {
            return type_;
        }

        [[nodiscard]] std::uint32_t referenceCount() const noexcept
        {
            return references_.load(std::memory_order_acquire);
        }

    private:
        std::atomic<std::uint32_t> references_{1};
        Value::Kind kind_;
        std::uint32_t type_ = 0;
        std::vector<Value> values_;
    };

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

    Value Value::text(std::u32string value)
    {
        Value result{Kind::Text};
        std::construct_at(&result.text_.text, std::move(value));
        return result;
    }

    Value Value::array(std::vector<Value> values)
    {
        Value result{Kind::Array};
        result.scalar_.aggregate = new AggregateStorage{Kind::Array, 0, std::move(values)};
        return result;
    }

    Value Value::component(const std::uint32_t type, std::vector<Value> fields)
    {
        Value result{Kind::Component};
        result.scalar_.aggregate = new AggregateStorage{Kind::Component, type, std::move(fields)};
        return result;
    }

    Value Value::object(const std::uint32_t type, std::vector<Value> fields)
    {
        Value result{Kind::Object};
        result.scalar_.aggregate = new AggregateStorage{Kind::Object, type, std::move(fields)};
        return result;
    }

    Value Value::objectBorrow(AggregateStorage* const storage) noexcept
    {
        Value result{Kind::ObjectBorrow};
        result.scalar_.aggregate = storage;
        return result;
    }

    std::uint32_t Value::aggregateType() const noexcept
    {
        return kind_ == Kind::Array || kind_ == Kind::Component || kind_ == Kind::Object || kind_ == Kind::ObjectBorrow
                   ? scalar_.aggregate->type()
                   : 0;
    }

    std::uint32_t Value::strongReferenceCount() const noexcept
    {
        return kind_ == Kind::Object ? scalar_.aggregate->referenceCount() : 0;
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

    std::u32string_view Value::asText() const noexcept
    {
        return kind_ == Kind::Text ? std::u32string_view{text_.text} : std::u32string_view{};
    }

    std::size_t Value::elementCount() const noexcept
    {
        return kind_ == Kind::Array ? scalar_.aggregate->values().size() : 0;
    }

    const Value* Value::element(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::Array || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value* Value::mutableElement(const std::size_t index) noexcept
    {
        if (kind_ != Kind::Array || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    std::size_t Value::fieldCount() const noexcept
    {
        return kind_ == Kind::Component || kind_ == Kind::Object || kind_ == Kind::ObjectBorrow
                   ? scalar_.aggregate->values().size()
                   : 0;
    }

    const Value* Value::field(const std::size_t index) const noexcept
    {
        if ((kind_ != Kind::Component && kind_ != Kind::Object && kind_ != Kind::ObjectBorrow) ||
            index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value* Value::mutableField(const std::size_t index) noexcept
    {
        if ((kind_ != Kind::Component && kind_ != Kind::Object && kind_ != Kind::ObjectBorrow) ||
            index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value Value::cloneOwned() const
    {
        if (kind_ != Kind::Array && kind_ != Kind::Component)
            return *this;
        std::vector<Value> cloned;
        cloned.reserve(scalar_.aggregate->values().size());
        for (const Value& value : scalar_.aggregate->values())
            cloned.push_back(value.cloneOwned());
        return kind_ == Kind::Array ? array(std::move(cloned)) : component(aggregateType(), std::move(cloned));
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
        case Kind::Text:
            return text_.text == other.text_.text;
        case Kind::Array:
            return scalar_.aggregate->values() == other.scalar_.aggregate->values();
        case Kind::Component:
            return aggregateType() == other.aggregateType() &&
                   scalar_.aggregate->values() == other.scalar_.aggregate->values();
        case Kind::Object:
        case Kind::ObjectBorrow:
            return scalar_.aggregate == other.scalar_.aggregate;
        case Kind::Place:
            return scalar_.place == other.scalar_.place;
        }
        return false;
    }

    void Value::destroy() noexcept
    {
        if (kind_ == Kind::String)
            std::destroy_at(&text_.string);
        else if (kind_ == Kind::Text)
            std::destroy_at(&text_.text);
        else if (kind_ == Kind::Array || kind_ == Kind::Component || kind_ == Kind::Object)
            scalar_.aggregate->release();
        kind_ = Kind::Empty;
        scalar_.unsignedInteger = 0;
    }

    void Value::copyFrom(const Value& other)
    {
        kind_ = other.kind_;
        if (kind_ == Kind::String)
            std::construct_at(&text_.string, other.text_.string);
        else if (kind_ == Kind::Text)
            std::construct_at(&text_.text, other.text_.text);
        else if (kind_ == Kind::Array || kind_ == Kind::Component || kind_ == Kind::Object)
        {
            scalar_.aggregate = other.scalar_.aggregate;
            scalar_.aggregate->retain();
        }
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
        else if (kind_ == Kind::Text)
        {
            std::construct_at(&text_.text, std::move(other.text_.text));
            std::destroy_at(&other.text_.text);
        }
        else
            scalar_ = other.scalar_;
        other.kind_ = Kind::Empty;
        other.scalar_.unsignedInteger = 0;
    }
} // namespace wio::vm
