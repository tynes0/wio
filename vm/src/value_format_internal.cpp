#include "value_format_internal.h"

#include "wio/vm/unicode.h"
#include <value_format.h>

namespace wio::vm::detail
{
    namespace
    {
        // Pinned bytecode semantic type identities, not host C++ RTTI.
        constexpr std::uint8_t TypeI8 = 3;
        constexpr std::uint8_t TypeU8 = 8;
        constexpr std::uint8_t TypeF32 = 13;
        constexpr std::uint8_t TypeByte = 15;
        constexpr std::uint8_t TypeChar = 16;
        constexpr std::uint8_t TypeNamed = 28;
        constexpr std::uint8_t TypeReference = 29;
        constexpr std::uint8_t TypeArray = 31;
        constexpr std::uint8_t TypeDictionary = 32;
        constexpr std::uint8_t NominalEnum = 4;
        constexpr std::uint8_t NominalFlagset = 5;
    } // namespace

    bool ValueFormatter::append(const bytecode::Module& module, std::uint32_t type, const Value& value,
                                std::string& target, const unsigned depth)
    {
        if (depth > 128 || type >= module.types.size())
            return false;
        const auto& layout = module.types[type];
        if (layout.kind == TypeReference && layout.arguments.size() == 1)
            return append(module, layout.arguments.front(), value, target, depth + 1);
        if (layout.kind == TypeNamed && (layout.nominalKind == NominalEnum || layout.nominalKind == NominalFlagset))
            return append(module, layout.enumUnderlyingType, value, target, depth + 1);
        if (layout.kind == TypeChar)
        {
            if (value.kind() != Value::Kind::UnsignedInteger || value.asUnsignedInteger() > 255)
                return false;
            target.push_back(static_cast<char>(value.asUnsignedInteger()));
            return true;
        }

        switch (value.kind())
        {
        case Value::Kind::Null:
            target.append("null");
            return true;
        case Value::Kind::Boolean:
            runtime::AppendBasicFormatted(target, value.asBoolean());
            return true;
        case Value::Kind::String:
            target.append(value.asString());
            return true;
        case Value::Kind::Text:
            target.append(encodeUtf8(value.asText()));
            return true;
        case Value::Kind::SignedInteger:
            if (layout.kind == TypeI8)
                target.append(std::to_string(value.asSignedInteger()));
            else
                runtime::AppendInteger(target, value.asSignedInteger());
            return true;
        case Value::Kind::UnsignedInteger:
            if (layout.kind == TypeU8 || layout.kind == TypeByte)
                target.append(std::to_string(value.asUnsignedInteger()));
            else
                runtime::AppendInteger(target, value.asUnsignedInteger());
            return true;
        case Value::Kind::Float64:
            if (layout.kind == TypeF32)
                runtime::AppendFloating(target, static_cast<float>(value.asFloat64()));
            else
                runtime::AppendFloating(target, value.asFloat64());
            return true;
        case Value::Kind::Array:
            if (layout.kind != TypeArray || layout.arguments.size() != 1)
                return false;
            target.push_back('[');
            for (std::size_t index = 0; index < value.elementCount(); ++index)
            {
                if (index)
                    target.append(", ");
                const auto* item = value.element(index);
                if (!item || !append(module, layout.arguments[0], *item, target, depth + 1))
                    return false;
            }

            target.push_back(']');
            return true;
        case Value::Kind::Dictionary:
            if (layout.kind != TypeDictionary || layout.arguments.size() != 2)
                return false;
            target.push_back('{');
            for (std::size_t index = 0; index < value.dictionaryCount(); ++index)
            {
                if (index)
                    target.append(", ");
                const auto* key = value.dictionaryKey(index);
                const auto* item = value.dictionaryValue(index);
                if (!key || !item || !append(module, layout.arguments[0], *key, target, depth + 1))
                    return false;
                target.append(": ");
                if (!append(module, layout.arguments[1], *item, target, depth + 1))
                    return false;
            }

            target.push_back('}');
            return true;
        default:
            return false;
        }
    }
} // namespace wio::vm::detail
