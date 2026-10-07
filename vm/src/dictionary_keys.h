#pragma once

#include "wio/vm/value.h"

#include <functional>

namespace wio::vm::detail
{
    struct DictionaryKeyHash final
    {
        static bool supported(const Value& value) noexcept
        {
            switch (value.kind())
            {
            case Value::Kind::Boolean:
            case Value::Kind::SignedInteger:
            case Value::Kind::UnsignedInteger:
            case Value::Kind::String:
            case Value::Kind::Text:
                return true;
            default:
                return false;
            }
        }

        std::size_t operator()(const Value* value) const noexcept
        {
            std::size_t hash = 0;
            switch (value->kind())
            {
            case Value::Kind::Boolean:
                hash = std::hash<bool>{}(value->asBoolean());
                break;
            case Value::Kind::SignedInteger:
                hash = std::hash<std::int64_t>{}(value->asSignedInteger());
                break;
            case Value::Kind::UnsignedInteger:
                hash = std::hash<std::uint64_t>{}(value->asUnsignedInteger());
                break;
            case Value::Kind::String:
                hash = std::hash<std::string_view>{}(value->asString());
                break;
            case Value::Kind::Text:
                hash = std::hash<std::u32string_view>{}(value->asText());
                break;
            default:
                break;
            }
            return hash ^ (static_cast<std::size_t>(value->kind()) * std::size_t{0x9e3779b9});
        }
    };

    struct DictionaryKeyEqual final
    {
        bool operator()(const Value* left, const Value* right) const noexcept
        {
            return *left == *right;
        }
    };
} // namespace wio::vm::detail
